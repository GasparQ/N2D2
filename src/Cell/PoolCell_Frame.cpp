/*
    (C) Copyright 2014 CEA LIST. All Rights Reserved.
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

#include "GradientCheck.hpp"
#include "Cell/PoolCell_Frame.hpp"
#include "DeepNet.hpp"
#include "third_party/half.hpp"

template <>
N2D2::Registrar<N2D2::PoolCell>
N2D2::PoolCell_Frame<half_float::half>::mRegistrar("Frame",
    N2D2::PoolCell_Frame<half_float::half>::create,
    N2D2::Registrar<N2D2::PoolCell>::Type<half_float::half>());

template <>
N2D2::Registrar<N2D2::PoolCell>
N2D2::PoolCell_Frame<float>::mRegistrar("Frame",
    N2D2::PoolCell_Frame<float>::create,
    N2D2::Registrar<N2D2::PoolCell>::Type<float>());

template <>
N2D2::Registrar<N2D2::PoolCell>
N2D2::PoolCell_Frame<double>::mRegistrar("Frame",
    N2D2::PoolCell_Frame<double>::create,
    N2D2::Registrar<N2D2::PoolCell>::Type<double>());


template <class T>
N2D2::PoolCell_Frame<T>::PoolCell_Frame(const DeepNet& deepNet, 
    const std::string& name,
    const std::vector<unsigned int>& poolDims,
    unsigned int nbOutputs,
    const std::vector<unsigned int>& strideDims,
    const std::vector<unsigned int>& paddingDims,
    Pooling pooling,
    const std::shared_ptr<Activation>& activation)
    : Cell(deepNet, name, nbOutputs),
      PoolCell(deepNet, name,
               poolDims,
               nbOutputs,
               strideDims,
               paddingDims,
               pooling),
      Cell_Frame<T>(deepNet, name, nbOutputs, activation),
      mPoolDesc(poolDims.size(),
                &poolDims[0],
                &strideDims[0],
                &paddingDims[0])
{
    // ctor
    assert(poolDims.size() <= POOL_KERNEL_MAX_DIMS);

    if (poolDims.size() != 2) {
        throw std::domain_error("PoolCell_Frame: only 2D pooling is"
                                " supported");
    }

    if (strideDims.size() != poolDims.size()) {
        throw std::domain_error("PoolCell_Frame: the number of dimensions"
                                " of stride must match the number of"
                                " dimensions of the pooling.");
    }

    if (paddingDims.size() != poolDims.size()) {
        throw std::domain_error("PoolCell_Frame: the number of dimensions"
                                " of padding must match the number of"
                                " dimensions of the pooling.");
    }
}

template <class T>
void N2D2::PoolCell_Frame<T>::initialize()
{
    for (unsigned int k = 0, size = mInputs.size(); k < size; ++k) {
        if (mInputs[k].size() == 0)
            throw std::runtime_error("Zero-sized input for PoolCell " + mName);

        if (mArgMax.size() == k) {
            mArgMax.push_back(new Tensor<PoolCell_Frame_Kernels::ArgMax>
                              (mOutputs.dims()));
        }
    }
}

template <class T>
void N2D2::PoolCell_Frame<T>::propagate(bool inference)
{
    mInputs.synchronizeDBasedToH();

    const T alpha = T(1.0);
    T beta = T(0.0);

    unsigned int offset = 0;

    for (unsigned int k = 0, size = mInputs.size(); k < size; ++k) {
        if (k > 0)
            beta = T(1.0);

        const Tensor<T>& input = tensor_cast<T>(mInputs[k]);

        if (mPooling == Max) {
            PoolCell_Frame_Kernels::forwardMax<T>(&alpha,
                                                    input,
                                                    mPoolDesc,
                                                    &beta,
                                                    mOutputs,
                                                    mArgMax[k],
                                                    false,
                                                    mMapping.rows(offset,
                                                                mInputs[k].dimZ()));
        }
        else {
            PoolCell_Frame_Kernels::forwardAverage<T>(&alpha,
                                                   input,
                                                   mPoolDesc,
                                                   &beta,
                                                   mOutputs,
                                                   true,
                                                   mMapping.rows(offset,
                                                        mInputs[k].dimZ()));
        }

        offset += mInputs[k].dimZ();
    }

    Cell_Frame<T>::propagate(inference);
    mDiffInputs.clearValid();
}

template <class T>
void N2D2::PoolCell_Frame<T>::backPropagate()
{
    if (mDiffOutputs.empty())
        return;

    Cell_Frame<T>::backPropagate();

    const T alpha = T(1.0);

    unsigned int offset = 0;

    for (unsigned int k = 0, size = mInputs.size(); k < size; ++k) {
        const T beta = (mDiffOutputs[k].isValid()) ? T(1.0) : T(0.0);

        Tensor<T> diffOutput = (mDiffOutputs[k].isValid())
            ? tensor_cast<T>(mDiffOutputs[k])
            : tensor_cast_nocopy<T>(mDiffOutputs[k]);

        if (mPooling == Max) {
            PoolCell_Frame_Kernels::backwardMax<T>(&alpha,
                                                mDiffInputs,
                                                mPoolDesc,
                                                &beta,
                                                diffOutput,
                                                mArgMax[k],
                                                mMapping.rows(offset,
                                                           mInputs[k].dimZ()));
        }
        else {
            PoolCell_Frame_Kernels::backwardAverage<T>(&alpha,
                                                    mDiffInputs,
                                                    mPoolDesc,
                                                    &beta,
                                                    diffOutput,
                                                    true,
                                                    mMapping.rows(offset,
                                                          mInputs[k].dimZ()));
        }

        offset += mInputs[k].dimZ();

        mDiffOutputs[k] = diffOutput;
        mDiffOutputs[k].setValid();
    }

    mDiffOutputs.synchronizeHToD();
}
template <class T>
void N2D2::PoolCell_Frame<T>::update()
{
}

template <class T>
void N2D2::PoolCell_Frame<T>::checkGradient(double epsilon, double maxError)
{
    GradientCheck<T> gc(epsilon, maxError);
    gc.initialize(mInputs,
                  mOutputs,
                  mDiffInputs,
                  std::bind(&PoolCell_Frame::propagate, this, false),
                  std::bind(&PoolCell_Frame::backPropagate, this),
                  (mPooling == Max));

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
N2D2::PoolCell_Frame<T>::~PoolCell_Frame()
{
    for (unsigned int k = 0, size = mArgMax.size(); k < size; ++k)
        delete &mArgMax[k];
}

namespace N2D2 {
    template class PoolCell_Frame<half_float::half>;
    template class PoolCell_Frame<float>;
    template class PoolCell_Frame<double>;
}
