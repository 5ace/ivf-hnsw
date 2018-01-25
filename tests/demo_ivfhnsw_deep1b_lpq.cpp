#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <queue>
#include <unordered_set>

#include <ivf-hnsw/IndexIVF_HNSW_LPQ.h>
#include <ivf-hnsw/Parser.h>

using namespace hnswlib;
using namespace ivfhnsw;

//========================
// Run IVF-HNSW on DEEP1B 
//========================
int main(int argc, char **argv)
{
    //===============
    // Parse Options 
    //===============
    Parser opt = Parser(argc, argv);

    //==================
    // Load Groundtruth 
    //==================
    std::cout << "Loading groundtruth from " << opt.path_gt << std::endl;
    std::vector<idx_t> massQA(opt.nq * opt.ngt);
    std::ifstream gt_input(opt.path_gt, ios::binary);
    readXvec<idx_t>(gt_input, massQA.data(), opt.ngt, opt.nq);
    gt_input.close();

    //==============
    // Load Queries 
    //==============
    std::cout << "Loading queries from " << opt.path_q << std::endl;
    std::vector<float> massQ(opt.nq * opt.d);
    std::ifstream query_input(opt.path_q, ios::binary);
    readXvec<float>(query_input, massQ.data(), opt.d, opt.nq);
    query_input.close();

    //==================
    // Initialize Index 
    //==================
    IndexIVF_HNSW *index = new IndexIVF_HNSW(opt.d, opt.nc, opt.code_size, 8);
    index->build_quantizer(opt.path_centroids, opt.path_info, opt.path_edges, opt.M, opt.efConstruction);


    {
        std::vector<float> avdists(opt.nc);
        StopW stopw = StopW();

        int batch_size = 1000000;
        int nbatches = opt.nb / batch_size;
        int groups_per_iter = 100000;

        std::vector<float> batch(batch_size * opt.d);
        std::vector<idx_t> idx_batch(batch_size);

        // Adding batches of groups to the index (batch size - <groups_per_iter> groups per iteration)
        for (int ngroups_added = 0; ngroups_added < opt.nc; ngroups_added += groups_per_iter) {
            std::cout << "[" << stopw.getElapsedTimeMicro() / 1000000 << "s] "
                      << ngroups_added << " / " << opt.nc << std::endl;

            std::vector <std::vector<float>> data(groups_per_iter);

            std::ifstream base_input(opt.path_base, ios::binary);
            std::ifstream idx_input(opt.path_precomputed_idxs, ios::binary);

            for (int b = 0; b < nbatches; b++) {
                readXvec<float>(base_input, batch.data(), opt.d, batch_size);
                readXvec<idx_t>(idx_input, idx_batch.data(), batch_size, 1);

                for (int i = 0; i < batch_size; i++) {
                    if (idx_batch[i] < ngroups_added ||
                        idx_batch[i] >= ngroups_added + groups_per_iter)
                        continue;

                    idx_t idx = idx_batch[i] % groups_per_iter;
                    for (int j = 0; j < opt.d; j++)
                        data[idx].push_back(batch[i * opt.d + j]);
                }
            }
            base_input.close();
            idx_input.close();

            // If <opt.nc> is not a multiple of groups_per_iter, change <groups_per_iter> on the last iteration
            if (opt.nc - ngroups_added <= groups_per_iter)
                groups_per_iter = opt.nc - ngroups_added;

            for (int i = 0; i < groups_per_iter; i++) {
                int group_size = data[i].size() / opt.d;

                //Compute average distance in the group
                float av_group_dist = 0.0;
                for (int j = 0; j < group_size; j++){
                    const float *centroid = index->quantizer->getDataByInternalId(i);
                    av_group_dist += fvec_L2sqr(centroid, data[i].data()+j*opt.d, opt.d);
                }
                av_group_dist /= group_size;
                avdists[ngroups_added+i] = av_group_dist;
            }

        }
        // Cluster av dists
        FILE *fout = fopen("group_dists.fvecs", "wb");
        for (int i = 0; i < opt.nc; i++){
            int d = 1;
            fwrite(&d, sizeof(int), 1, fout);
            fwrite(avdists.data()+i*d, sizeof(float), d, fout);
        }
        fclose(fout);
    }
    exit(0);
    //==========
    // Train PQ 
    //==========
    if (exists(opt.path_pq) && exists(opt.path_norm_pq)) {
        std::cout << "Loading Residual PQ codebook from " << opt.path_pq << std::endl;
        index->pq = faiss::read_ProductQuantizer(opt.path_pq);

        std::cout << "Loading Norm PQ codebook from " << opt.path_norm_pq << std::endl;
        index->norm_pq = faiss::read_ProductQuantizer(opt.path_norm_pq);
    }
    else {
        // Load learn set
        std::ifstream learn_input(opt.path_learn, ios::binary);
        std::vector<float> trainvecs(opt.nt * opt.d);
        readXvec<float>(learn_input, trainvecs.data(), opt.d, opt.nt);
        learn_input.close();

        // Set Random Subset of sub_nt trainvecs
        std::vector<float> trainvecs_rnd_subset(opt.nsubt * opt.d);
        random_subset(trainvecs.data(), trainvecs_rnd_subset.data(), opt.d, opt.nt, opt.nsubt);
        index->train_pq(opt.nsubt, trainvecs_rnd_subset.data());

        std::cout << "Saving Residual PQ codebook to " << opt.path_pq << std::endl;
        faiss::write_ProductQuantizer(index->pq, opt.path_pq);

        std::cout << "Saving Norm PQ codebook to " << opt.path_norm_pq << std::endl;
        faiss::write_ProductQuantizer(index->norm_pq, opt.path_norm_pq);
    }

    //==========================
    // Construct IVF-HNSW Index 
    //==========================
    if (exists(opt.path_index)){
        // Load Index 
        std::cout << "Loading index from " << opt.path_index << std::endl;
        index->read(opt.path_index);
    } else {
        // Add elements 
        StopW stopw = StopW();

        std::ifstream base_input(opt.path_base, ios::binary);
        std::ifstream idx_input(opt.path_precomputed_idxs, ios::binary);

        size_t batch_size = 1000000;
        size_t nbatch = opt.nb / batch_size;
        std::vector<float> batch(batch_size * opt.d);
        std::vector <idx_t> idx_batch(batch_size);
        std::vector <idx_t> ids_batch(batch_size);

        for (int b = 0; b < nbatch; b++) {
            if (b % 10 == 0) {
                std::cout << "[" << stopw.getElapsedTimeMicro() / 1000000 << "s] " << (100. * b) / nbatch << "%\n";
            }
            readXvec<idx_t>(idx_input, idx_batch.data(), batch_size, 1);
            readXvecFvec<float>(base_input, batch.data(), opt.d, batch_size);

            for (size_t i = 0; i < batch_size; i++)
                ids_batch[i] = batch_size * b + i;

            index->add_batch(batch_size, batch.data(), ids_batch.data(), idx_batch.data());
        }

        idx_input.close();
        base_input.close();

        // Computing Centroid Norms
        std::cout << "Computing centroid norms"<< std::endl;
        index->compute_centroid_norms();

        // Save index, pq and norm_pq 
        std::cout << "Saving index to " << opt.path_index << std::endl;
        index->write(opt.path_index);
    }
    // For correct search using OPQ encoding rotate points in the coarse quantizer
    if (opt.do_opq) {
        std::cout << "Rotating centroids"<< std::endl;
        index->rotate_quantizer();
    }
    //===================
    // Parse groundtruth
    //===================
    std::cout << "Parsing groundtruth" << std::endl;
    std::vector<std::priority_queue< std::pair<float, idx_t >>> answers;
    (std::vector<std::priority_queue< std::pair<float, idx_t >>>(opt.nq)).swap(answers);
    for (int i = 0; i < opt.nq; i++)
        answers[i].emplace(0.0f, massQA[opt.ngt*i]);

    //=======================
    // Set search parameters
    //=======================
    index->nprobe = opt.nprobe;
    index->max_codes = opt.max_codes;
    index->quantizer->efSearch = opt.efSearch;

    //========
    // Search 
    //========
    int correct = 0;
    float distances[opt.k];
    long labels[opt.k];

    StopW stopw = StopW();
    for (int i = 0; i < opt.nq; i++) {
        index->search(opt.k, massQ.data() + i*opt.d, distances, labels);

        std::priority_queue<std::pair<float, idx_t >> gt(answers[i]);
        std::unordered_set<idx_t> g;

        while (gt.size()) {
            g.insert(gt.top().second);
            gt.pop();
        }

        for (int j = 0; j < opt.k; j++)
            if (g.count(labels[j]) != 0) {
                correct++;
                break;
            }
    }
    //===================
    // Represent results 
    //===================
    float time_us_per_query = stopw.getElapsedTimeMicro() / opt.nq;
    std::cout << "Recall@" << opt.k << ": " << 1.0f * correct / opt.nq << std::endl;
    std::cout << "Time per query: " << time_us_per_query << " us" << std::endl;

    delete index;
    return 0;
}