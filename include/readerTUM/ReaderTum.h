//
// Copyright (c) Leonid Seniukov. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.
//


#ifndef GDR_READERTUM_H
#define GDR_READERTUM_H

#include <vector>
#include "poseInfo.h"

namespace gdr {
    class ReaderTUM {
    public:
        static std::vector<poseInfo> getPoseInfoTimeTranslationOrientation(const std::string &pathToGroundTruthFile);
    };
}

#endif