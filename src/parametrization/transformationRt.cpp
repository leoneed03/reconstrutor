//
// Copyright (c) Leonid Seniukov. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#include "parametrization/transformationRt.h"


namespace gdr {
//
//    transformationRtMatrix::transformationRtMatrix(const Eigen::Matrix4d &newInnerEssentialMatrix,
//                                                   const VertexCG &newVertexFrom,
//                                                   const VertexCG &newVertexTo)
//            : innerTranformationRtMatrix(newInnerEssentialMatrix),
//              vertexFrom(newVertexFrom),
//              vertexTo(newVertexTo) {
//
//        auto dim = newInnerEssentialMatrix.cols() - 1;
//        R = newInnerEssentialMatrix.block(0, 0, dim, dim);
//        t = newInnerEssentialMatrix.block(0, dim, dim, 1);
//        relativePoseSE3d = Sophus::SE3d::fitToSE3(newInnerEssentialMatrix);
//    }

    int transformationRtMatrix::getIndexTo() const {
        return vertexTo.getIndex();
    }

    int transformationRtMatrix::getIndexFrom() const {
        return vertexFrom.getIndex();
    }

    Eigen::Quaterniond transformationRtMatrix::getRelativeRotation() const {
        return relativePose.getRotationQuatd();
    }

    Eigen::Vector3d transformationRtMatrix::getRelativeTranslation() const {
        return relativePose.getTranslation();
//        return innerTranformationRtMatrix.block<3, 1>(0, 3);
    }

//    void transformationRtMatrix::setRotation(const Eigen::Quaterniond &newRelativeRotation) {
//        relativePose.setRotation(Sophus::SO3d(newRelativeRotation.normalized()));
//    }

//    transformationRtMatrix::transformationRtMatrix(const Sophus::SE3d &newRelativePose,
//                                                   const VertexCG &newVertexFrom,
//                                                   const VertexCG &newVertexTo) : vertexFrom(newVertexFrom),
//                                                                                  vertexTo(newVertexTo) {
//        relativePoseSE3d = newRelativePose;
//        innerTranformationRtMatrix = newRelativePose.matrix();
//        auto dim = innerTranformationRtMatrix.cols() - 1;
//        R = innerTranformationRtMatrix.block(0, 0, dim, dim);
//        t = innerTranformationRtMatrix.block(0, dim, dim, 1);
//
//    }

    const Sophus::SE3d transformationRtMatrix::getRelativePoseSE3() const {
        return relativePose.getSE3();
    }

    transformationRtMatrix::transformationRtMatrix(const SE3 &se3,
                                                   const VertexCG &newVertexFrom,
                                                   const VertexCG &newVertexTo):
                                                   vertexFrom(newVertexFrom),
                                                   vertexTo(newVertexTo),
                                                   relativePose(se3) {}

    const SE3& transformationRtMatrix::getRelativePose() const {
        return relativePose;
    }
}
