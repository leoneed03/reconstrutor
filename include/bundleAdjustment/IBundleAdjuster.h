//
// Copyright (c) Leonid Seniukov. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#ifndef GDR_IBUNDLEADJUSTER_H
#define GDR_IBUNDLEADJUSTER_H

#include <parametrization/SE3.h>

namespace gdr {

    class IBundleAdjuster {
    public:
        virtual std::vector<SE3> optimizePointsAndPosesUsingDepthInfo(int fixedPoseNumber) = 0;

        virtual ~IBundleAdjuster() = default;
    };
}

#endif
