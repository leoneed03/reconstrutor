//
// Copyright (c) Leonid Seniukov. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#include "KeyPointsDepthDescriptor.h"

#include <cassert>

namespace gdr {


    keyPointsDepthDescriptor::keyPointsDepthDescriptor(const std::vector<SiftGPU::SiftKeypoint> &newKeypointsKnownDepth,
                                                       const std::vector<float> &newDescriptorsKnownDepth,
                                                       const std::vector<double> &newDepths) :
            keypointsKnownDepth(newKeypointsKnownDepth),
            descriptorsKnownDepth(newDescriptorsKnownDepth),
            depths(newDepths) {

        assert(depths.size() == keypointsKnownDepth.size());
        assert(depths.size() * 128 == descriptorsKnownDepth.size());
    }

    const std::vector<SiftGPU::SiftKeypoint> &keyPointsDepthDescriptor::getKeyPointsKnownDepth() const {
        return keypointsKnownDepth;
    }

    const std::vector<float> &keyPointsDepthDescriptor::getDescriptorsKnownDepth() const {
        return descriptorsKnownDepth;
    }

    const std::vector<double> &keyPointsDepthDescriptor::getDepths() const {
        return depths;
    }
}