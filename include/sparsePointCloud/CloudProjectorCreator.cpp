//
// Copyright (c) Leonid Seniukov. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#include "sparsePointCloud/CloudProjector.h"
#include "CloudProjectorCreator.h"


namespace gdr {

    std::unique_ptr<ICloudProjector>
    gdr::CloudProjectorCreator::getRefiner(const std::vector<ProjectableInfo> &cameraPoses) {

        return std::make_unique<CloudProjector>(cameraPoses);
    }

}