//
// Copyright (c) Leonid Seniukov. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#include "rotationMeasurement.h"

namespace gdr {
    rotationMeasurement::rotationMeasurement(const Eigen::Quaterniond &newRelativeRotationQuat,
                                             int newIndexFrom,
                                             int newIndexTo) : relativeRotationQuat(newRelativeRotationQuat.normalized()),
                                                               indexFrom(newIndexFrom),
                                                               indexTo(newIndexTo) {}
    Eigen::Quaterniond rotationMeasurement::getRotationQuat() const {
        return relativeRotationQuat;
    }

    int rotationMeasurement::getIndexFromToBeTransformed() const {
        return indexFrom;
    }
    int rotationMeasurement::getIndexToDestination() const {
        return indexTo;
    }
}