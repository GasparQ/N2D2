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

#ifndef N2D2_FCCELL_SPIKE_ANALOG_H
#define N2D2_FCCELL_SPIKE_ANALOG_H

#include "DeepNet.hpp"
#include "FcCell_Spike.hpp"
#include "Synapse_Behavioral.hpp"

namespace N2D2 {
class FcCell_Spike_Analog : public FcCell_Spike {
public:
    FcCell_Spike_Analog(Network& net, const DeepNet& deepNet, 
                        const std::string& name,
                        unsigned int nbOutputs);
    static std::shared_ptr<FcCell> create(Network& net, const DeepNet& deepNet, 
                                          const std::string& name,
                                          unsigned int nbOutputs,
                                          const std::shared_ptr
                                          <Activation>& /*activation*/
                                          = std::shared_ptr
                                          <Activation>())
    {
        return std::make_shared<FcCell_Spike_Analog>(net, deepNet, name, nbOutputs);
    }

    virtual void initialize();
    void propagateSpike(NodeIn* node, Time_T timestamp, EventType_T type = 0);
    void incomingSpike(NodeIn* node, Time_T timestamp, EventType_T type = 0);
    void notify(Time_T timestamp, NotifyType notify);
    virtual ~FcCell_Spike_Analog() {};

private:
    Synapse* newSynapse() const;
    void increaseWeight(Synapse_Behavioral* synapse) const;
    void decreaseWeight(Synapse_Behavioral* synapse) const;

    // Parameters
    /// Mean minimum synaptic weight \f$w_{min}\f$
    ParameterWithSpread<Weight_T> mWeightsMinMean;
    /// Mean maximum synaptic weight \f$w_{max}\f$
    ParameterWithSpread<Weight_T> mWeightsMaxMean;
    /// Synaptic weight increment for this neuron \f$\alpha{}_{+}\f$
    ParameterWithSpread<Weight_T> mWeightIncrement;
    /// Synaptic weight increment damping factor for this neuron
    /// \f$\beta{}_{+}\f$
    ParameterWithSpread<double> mWeightIncrementDamping;
    /// Synaptic weight decrement for this neuron \f$\alpha{}_{-}\f$
    ParameterWithSpread<Weight_T> mWeightDecrement;
    /// Synaptic weight decrement damping factor for this neuron
    /// \f$\beta{}_{-}\f$
    ParameterWithSpread<double> mWeightDecrementDamping;
    Parameter<bool> mBipolarIntegration;
    /// STDP LTP time window \f$T_{LTP}\f$
    Parameter<Time_T> mStdpLtp;
    /// Neural lateral inhibition period \f$T_{inhibit}\f$
    Parameter<Time_T> mInhibitRefractory;
    /// If false, STDP is disabled (no synaptic weight change)
    Parameter<bool> mEnableStdp;
    Parameter<bool> mRefractoryIntegration;

    Tensor<Time_T> mInputsActivationTime;
    Tensor<int> mInputsActivity;

private:
    static Registrar<FcCell> mRegistrar;
};
}

#endif // N2D2_FCCELL_SPIKE_ANALOG_H
