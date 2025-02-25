/*
    (C) Copyright 2016 CEA LIST. All Rights Reserved.
    Contributor(s): Olivier BICHLER (olivier.bichler@cea.fr)

    This software is governed by the CeCILL-C license under French law and
    abiding by the rules of distribution of free software.  You can  use,
    modify and/ or redistribute the software under the terms of the CeCILL-C
    license as circulated by CEA, CNRS and INRIA at the following URL
    "http://www.cecill.info".

    As a counterpart to the access to the source code and  rights to copy,
    modify and redistribute granted by the license, users are provided only
    with a limited warranty  and the software's author,  the holder of the
    economic rights,  and the successive licensors  have only  limited
    liability.

    The fact that you are presently reading this means that you have had
    knowledge of the CeCILL-C license and that you accept its terms.
*/

#ifdef CUDA
#include <cudnn.h>
#if CUDNN_VERSION >= 4000

#include "GradientCheck.hpp"
#include "DeepNet.hpp"
#include "Cell/BatchNormCell_Frame_CUDA.hpp"
#include "third_party/half.hpp"

template <>
N2D2::Registrar<N2D2::BatchNormCell>
N2D2::BatchNormCell_Frame_CUDA<half_float::half>::mRegistrar("Frame_CUDA",
    N2D2::BatchNormCell_Frame_CUDA<half_float::half>::create,
    N2D2::Registrar<N2D2::BatchNormCell>::Type<half_float::half>());

template <>
N2D2::Registrar<N2D2::BatchNormCell>
N2D2::BatchNormCell_Frame_CUDA<float>::mRegistrar("Frame_CUDA",
    N2D2::BatchNormCell_Frame_CUDA<float>::create,
    N2D2::Registrar<N2D2::BatchNormCell>::Type<float>());

template <>
N2D2::Registrar<N2D2::BatchNormCell>
N2D2::BatchNormCell_Frame_CUDA<double>::mRegistrar("Frame_CUDA",
    N2D2::BatchNormCell_Frame_CUDA<double>::create,
    N2D2::Registrar<N2D2::BatchNormCell>::Type<double>());

template <class T>
N2D2::BatchNormCell_Frame_CUDA<T>::BatchNormCell_Frame_CUDA(
    const DeepNet& deepNet,
    const std::string& name,
    unsigned int nbOutputs,
    const std::shared_ptr<Activation>& activation)
    : Cell(deepNet, name, nbOutputs),
      BatchNormCell(deepNet, name, nbOutputs),
      Cell_Frame_CUDA<T>(deepNet, name, nbOutputs, activation),
      mScale(std::make_shared<CudaTensor<ParamT> >()),
      mBias(std::make_shared<CudaTensor<ParamT> >()),
      mMean(std::make_shared<CudaTensor<ParamT> >()),
      mVariance(std::make_shared<CudaTensor<ParamT> >()),
      mSynchronized(false)
{
    // ctor
    mScaleSolver = std::make_shared<SGDSolver_Frame_CUDA<ParamT> >();
    mBiasSolver = std::make_shared<SGDSolver_Frame_CUDA<ParamT> >();
}

template <class T>
void N2D2::BatchNormCell_Frame_CUDA<T>::initialize()
{
    if (mInputs.size() > 1)
        throw std::domain_error("BatchNormCell_Frame_CUDA<T>::initialize(): "
                                "inputs concatenation is not supported.");

    mMode = CUDNN_BATCHNORM_SPATIAL;
    mNbPropagate = 0;

    // CUDNN_BN_MIN_EPSILON is set to 0.0 since cuDNN 7.5.0
    if (CUDNN_BN_MIN_EPSILON > 0.0 && mEpsilon < CUDNN_BN_MIN_EPSILON) {
        mEpsilon = CUDNN_BN_MIN_EPSILON;
    }

    cudnnTensorDescriptor_t derivedBnDesc;
    CHECK_CUDNN_STATUS(cudnnCreateTensorDescriptor(&derivedBnDesc));
    CHECK_CUDNN_STATUS(cudnnDeriveBNTensorDescriptor(
        derivedBnDesc, mInputs[0].getCudnnTensorDesc(), mMode));

    cudnnDataType_t dataType;
    const unsigned int nbDimsRequested = 5;
    std::vector<int> dims(nbDimsRequested);
    std::vector<int> strides(nbDimsRequested);
    int nbDims;

    CHECK_CUDNN_STATUS(cudnnGetTensorNdDescriptor(derivedBnDesc,
                                                  nbDimsRequested,
                                                  &dataType,
                                                  &nbDims,
                                                  &dims[0],
                                                  &strides[0]));

    dims.resize(nbDims);
    strides.resize(nbDims);

    CHECK_CUDNN_STATUS(cudnnDestroyTensorDescriptor(derivedBnDesc));

    const std::vector<size_t> requiredDims(dims.rbegin(), dims.rend());

    if (mScale->empty())
        mScale->resize(requiredDims, ParamT(1.0));
    else {
        if (mScale->dims() != requiredDims) {
            std::stringstream msgStr;
            msgStr << "BatchNormCell_Frame_CUDA<T>::initialize():"
                " in cell " + mName + ", wrong size for shared scale, expected"
                " size is " << requiredDims << " whereas actual size is "
                << mScale->dims() << std::endl;

            throw std::runtime_error(msgStr.str());
        }
    }

    if (mBias->empty())
        mBias->resize(requiredDims, ParamT(0.0));
    else {
        if (mBias->dims() != requiredDims) {
            std::stringstream msgStr;
            msgStr << "BatchNormCell_Frame_CUDA<T>::initialize():"
                " in cell " + mName + ", wrong size for shared bias, expected"
                " size is " << requiredDims << " whereas actual size is "
                << mBias->dims() << std::endl;

            throw std::runtime_error(msgStr.str());
        }
    }

    if (mMean->empty())
        mMean->resize(requiredDims, ParamT(0.0));
    else {
        if (mMean->dims() != requiredDims) {
            std::stringstream msgStr;
            msgStr << "BatchNormCell_Frame_CUDA<T>::initialize():"
                " in cell " + mName + ", wrong size for shared mean, expected"
                " size is " << requiredDims << " whereas actual size is "
                << mMean->dims() << std::endl;

            throw std::runtime_error(msgStr.str());
        }
    }

    if (mVariance->empty())
        mVariance->resize(requiredDims, ParamT(0.0));
    else {
        if (mVariance->dims() != requiredDims) {
            std::stringstream msgStr;
            msgStr << "BatchNormCell_Frame_CUDA<T>::initialize():"
                " in cell " + mName + ", wrong size for shared variance, expected"
                " size is " << requiredDims << " whereas actual size is "
                << mVariance->dims() << std::endl;

            throw std::runtime_error(msgStr.str());
        }
    }

    if(mMovingAverageMomentum <= 0.0 || mMovingAverageMomentum >= 1.0)
    {
        std::stringstream msgStr;
        msgStr << "BatchNormCell_Frame_CUDA<T>::initialize():"
            " in cell " + mName + ", wrong value for MovingAverageMomentum. "
            "Expected value range ]0.0, 1.0[ whereas actual value is "
            << mMovingAverageMomentum << std::endl;

        throw std::runtime_error(msgStr.str());

    }

    mSavedMean.resize(requiredDims, ParamT(0.0));
    mSavedVariance.resize(requiredDims, ParamT(0.0));

    mDiffScale.resize(requiredDims, ParamT(0.0));
    mDiffBias.resize(requiredDims, ParamT(0.0));
}

template <class T>
void N2D2::BatchNormCell_Frame_CUDA<T>::propagate(bool inference)
{
    mInputs.synchronizeHBasedToD();

    const typename Cuda::cudnn_scaling_type<T>::type alpha = 1.0f;
    const typename Cuda::cudnn_scaling_type<T>::type beta = 0.0f;

    std::shared_ptr<CudaDeviceTensor<T> > input0
        = cuda_device_tensor_cast<T>(mInputs[0]);

    if (inference) {
        CHECK_CUDNN_STATUS(cudnnBatchNormalizationForwardInference(
            CudaContext::cudnnHandle(),
            mMode,
            &alpha,
            &beta,
            input0->getCudnnTensorDesc(),
            input0->getDevicePtr(),
            mOutputs.getCudnnTensorDesc(),
            mOutputs.getDevicePtr(),
            mScale->getCudnnTensorDesc(),
            mScale->getDevicePtr(),
            mBias->getDevicePtr(),
            mMean->getDevicePtr(),
            mVariance->getDevicePtr(),
            mEpsilon));
    } else {
        // mSavedMean and mSavedVariance cache parameters 
        // must be reinitialized to 0.0 at each forward pass on training:
        mSavedMean.fill(ParamT(0.0));
        mSavedVariance.fill(ParamT(0.0));

        CHECK_CUDNN_STATUS(cudnnBatchNormalizationForwardTraining(
            CudaContext::cudnnHandle(),
            mMode,
            &alpha,
            &beta,
            input0->getCudnnTensorDesc(),
            input0->getDevicePtr(),
            mOutputs.getCudnnTensorDesc(),
            mOutputs.getDevicePtr(),
            mScale->getCudnnTensorDesc(),
            mScale->getDevicePtr(),
            mBias->getDevicePtr(),
            mMovingAverageMomentum,
            mMean->getDevicePtr(),
            mVariance->getDevicePtr(),
            mEpsilon,
            mSavedMean.getDevicePtr(),
            mSavedVariance.getDevicePtr()));
    }

    if (!inference)
        ++mNbPropagate;

    Cell_Frame_CUDA<T>::propagate(inference);
    mDiffInputs.clearValid();
}

template <class T>
void N2D2::BatchNormCell_Frame_CUDA<T>::backPropagate()
{
    Cell_Frame_CUDA<T>::backPropagate();

    const typename Cuda::cudnn_scaling_type<T>::type alpha = 1.0f;
    const typename Cuda::cudnn_scaling_type<T>::type alphaData = 1.0f;
    assert(mScaleSolver->isNewIteration() == mBiasSolver->isNewIteration());
    const typename Cuda::cudnn_scaling_type<T>::type beta
        = (mScaleSolver->isNewIteration()) ? 0.0f : 1.0f;
    const typename Cuda::cudnn_scaling_type<T>::type betaData
        = (mDiffOutputs[0].isValid()) ? 1.0f : 0.0f;

    std::shared_ptr<CudaDeviceTensor<T> > input0
        = cuda_device_tensor_cast_nocopy<T>(mInputs[0]);
    std::shared_ptr<CudaDeviceTensor<T> > diffOutput0
        = (mDiffOutputs[0].isValid())
            ? cuda_device_tensor_cast<T>(mDiffOutputs[0])
            : cuda_device_tensor_cast_nocopy<T>(mDiffOutputs[0]);

    CHECK_CUDNN_STATUS(
        cudnnBatchNormalizationBackward(CudaContext::cudnnHandle(),
                                        mMode,
                                        &alphaData,
                                        &betaData,
                                        &alpha,
                                        &beta,
                                        input0->getCudnnTensorDesc(),
                                        input0->getDevicePtr(),
                                        mOutputs.getCudnnTensorDesc(),
                                        mDiffInputs.getDevicePtr(),
                                        diffOutput0->getCudnnTensorDesc(),
                                        diffOutput0->getDevicePtr(),
                                        mScale->getCudnnTensorDesc(),
                                        mScale->getDevicePtr(),
                                        mDiffScale.getDevicePtr(),
                                        mDiffBias.getDevicePtr(),
                                        mEpsilon,
                                        mSavedMean.getDevicePtr(),
                                        mSavedVariance.getDevicePtr()));

    mDiffOutputs[0].deviceTensor() = *diffOutput0;
    mDiffOutputs[0].setValid();
    mDiffOutputs.synchronizeDToHBased();
}

template <class T>
void N2D2::BatchNormCell_Frame_CUDA<T>::update()
{
    mScaleSolver->update(*mScale, mDiffScale, mInputs.dimB());
    mBiasSolver->update(*mBias, mDiffBias, mInputs.dimB());
}

template <class T>
void N2D2::BatchNormCell_Frame_CUDA<T>::setScales(
    const std::shared_ptr<BaseTensor>& scales)
{
    std::shared_ptr<CudaTensor<ParamT> > cudaScales
        = std::dynamic_pointer_cast<CudaTensor<ParamT> >(scales);

    if (!cudaScales) {
        throw std::runtime_error("BatchNormCell_Frame_CUDA<T>::setBiases(): scales"
                                 " must be a CudaTensor");
    }

    mScale = cudaScales;
}

template <class T>
void N2D2::BatchNormCell_Frame_CUDA<T>::setBiases(
    const std::shared_ptr<BaseTensor>& biases)
{
    std::shared_ptr<CudaTensor<ParamT> > cudaBiases
        = std::dynamic_pointer_cast<CudaTensor<ParamT> >(biases);

    if (!cudaBiases) {
        throw std::runtime_error("BatchNormCell_Frame_CUDA<T>::setBiases(): biases"
                                 " must be a CudaTensor");
    }

    mBias = cudaBiases;
}

template <class T>
void N2D2::BatchNormCell_Frame_CUDA<T>::setMeans(
    const std::shared_ptr<BaseTensor>& means)
{
    std::shared_ptr<CudaTensor<ParamT> > cudaMeans
        = std::dynamic_pointer_cast<CudaTensor<ParamT> >(means);

    if (!cudaMeans) {
        throw std::runtime_error("BatchNormCell_Frame_CUDA<T>::setBiases(): means"
                                 " must be a CudaTensor");
    }

    mMean = cudaMeans;
}

template <class T>
void N2D2::BatchNormCell_Frame_CUDA<T>::setVariances(
    const std::shared_ptr<BaseTensor>& variances)
{
    std::shared_ptr<CudaTensor<ParamT> > cudaVariances
        = std::dynamic_pointer_cast<CudaTensor<ParamT> >(variances);

    if (!cudaVariances) {
        throw std::runtime_error("BatchNormCell_Frame_CUDA<T>::setBiases():"
                                 " variances must be a CudaTensor");
    }

    mVariance = cudaVariances;
}

template <class T>
void N2D2::BatchNormCell_Frame_CUDA<T>::checkGradient(double epsilon,
                                                   double maxError)
{
    GradientCheck<T> gc(epsilon, maxError);
    gc.initialize(mInputs,
                  mOutputs,
                  mDiffInputs,
                  std::bind(&BatchNormCell_Frame_CUDA<T>::propagate, this, false),
                  std::bind(&BatchNormCell_Frame_CUDA<T>::backPropagate, this));
    gc.check(mName + "_mDiffScale", (*mScale), mDiffScale);
    gc.check(mName + "_mDiffBias", (*mBias), mDiffBias);

    if (!mDiffOutputs.empty()) {
        for (unsigned int in = 0; in < mInputs.size(); ++in) {
            std::stringstream name;
            name << mName + "_mDiffOutputs[" << in << "]";

            gc.check(name.str(), mInputs[in], mDiffOutputs[in]);
        }
    } else {
        std::cout << Utils::cwarning << "Empty diff. outputs for cell " << mName
                  << ", could not check the gradient!" << Utils::cdef
                  << std::endl;
    }
}

template <class T>
void N2D2::BatchNormCell_Frame_CUDA<T>::saveFreeParameters(const std::string
                                                        & fileName) const
{
    std::ofstream syn(fileName.c_str(), std::fstream::binary);

    if (!syn.good())
        throw std::runtime_error("Could not create parameter file (.SYN): "
                                 + fileName);

    mScale->synchronizeDToH();
    mScale->save(syn);
    mBias->synchronizeDToH();
    mBias->save(syn);
    mMean->synchronizeDToH();
    mMean->save(syn);
    mVariance->synchronizeDToH();
    mVariance->save(syn);

    if (!syn.good())
        throw std::runtime_error("Error writing parameter file: " + fileName);
}

template <class T>
void N2D2::BatchNormCell_Frame_CUDA<T>::loadFreeParameters(const std::string
                                                        & fileName,
                                                        bool ignoreNotExists)
{
    std::ifstream syn(fileName.c_str(), std::fstream::binary);

    if (!syn.good()) {
        if (ignoreNotExists) {
            std::cout << Utils::cnotice
                      << "Notice: Could not open parameter file (.SYN): "
                      << fileName << Utils::cdef << std::endl;
            return;
        } else
            throw std::runtime_error("Could not open parameter file (.SYN): "
                                     + fileName);
    }

    mScale->load(syn);
    mScale->synchronizeHToD();
    mBias->load(syn);
    mBias->synchronizeHToD();
    mMean->load(syn);
    mMean->synchronizeHToD();
    mVariance->load(syn);
    mVariance->synchronizeHToD();

    if (syn.eof())
        throw std::runtime_error(
            "End-of-file reached prematurely in parameter file (.SYN): "
            + fileName);
    else if (!syn.good())
        throw std::runtime_error("Error while reading parameter file (.SYN): "
                                 + fileName);
    else if (syn.get() != std::fstream::traits_type::eof())
        throw std::runtime_error(
            "Synaptic file (.SYN) size larger than expected: " + fileName);
}

template <class T>
void N2D2::BatchNormCell_Frame_CUDA<T>::exportFreeParameters(const std::string
                                                          & fileName) const
{
    mScale->synchronizeDToH();
    mBias->synchronizeDToH();
    mMean->synchronizeDToH();
    mVariance->synchronizeDToH();

    mSynchronized = true;
    BatchNormCell::exportFreeParameters(fileName);
    mSynchronized = false;
}

template <class T>
void N2D2::BatchNormCell_Frame_CUDA<T>::importFreeParameters(const std::string
                                                          & fileName,
                                                          bool ignoreNotExists)
{
    mSynchronized = true;
    BatchNormCell::importFreeParameters(fileName, ignoreNotExists);
    mSynchronized = false;

    mScale->synchronizeHToD();
    mBias->synchronizeHToD();
    mMean->synchronizeHToD();
    mVariance->synchronizeHToD();
}

template <class T>
N2D2::BatchNormCell_Frame_CUDA<T>::~BatchNormCell_Frame_CUDA()
{
}

namespace N2D2 {
    template class BatchNormCell_Frame_CUDA<half_float::half>;
    template class BatchNormCell_Frame_CUDA<float>;
    template class BatchNormCell_Frame_CUDA<double>;
}

#endif
#endif
