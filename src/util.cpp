//
// Copyright (c) Leonid Seniukov. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#include "../include/util.h"

MatrixX getSomeMatrix(int height, int width) {
    return MatrixX::Random(height, width);
}

Eigen::Matrix3d getRotationMatrixDouble(const MatrixX &m) {
    Eigen::Matrix3d resMatrix;

    assert(m.cols() == 3);
    assert(m.rows() == 3);

    resMatrix = m;
    return resMatrix;

}