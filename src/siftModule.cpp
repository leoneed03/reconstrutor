//
// Copyright (c) Leonid Seniukov. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#include "siftModule.h"
#include "printer.h"

void gdr::SiftModule::siftParseParams(std::vector<char *> &siftGpuArgs) {
    sift.ParseParam(siftGpuArgs.size(), siftGpuArgs.data());
}

gdr::SiftModule::SiftModule() {

    std::string siftGpuFarg = std::to_string(SIFTGPU_ARG_V);
    std::vector<std::string> siftGpuArgsStrings = {"-cuda", "-fo", "-1", "-v", siftGpuFarg};
    std::vector<char *> siftGpuArgs;
    for (auto& stringArg: siftGpuArgsStrings) {
        siftGpuArgs.push_back(stringArg.data());
    }
//    std::vector<char *> siftGpuArgs = {"-cuda", "-fo", "-1", "-v", siftGpuFarg};
    matcher = std::unique_ptr<SiftMatchGPU>(new SiftMatchGPU(maxSift));
    PRINT_PROGRESS("Parse params for sift default");
    siftParseParams(siftGpuArgs);
    matcher->VerifyContextGL();
}
