
#pragma once
#ifdef _MSC_VER
#include <intrin.h>
#include <stdexcept>

#define  __builtin_popcount(t) __popcnt(t)
#else
#include <x86intrin.h>
#endif
#define USE_AVX
#if defined(__GNUC__)
#define PORTABLE_ALIGN32 __attribute__((aligned(32)))
#else
#define PORTABLE_ALIGN32 __declspec(align(32))
#endif

#include <cmath>
#include "hnswlib.h"
#include <faiss/utils.h>
#include <faiss/ProductQuantizer.h>
#include <faiss/index_io.h>
#include "hnswIndexPQ.h"

namespace hnswlib {
	using namespace std;
    /**= Prev fstdistfunc =**/
                    static float
                    L2SqrSIMD16Ext(const void *pVect1v, const void *pVect2v, const void *qty_ptr)
                {
                    float *pVect1 = (float *)pVect1v;
                    float *pVect2 = (float *)pVect2v;
                    size_t qty = *((size_t *)qty_ptr);
                    float PORTABLE_ALIGN32 TmpRes[8];
            #ifdef USE_AVX
                    size_t qty16 = qty >> 4;

                    const float *pEnd1 = pVect1 + (qty16 << 4);

                    __m256 diff, v1, v2;
                    __m256 sum = _mm256_set1_ps(0);

                    while (pVect1 < pEnd1) {
                        v1 = _mm256_loadu_ps(pVect1);
                        pVect1 += 8;
                        v2 = _mm256_loadu_ps(pVect2);
                        pVect2 += 8;
                        diff = _mm256_sub_ps(v1, v2);
                        sum = _mm256_add_ps(sum, _mm256_mul_ps(diff, diff));

                        v1 = _mm256_loadu_ps(pVect1);
                        pVect1 += 8;
                        v2 = _mm256_loadu_ps(pVect2);
                        pVect2 += 8;
                        diff = _mm256_sub_ps(v1, v2);
                        sum = _mm256_add_ps(sum, _mm256_mul_ps(diff, diff));
                    }

                    _mm256_store_ps(TmpRes, sum);
                    float res = TmpRes[0] + TmpRes[1] + TmpRes[2] + TmpRes[3] + TmpRes[4] + TmpRes[5] + TmpRes[6] + TmpRes[7];

                    return (res);
            #else
                    // size_t qty4 = qty >> 2;
                    size_t qty16 = qty >> 4;

                    const float *pEnd1 = pVect1 + (qty16 << 4);
                    // const float* pEnd2 = pVect1 + (qty4 << 2);
                    // const float* pEnd3 = pVect1 + qty;

                    __m128 diff, v1, v2;
                    __m128 sum = _mm_set1_ps(0);

                    while (pVect1 < pEnd1) {
                        //_mm_prefetch((char*)(pVect2 + 16), _MM_HINT_T0);
                        v1 = _mm_loadu_ps(pVect1);
                        pVect1 += 4;
                        v2 = _mm_loadu_ps(pVect2);
                        pVect2 += 4;
                        diff = _mm_sub_ps(v1, v2);
                        sum = _mm_add_ps(sum, _mm_mul_ps(diff, diff));

                        v1 = _mm_loadu_ps(pVect1);
                        pVect1 += 4;
                        v2 = _mm_loadu_ps(pVect2);
                        pVect2 += 4;
                        diff = _mm_sub_ps(v1, v2);
                        sum = _mm_add_ps(sum, _mm_mul_ps(diff, diff));

                        v1 = _mm_loadu_ps(pVect1);
                        pVect1 += 4;
                        v2 = _mm_loadu_ps(pVect2);
                        pVect2 += 4;
                        diff = _mm_sub_ps(v1, v2);
                        sum = _mm_add_ps(sum, _mm_mul_ps(diff, diff));

                        v1 = _mm_loadu_ps(pVect1);
                        pVect1 += 4;
                        v2 = _mm_loadu_ps(pVect2);
                        pVect2 += 4;
                        diff = _mm_sub_ps(v1, v2);
                        sum = _mm_add_ps(sum, _mm_mul_ps(diff, diff));
                    }
                    _mm_store_ps(TmpRes, sum);
                    float res = TmpRes[0] + TmpRes[1] + TmpRes[2] + TmpRes[3];

                    return (res);
            #endif
                };
                static float PORTABLE_ALIGN32 TmpRes[8];
                static float
                    L2SqrSIMD4Ext(const void *pVect1v, const void *pVect2v, const void *qty_ptr)
                {
                    float *pVect1 = (float *)pVect1v;
                    float *pVect2 = (float *)pVect2v;
                    size_t qty = *((size_t *)qty_ptr);


                    // size_t qty4 = qty >> 2;
                    size_t qty16 = qty >> 2;

                    const float *pEnd1 = pVect1 + (qty16 << 2);

                    __m128 diff, v1, v2;
                    __m128 sum = _mm_set1_ps(0);

                    while (pVect1 < pEnd1) {
                        v1 = _mm_loadu_ps(pVect1);
                        pVect1 += 4;
                        v2 = _mm_loadu_ps(pVect2);
                        pVect2 += 4;
                        diff = _mm_sub_ps(v1, v2);
                        sum = _mm_add_ps(sum, _mm_mul_ps(diff, diff));
                    }
                    _mm_store_ps(TmpRes, sum);
                    float res = TmpRes[0] + TmpRes[1] + TmpRes[2] + TmpRes[3];

                    return (res);
                };
	/**====================**/

    enum class L2SpaceType { Int, Float, PQ };

	class L2Space : public SpaceInterface<float>
    {
		size_t data_size_;
		size_t dim_;
	public:
		L2Space(size_t dim) {
			dim_ = dim;
			data_size_ = dim * sizeof(float);
		}

		size_t get_data_size() { return data_size_; }
        size_t get_data_dim() { return dim_; }

        float fstdistfunc(const void *x, const void *y) {
            float *pVect1 = (float *) x;
            float *pVect2 = (float *) y;
            float PORTABLE_ALIGN32 TmpRes[8];
            #ifdef USE_AVX
            size_t qty16 = dim_ >> 4;

            const float *pEnd1 = pVect1 + (qty16 << 4);

            __m256 diff, v1, v2;
            __m256 sum = _mm256_set1_ps(0);

            while (pVect1 < pEnd1) {
                v1 = _mm256_loadu_ps(pVect1);
                pVect1 += 8;
                v2 = _mm256_loadu_ps(pVect2);
                pVect2 += 8;
                diff = _mm256_sub_ps(v1, v2);
                sum = _mm256_add_ps(sum, _mm256_mul_ps(diff, diff));

                v1 = _mm256_loadu_ps(pVect1);
                pVect1 += 8;
                v2 = _mm256_loadu_ps(pVect2);
                pVect2 += 8;
                diff = _mm256_sub_ps(v1, v2);
                sum = _mm256_add_ps(sum, _mm256_mul_ps(diff, diff));
            }

            _mm256_store_ps(TmpRes, sum);
            float res = TmpRes[0] + TmpRes[1] + TmpRes[2] + TmpRes[3] + TmpRes[4] + TmpRes[5] + TmpRes[6] + TmpRes[7];

            return (res);
            #else
            size_t qty16 = dim_ >> 4;

            const float *pEnd1 = pVect1 + (qty16 << 4);

            __m128 diff, v1, v2;
            __m128 sum = _mm_set1_ps(0);

            while (pVect1 < pEnd1) {
                v1 = _mm_loadu_ps(pVect1);
                pVect1 += 4;
                v2 = _mm_loadu_ps(pVect2);
                pVect2 += 4;
                diff = _mm_sub_ps(v1, v2);
                sum = _mm_add_ps(sum, _mm_mul_ps(diff, diff));

                v1 = _mm_loadu_ps(pVect1);
                pVect1 += 4;
                v2 = _mm_loadu_ps(pVect2);
                pVect2 += 4;
                diff = _mm_sub_ps(v1, v2);
                sum = _mm_add_ps(sum, _mm_mul_ps(diff, diff));

                v1 = _mm_loadu_ps(pVect1);
                pVect1 += 4;
                v2 = _mm_loadu_ps(pVect2);
                pVect2 += 4;
                diff = _mm_sub_ps(v1, v2);
                sum = _mm_add_ps(sum, _mm_mul_ps(diff, diff));

                v1 = _mm_loadu_ps(pVect1);
                pVect1 += 4;
                v2 = _mm_loadu_ps(pVect2);
                pVect2 += 4;
                diff = _mm_sub_ps(v1, v2);
                sum = _mm_add_ps(sum, _mm_mul_ps(diff, diff));
            }
            _mm_store_ps(TmpRes, sum);
            float res = TmpRes[0] + TmpRes[1] + TmpRes[2] + TmpRes[3];

            return (res);
            #endif
        }

//        float fstdistfunc(const void *x, const void *y) {
//            return faiss::fvec_L2sqr ((float *) x, (float *) y, dim_);
//        }

        float fstdistfuncST(const void *y_code) { return 0.0; }
	};


	class L2SpaceI : public SpaceInterface<int>
    {
		size_t data_size_;
		size_t dim_;
	public:
		L2SpaceI(size_t dim) {
			dim_ = dim;
			data_size_ = dim * sizeof(unsigned char);
		}

		size_t get_data_size() { return data_size_; }
        size_t get_data_dim() { return dim_; }

        int fstdistfunc(const void *x, const void *y)
        {
            size_t dim = dim_ >> 2;
            int res = 0;
            unsigned char *a = (unsigned char *)x;
            unsigned char *b = (unsigned char *)y;

            for (int i = 0; i < dim; i++) {
                res += ((*a) - (*b))*((*a) - (*b)); a++; b++;
                res += ((*a) - (*b))*((*a) - (*b)); a++; b++;
                res += ((*a) - (*b))*((*a) - (*b)); a++; b++;
                res += ((*a) - (*b))*((*a) - (*b)); a++; b++;
            }
            return res;
        }
        int fstdistfuncST(const void *y_code) { return 0; }
	};

    class L2SpacePQ: public SpaceInterface<int>
    {
        size_t data_size_;
        size_t dim_;
        size_t m_;
        size_t k_;
        size_t vocab_dim_;

        std::vector<float *> codebooks;
        std::vector<int *> constructionTables;
        int *query_table;
    public:
        L2SpacePQ(const size_t dim, const size_t m, const size_t k):
                dim_(dim), m_(m), k_(k)
        {
            vocab_dim_ = (dim_ % m_ == 0) ? dim_ / m_ : -1;
            data_size_ = m_ * sizeof(unsigned char);

            if (vocab_dim_ == -1) {
                std::cerr << "M is not multiply of D" << std::endl;
                exit(1);
            }
            query_table = new int [m_*k_];

        }

        virtual ~L2SpacePQ()
        {
            for (int i = 0; i < constructionTables.size(); i++)
                free(constructionTables[i]);

            delete query_table;

            for (int i = 0; i < codebooks.size(); i++)
                free(codebooks[i]);
        }

        void set_query_table(unsigned char *q)
        {
            unsigned char *x;
            float *y;

            for (size_t m = 0; m < m_; m++) {
                x = q + m * vocab_dim_;
                for (size_t k = 0; k < k_; k++){
                    int res = 0;
                    y = codebooks[m] + k * vocab_dim_;
                    for (int j = 0; j < vocab_dim_; j++) {
                        int t = x[j] - y[j];
                        res += t * t;
                    }
                    query_table[k_*m + k] = res;
                }
            }
        }

        void set_codebooks(const char *codebooksFilename)
        {
            codebooks = std::vector<float *>(m_);
            for (int i = 0; i < m_; i++)
                codebooks[i] = (float *) calloc(sizeof(float), vocab_dim_ * k_);

            FILE *fin = fopen(codebooksFilename, "rb");
            for (int i = 0; i < m_; i++) {
                for (int j = 0; j < k_; j++) {
                    fread((int *) &vocab_dim_, sizeof(int), 1, fin);
                    if (vocab_dim_ != dim_ / m_) {
                        std::cerr << "Wrong codebook dim" << std::endl;
                        exit(1);
                    }
                    fread((float *) (codebooks[i] + vocab_dim_ * j), sizeof(float), vocab_dim_, fin);
                }
            }
            fclose(fin);
        }

        void set_construction_tables(const char *tablesFilename) {
            constructionTables = std::vector<int *>(m_);
            float massf[k_*k_];

            FILE *fin = fopen(tablesFilename, "rb");
            for (int i = 0; i < m_; i++) {
                constructionTables[i] = (int *) calloc(sizeof(int), k_ * k_);
                fread((float *) massf, sizeof(float), k_ * k_, fin);
                for (int j =0; j < k_*k_; j++) {
                    constructionTables[i][j] = round(massf[j]);
                }
            }
            fclose(fin);
        }


        size_t get_data_size() { return data_size_; }
        size_t get_data_dim() { return m_; }

        int fstdistfunc(const void *x_code, const void *y_code)
        {
            int res = 0;
            int dim = m_ >> 3;
            unsigned char *x = (unsigned char *)x_code;
            unsigned char *y = (unsigned char *)y_code;

            int n = 0;
            for (int i = 0; i < dim; ++i) {
                res += constructionTables[n][k_*x[n] + y[n]]; ++n;
                res += constructionTables[n][k_*x[n] + y[n]]; ++n;
                res += constructionTables[n][k_*x[n] + y[n]]; ++n;
                res += constructionTables[n][k_*x[n] + y[n]]; ++n;
                res += constructionTables[n][k_*x[n] + y[n]]; ++n;
                res += constructionTables[n][k_*x[n] + y[n]]; ++n;
                res += constructionTables[n][k_*x[n] + y[n]]; ++n;
                res += constructionTables[n][k_*x[n] + y[n]]; ++n;
            }
            return res;
        };

        int fstdistfuncST(const void *y_code)
        {
            int res = 0;
            int dim = m_ >> 3;
            unsigned char *y = (unsigned char *)y_code;

            int m = 0;
            for (int i = 0; i < dim; ++i) {
                res += query_table[k_ * m + y[m]]; ++m;
                res += query_table[k_ * m + y[m]]; ++m;
                res += query_table[k_ * m + y[m]]; ++m;
                res += query_table[k_ * m + y[m]]; ++m;
                res += query_table[k_ * m + y[m]]; ++m;
                res += query_table[k_ * m + y[m]]; ++m;
                res += query_table[k_ * m + y[m]]; ++m;
                res += query_table[k_ * m + y[m]]; ++m;
            }
            return res;
        };
    };

//    class NewL2SpacePQ: public SpaceInterface<float>
//    {
//        size_t data_size_;
//        size_t dim_;
//        size_t m_;
//        size_t k_;
//        size_t vocab_dim_;
//
//        float *query_table;
//        faiss::ProductQuantizer *pq;
//
//    public:
//        NewL2SpacePQ(const size_t dim, const size_t m, const size_t k,
//                  const char *path_pq, const char *path_learn):
//                dim_(dim), m_(m), k_(k)
//        {
//            vocab_dim_ = (dim_ % m_ == 0) ? dim_ / m_ : -1;
//            data_size_ = m_ * sizeof(float);
//
//            if (vocab_dim_ == -1) {
//                std::cerr << "M is not multiply of D" << std::endl;
//                exit(1);
//            }
//
//            query_table = new float [m_*k_];
//            pq = new faiss::ProductQuantizer(dim_, m_, 8);
//
//            if (exists_test(path_pq))
//                read_pq(path_pq, pq);
//            else {
//                std::ifstream learn_input(path_learn, ios::binary);
//                int nt = 65536;
//                std::vector<uint8_t> trainvecs(nt * vecdim);
//
//                readXvec<uint8_t>(learn_input, trainvecs.data(), vecdim, nt);
//                learn_input.close();
//
//                pq->verbose = true;
//                pq->train(nt, trainvecs.data());
//                write_pq(path_pq, pq);
//            }
//            pq->compute_sdc_table();
//        }
//
//        virtual ~NewL2SpacePQ()
//        {
//            delete construction_table;
//            delete query_table;
//            delete pq;
//        }
//
//        void set_query_table(const float *q) {
//            pq->compute_distance_table(q, query_table);
//        }
//
//
//        size_t get_data_size() { return data_size_; }
//        size_t get_data_dim() { return m_; }
//
//        float fstdistfunc(const void *x_code, const void *y_code)
//        {
//            float res = 0;
//            int dim = m_ >> 3;
//            unsigned char *x = (unsigned char *)x_code;
//            unsigned char *y = (unsigned char *)y_code;
//
//            int m = 0;
//            float *table = pq->sdc_table.data();
//            for (int i = 0; i < dim; ++i) {
//                res += table[k_ * (m * k_ + x[m]) + y[m]]; ++m;
//                res += table[k_ * (m * k_ + x[m]) + y[m]]; ++m;
//                res += table[k_ * (m * k_ + x[m]) + y[m]]; ++m;
//                res += table[k_ * (m * k_ + x[m]) + y[m]]; ++m;
//                res += table[k_ * (m * k_ + x[m]) + y[m]]; ++m;
//                res += table[k_ * (m * k_ + x[m]) + y[m]]; ++m;
//                res += table[k_ * (m * k_ + x[m]) + y[m]]; ++m;
//                res += table[k_ * (m * k_ + x[m]) + y[m]]; ++m;
//            }
//            return res;
//        };
//
//        float fstdistfuncST(const void *y_code)
//        {
//            float res = 0.;
//            int dim = m_ >> 3;
//            unsigned char *y = (unsigned char *)y_code;
//
//            int m = 0;
//            for (int i = 0; i < dim; ++i) {
//                res += query_table[k_ * m + y[m]]; ++m;
//                res += query_table[k_ * m + y[m]]; ++m;
//                res += query_table[k_ * m + y[m]]; ++m;
//                res += query_table[k_ * m + y[m]]; ++m;
//                res += query_table[k_ * m + y[m]]; ++m;
//                res += query_table[k_ * m + y[m]]; ++m;
//                res += query_table[k_ * m + y[m]]; ++m;
//                res += query_table[k_ * m + y[m]]; ++m;
//            }
//            return res;
//        };
//    };
//
//
    class NewL2SpaceIPQ: public SpaceInterface<int>
    {
        size_t data_size_;
        size_t dim_;
        size_t m_;
        size_t k_;
        size_t vocab_dim_;

        float *query_table;
        faiss::ProductQuantizer *pq;

    public:
        NewL2SpaceIPQ(const size_t dim, const size_t m, const size_t k,
                     const char *path_pq, const char *path_learn):
                dim_(dim), m_(m), k_(k)
        {
            vocab_dim_ = (dim_ % m_ == 0) ? dim_ / m_ : -1;
            data_size_ = m_ * sizeof(unsigned char);

            if (vocab_dim_ == -1) {
                std::cerr << "M is not multiply of D" << std::endl;
                exit(1);
            }
            query_table = new float [m_*k_];

            pq = new faiss::ProductQuantizer(dim_, m_, 8);
            if (exists_test(path_pq))
                read_pq(path_pq, pq);
            else {
                std::ifstream learn_input(path_learn, ios::binary);
                int nt = 65536;
                std::vector<unsigned char> trainvecs(nt * dim_);

                readXvec<unsigned char>(learn_input, trainvecs.data(), dim_, nt);
                learn_input.close();

                float *trainvecs_float = new float[nt * dim_];
                for (int i = 0; i < nt * dim_; i++)
                    trainvecs_float[i] = trainvecs[i];
                
                pq->verbose = true;
                pq->train(nt, trainvecs_float.data());
                write_pq(path_pq, pq);

                delete trainvecs_float;
            }
            pq->compute_sdc_table();
        }

        virtual ~NewL2SpaceIPQ()
        {
            delete query_table;
            delete pq;
        }

        void set_query_table(const uint8_t *q) {
            float float_q = *q;
            pq->compute_distance_table(&float_q, query_table);
        }


        size_t get_data_size() { return data_size_; }
        size_t get_data_dim() { return m_; }

        int fstdistfunc(const void *x_code, const void *y_code)
        {
            int res = 0;
            int dim = m_ >> 3;
            unsigned char *x = (unsigned char *)x_code;
            unsigned char *y = (unsigned char *)y_code;

            int m = 0;
            float *table = pq->sdc_table.data();
            for (int i = 0; i < dim; ++i) {
                res += table[k_ * (m * k_ + x[m]) + y[m]]; ++m;
                res += table[k_ * (m * k_ + x[m]) + y[m]]; ++m;
                res += table[k_ * (m * k_ + x[m]) + y[m]]; ++m;
                res += table[k_ * (m * k_ + x[m]) + y[m]]; ++m;
                res += table[k_ * (m * k_ + x[m]) + y[m]]; ++m;
                res += table[k_ * (m * k_ + x[m]) + y[m]]; ++m;
                res += table[k_ * (m * k_ + x[m]) + y[m]]; ++m;
                res += table[k_ * (m * k_ + x[m]) + y[m]]; ++m;
            }
            return res;
        };

        int fstdistfuncST(const void *y_code)
        {
            int res = 0;
            int dim = m_ >> 3;
            unsigned char *y = (unsigned char *)y_code;

            int m = 0;
            for (int i = 0; i < dim; ++i) {
                res += query_table[k_ * m + y[m]]; ++m;
                res += query_table[k_ * m + y[m]]; ++m;
                res += query_table[k_ * m + y[m]]; ++m;
                res += query_table[k_ * m + y[m]]; ++m;
                res += query_table[k_ * m + y[m]]; ++m;
                res += query_table[k_ * m + y[m]]; ++m;
                res += query_table[k_ * m + y[m]]; ++m;
                res += query_table[k_ * m + y[m]]; ++m;
            }
            return res;
        };
    };
};