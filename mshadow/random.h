/*!
 *  Copyright (c) 2014 by Contributors
 *  \file random.h
 *  \brief Random inline functions for tensor.
 *  \author Bing Xu, Tianqi Chen
 *   Based on curand|MKL|stdlib
 */
#ifndef MSHADOW_RANDOM_H_
#define MSHADOW_RANDOM_H_
#include <cstdlib>
#include "./tensor.h"
#include "./tensor_container.h"

namespace mshadow {
/*! 
 * \brief random number generator 
 * \tparam Device the device of random number generator
 * \tparam DType the target data type of random number can be float for double
 */
template<typename Device, typename DType MSHADOW_DEFAULT_DTYPE>
class Random {};

/*! \brief CPU random number generator */
template<typename DType>
class Random<cpu, DType> {
 public:
  /*!
   * \brief constructor of random engine
   * \param seed random number seed
   */
  explicit Random(int seed) {
    this->Seed(seed);
    buffer_.Resize(Shape1(kRandBufferSize));
  }
  ~Random(void) {
#if MSHADOW_USE_MKL
    vslDeleteStream(&vStream_);
#endif
  }
  /*!
   * \brief seed random number generator using this seed
   * \param seed seed of prng
   */
  inline void Seed(int seed) {
#if MSHADOW_USE_MKL
    int status = vslNewStream(&vStream_, VSL_BRNG_MT19937, seed);
    utils::Check(status == VSL_STATUS_OK,
                 "MKL VSL Random engine failed to be initialized.\n");
#else
    this->rseed_ = static_cast<unsigned>(seed);
#endif
  }
  /*!
   * \brief set the stream of computation
   * \param stream computation stream
   */
  inline void set_stream(Stream<cpu> *stream) {    
  }
  /*!
   * \brief generate data from uniform [a,b)
   * \param dst destination
   * \param a lower bound of uniform
   * \param b upper bound of uniform
   * \tparam dim dimension of tensor
   */
  template<int dim>
  inline void SampleUniform(Tensor<cpu, dim, DType> *dst,
                            DType a = 0.0f, DType b = 1.0f) {
    Tensor<cpu, 2, DType> mat = dst->FlatTo2D();
    for (index_t i = 0; i < mat.size(0); ++i) {
      this->GenUniform(mat[i].dptr_, mat.size(1), a, b);
    }
  }
  /*!
   * \brief generate data from standard gaussian
   * \param dst destination
   * \param mu mean variable
   * \param sigma standard deviation
   * \tparam dim dimension of tensor
   */
  template<int dim>
  inline void SampleGaussian(Tensor<cpu, dim, DType> *dst,
                             DType mu = 0.0f, DType sigma = 1.0f) {
    if (sigma <= 0.0f) {
      *dst = mu; return;
    }
    Tensor<cpu, 2, DType> mat = dst->FlatTo2D();
    for (index_t i = 0; i < mat.size(0); ++i) {
      this->GenGaussian(mat[i].dptr_, mat.size(1), mu, sigma);
    }
  }
  /*!
   * \brief return a temporal expression storing standard gaussian random variables
   *        the temporal tensor is only valid before next call of gaussian or uniform
   *        can be used as part of expression
   *  Caution: this means expression such as A = gaussian(s1) * gaussian(s2) will give invalid result,
   *           since second call of gaussian(s2) makes gaussian(s1) invalid
   *           A = gaussian(s1)*B+C; is correct; use one gaussian/uniform in each expression
   * \param shape shape of the tensor
   * \tparam dim dimension of tensor
   */
  template<int dim>
  inline expr::ReshapeExp<Tensor<cpu, 1, DType>, DType, dim, 1>
  gaussian(Shape<dim> shape) {
    buffer_.Resize(Shape1(shape.Size()));
    this->SampleGaussian(&buffer_, 0.0f, 1.0f);
    return expr::reshape(buffer_, shape);
  }
  /*!
   * \brief return a temporal expression storing standard uniform [0,1)
   *        the temporal tensor is only valid before next call of gaussian or uniform
   *        can be used as part of expression
   *  Caution: this means expression such as A = uniform(s1) * uniform(s2) will give invalid result,
   *           since second call of gaussian(s2) makes gaussian(s1) invalid
   *           A = gaussian(s1)*B+C; is correct; use one gaussian/uniform in each expression
   * \param shape shape of the tensor
   * \tparam dim dimension of tensor
   */
  template<int dim>
  inline expr::ReshapeExp<Tensor<cpu, 1, DType>, DType, dim, 1>
  uniform(Shape<dim> shape) {
    buffer_.Resize(Shape1(shape.Size()));
    this->SampleUniform(&buffer_, 0.0f, 1.0f);
    return expr::reshape(buffer_, shape);
  }

 private:
#if MSHADOW_USE_MKL
  /*! \brief stream used by MKL VSL */
  VSLStreamStatePtr vStream_;
  // generate uniform distribution
  inline void GenUniform(float *dptr, index_t size, float a, float b) {
    int status = vsRngUniform(0, vStream_, size, dptr, a, b);
    utils::Check(status == VSL_STATUS_OK,
                 "Failed to generate random number by MKL.");
  }
  inline void GenUniform(double *dptr, index_t size, double a, double b) {
    int status = vdRngUniform(0, vStream_, size, dptr, a, b);
    utils::Check(status == VSL_STATUS_OK,
                 "Failed to generate random number by MKL.");
  }
  inline void GenGaussian(float *dptr, index_t size, float mu, float sigma) {
    int status = vsRngGaussian(0, vStream_, size, dptr, mu, sigma);
    utils::Check(status == VSL_STATUS_OK,
                 "Failed to generate random number by MKL.");
  }
  inline void GenGaussian(double *dptr, index_t size, double mu, double sigma) {
    int status = vdRngGaussian(0, vStream_, size, dptr, mu, sigma);
    utils::Check(status == VSL_STATUS_OK,
                 "Failed to generate random number by MKL.");
  }
#else
  /*! \brief random number seed used by PRNG*/
  unsigned rseed_;
  // functions
  inline void GenUniform(float *dptr, index_t size, float a, float b) {
    for (index_t j = 0; j < size; ++j) {
      dptr[j] = static_cast<float>(RandNext()) * (b - a) + a;
    }
  }
  inline void GenUniform(double *dptr, index_t size, double a, double b) {
    for (index_t j = 0; j < size; ++j) {
      dptr[j] = static_cast<double>(RandNext()) * (b - a) + a;
    }
  }
  inline void GenGaussian(float *dptr, index_t size, float mu, float sigma) {
    this->GenGaussianX(dptr, size, mu, sigma);
  }
  inline void GenGaussian(double *dptr, index_t size, double mu, double sigma) {
    this->GenGaussianX(dptr, size, mu, sigma);
  }
  inline void GenGaussianX(DType *dptr, index_t size, DType mu, DType sigma) {
    DType g1 = 0.0f, g2 = 0.0f;
    for (index_t j = 0; j < size; ++j) {
      if ((j & 1) == 0) {
        this->SampleNormal2D(&g1, &g2);
        dptr[j] = mu + g1 * sigma;
      } else {
        dptr[j] = mu + g2 * sigma;
      }
    }
  }
  /*! \brief get next random number from rand */
  inline DType RandNext(void) {
    return static_cast<DType>(rand_r(&rseed_)) /
        (static_cast<DType>(RAND_MAX) + 1.0f);
  }
  /*! \brief return a real numer uniform in (0,1) */
  inline DType RandNext2(void) {
    return (static_cast<DType>(rand_r(&rseed_)) + 1.0f) /
        (static_cast<DType>(RAND_MAX) + 2.0f);
  }
  /*!
   * \brief sample iid xx,yy ~N(0,1)
   * \param xx first  gaussian output
   * \param yy second gaussian output
   */
  inline void SampleNormal2D(DType *xx_, DType *yy_) {
    DType &xx = *xx_, &yy = *yy_;
    DType x, y, s;
    do {
      x = 2.0f * RandNext2() - 1.0f;
      y = 2.0f * RandNext2() - 1.0f;
      s = x * x + y * y;
    } while (s >= 1.0f || s == 0.0f);
    DType t = std::sqrt(-2.0f * std::log(s) / s);
    xx = x * t; yy = y * t;
  }
#endif
  /*! \brief temporal space used to store random numbers */
  TensorContainer<cpu, 1, DType> buffer_;
};  // class Random<cpu, DType>
// only allow GPU PRNG in CUDACC
#ifdef __CUDACC__
/*! \brief GPU random number generator */
template<typename DType>
class Random<gpu, DType> {
 public:
  /*!
   * \brief constructor of random engine
   * \param seed random number seed
   */
  Random(int seed) {
    curandStatus_t status;
    status = curandCreateGenerator(&gen_, CURAND_RNG_PSEUDO_DEFAULT);
    utils::Check(status == CURAND_STATUS_SUCCESS,
                 "Can not create CURAND Generator");
    this->Seed(seed);
    buffer_.Resize(Shape1(kRandBufferSize));
  }

  ~Random(void) {
    curandStatus_t status;
    status = curandDestroyGenerator(gen_);
    utils::Check(status == CURAND_STATUS_SUCCESS,
                 "Destory CURAND Gen failed");
  }
  /*!
   * \brief set the stream of computation
   * \param stream computation stream
   */
  inline void set_stream(Stream<gpu> *stream) {
    curandStatus_t status;
    status = curandset_stream(gen_, Stream<gpu>::GetStream(stream));
    utils::Check(status == CURAND_STATUS_SUCCESS,
                 "set_stream CURAND failed");
  }
  /*!
   * \brief seed random number generator using this seed
   * \param seed seed of prng
   */
  inline void Seed(int seed) {
    curandStatus_t status;
    status = curandSetPseudoRandomGeneratorSeed(gen_, seed);
    utils::Check(status == CURAND_STATUS_SUCCESS,
                 "Set CURAND seed failed.");
  }
  /*!
   * \brief generate data from uniform [a,b)
   * \param dst destination
   * \param a lower bound of uniform
   * \param b upper bound of uniform
   * \tparam dim dimension of tensor
   */
  template<int dim>
  inline void SampleUniform(Tensor<gpu, dim, DType> *dst,
                            DType a = 0.0f, DType b = 1.0f) {
    if (a == 0.0f && b == 1.0f) {
      *dst = this->uniform(dst->shape_);
    } else {
      *dst = this->uniform(dst->shape_) * (b - a) + a;
    }
  }
  /*!
   * \brief generate data from standard gaussian
   * \param dst destination
   * \param mu mean variable
   * \param sigma standard deviation
   * \tparam dim dimension of tensor
   */
  template<int dim>
  inline void SampleGaussian(Tensor<gpu, dim, DType> *dst,
                             DType mu = 0.0f, DType sigma = 1.0f) {
    *dst = this->gaussian(dst->shape_, mu, sigma);
  }
  /*!
   * \brief return a temporal expression storing standard gaussian random variables
   *        the temporal tensor is only valid before next call of gaussian or uniform
   *        can be used as part of expression
   *  Caution: this means expression such as A = gaussian(s1) * gaussian(s2) will give invalid result,
   *           since second call of gaussian(s2) makes gaussian(s1) invalid
   *           A = gaussian(s1)*B+C; is correct; use one gaussian/uniform in each expression
   * \param shape shape of the tensor
   * \param mu mean
   * \param sigma variance
   * \tparam dim dimension of tensor
   */
  template<int dim>
  inline expr::ReshapeExp<Tensor<gpu, 1, DType>, DType, dim, 1>
  gaussian(Shape<dim> shape, DType mu = 0.0f, DType sigma = 1.0f) {
    size_t aligned_sz = ((shape.Size() + 1UL) >> 1) << 1;
    // allocate alligned size
    buffer_.Resize(Shape1(aligned_sz));
    buffer_.Resize(Shape1(shape.Size()));
    this->GenGaussian(buffer_.dptr_, aligned_sz, mu, sigma);
    return expr::reshape(buffer_, shape);
  }
  /*!
   * \brief return a temporal expression storing standard uniform [0,1)
   *        the temporal tensor is only valid before next call of gaussian or uniform
   *        can be used as part of expression
   *  Caution: this means expression such as A = gaussian(s1) * gaussian(s2) will give invalid result,
   *           since second call of gaussian(s2) makes gaussian(s1) invalid
   *           A = gaussian(s1)*B+C; is correct; use one gaussian/uniform in each expression
   * \param shape shape of the tensor
   * \tparam dim dimension of tensor
   */
  template<int dim>
  inline expr::ReshapeExp<Tensor<gpu, 1, DType>, DType, dim, 1>
  uniform(Shape<dim> shape) {
    buffer_.Resize(Shape1(shape.Size()));
    this->GenUniform(buffer_.dptr_, buffer_.size(0));
    return expr::reshape(buffer_, shape);
  }

 private:
  inline void GenGaussian(float *dptr, size_t size, float mu, float sigma) {
    curandStatus_t status;
    status = curandGenerateNormal(gen_, dptr, size, mu, sigma);
    utils::Check(status == CURAND_STATUS_SUCCESS, "CURAND Gen Uniform failed");
  }
  inline void GenGaussian(double *dptr, size_t size, double mu, double sigma) {
    curandStatus_t status;
    status = curandGenerateNormalDouble(gen_, dptr, size, mu, sigma);
    utils::Check(status == CURAND_STATUS_SUCCESS, "CURAND Gen Uniform failed");
  }
  inline void GenUniform(float *dptr, size_t size) {
    curandStatus_t status;
    status = curandGenerateUniform(gen_, dptr, size);
    utils::Check(status == CURAND_STATUS_SUCCESS, "CURAND Gen Uniform failed");
  }
  inline void GenUniform(double *dptr, size_t size) {
    curandStatus_t status;
    status = curandGenerateUniformDouble(gen_, dptr, size);
    utils::Check(status == CURAND_STATUS_SUCCESS, "CURAND Gen Uniform failed");
  }
  /*! \brief random numbeer generator */
  curandGenerator_t gen_;
  /*! \brief templ buffer */
  TensorContainer<gpu, 1, DType> buffer_;
};  // class Random<gpu, DType>
#endif
}  // namespace mshadow
#endif  // MSHADOW_RANDOM_H_
