//
// Copyright (c) Leonid Seniukov. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#ifndef GDR_ICP_H
#define GDR_ICP_H

#include "IRefinerRelativePose.h"
#include "VertexCG.h"
#include <Eigen/Eigen>

namespace gdr {

    class ProcessorICP : public IRefinerRelativePose {

    public:
        bool refineRelativePose(const MatchableInfo &poseToBeTransformed,
                               const MatchableInfo &poseDestination,
                               SE3 &initTransformationSE3) override;

    private:
        static int refineRelativePoseICP(const VertexCG &poseToBeTransformed,
                                         const VertexCG &poseDestination,
                                         Eigen::Matrix4d &initRelPosEstimation);
    };
}

#endif
