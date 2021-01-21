//
// Copyright (c) Leonid Seniukov. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#include <iostream>
#include <gtest/gtest.h>
#include <vector>

#include "CorrespondenceGraph.h"
#include "groundTruthTransformer.h"
#include "rotationAveraging.h"
#include "quaternions.h"

TEST(testRotationAveraging, errorShouldBeZero) {

    std::string absolutePoses = "../../data/files/absolutePosesFirstPoseZero.txt";
    std::string relativeRotations = "pairWiseFirstPoseZero.txt";
    std::string absoluteRotations = "absoluteRotationsTestShanonAveraging.txt";
    gdr::GTT::extractAllRelativeTransformationPairwise(absolutePoses, relativeRotations,
                                                       "   10000.000000 0.000000 0.000000 0.000000 0.000000 0.000000   10000.000000 0.000000 0.000000 0.000000 0.000000   10000.000000 0.000000 0.000000 0.000000   10000.000000 0.000000 0.000000   10000.000000 0.000000   10000.000000");
    std::vector<Eigen::Quaterniond> absoluteRotationsQuat = gdr::rotationAverager::shanonAveraging(relativeRotations,
                                                                                                   absoluteRotations);
    std::cout << "finishing averaging" << std::endl;

    for (const auto &orientation: absoluteRotationsQuat) {
        std::cout << "\t" << orientation.x() << "\t" << orientation.y() << "\t" << orientation.z() << "\t"
                  << orientation.w() << "\t" << std::endl;
    }
    Eigen::Matrix3d id;
    id.setIdentity();
    Eigen::Quaterniond qid(id);

    std::cout << "\t_" << qid.x() << "\t" << qid.y() << "\t" << qid.z() << "\t" << qid.w() << "\t" << std::endl;

    std::vector<gdr::poseInfo> posesInfo = gdr::GTT::getPoseInfoTimeTranslationOrientation(absolutePoses);

    int counter0 = 0;
    for (const auto &poseInfo: posesInfo) {

        std::cout << poseInfo << " " << counter0 << std::endl;
        ++counter0;
    }

    assert(absoluteRotationsQuat.size() == posesInfo.size());

    std::cout << "________________________________________________" << std::endl;

    double sumErrors = 0;
    double sumErrorsSquared = 0;
    double dev = 0;

    for (int i = 0; i < posesInfo.size(); ++i) {
        double currentAngleError = posesInfo[i].orientationQuat.angularDistance(absoluteRotationsQuat[i]);
        std::cout << i << ":\t" << currentAngleError << std::endl;
        sumErrors += currentAngleError;
        sumErrorsSquared += pow(currentAngleError, 2);

    }
    double meanError = sumErrors / posesInfo.size();
    double meanSquaredError = sumErrorsSquared / posesInfo.size();

    std::cout << "E(error) = " << meanError << std::endl;
    std::cout << "standard deviation(error) = " << meanSquaredError - pow(meanError, 2) << std::endl;

    ASSERT_LE(meanError, 1e-5);
}

TEST(testRotationAveraging, errorShouldBeZeroFirstPoseNotZero) {

    std::string absolutePoses = "../../data/files/absolutePoses_19.txt";
    std::string relativeRotations = "pairWiseFirstPoseZero_19.txt";
    std::string absoluteRotations = "absoluteRotationsTestShanonAveraging_19.txt";
    std::vector<gdr::relativePose> relativePosesVector = gdr::GTT::extractAllRelativeTransformationPairwise(
            absolutePoses,
            relativeRotations,
            "   10000.000000 0.000000 0.000000 0.000000 0.000000 0.000000   10000.000000 0.000000 0.000000 0.000000 0.000000   10000.000000 0.000000 0.000000 0.000000   10000.000000 0.000000 0.000000   10000.000000 0.000000   10000.000000");

    std::vector<Eigen::Quaterniond> absoluteRotationsQuat = gdr::rotationAverager::shanonAveraging(relativeRotations,
                                                                                                   absoluteRotations);

    Eigen::Matrix3d id;
    id.setIdentity();
    Eigen::Quaterniond qid(id);

    std::vector<gdr::poseInfo> posesInfo = gdr::GTT::getPoseInfoTimeTranslationOrientation(absolutePoses);
    std::vector<Eigen::Quaterniond> absoluteRotationsQuatFromGroundTruth;

    for (int i = 0; i < posesInfo.size(); ++i) {
        absoluteRotationsQuatFromGroundTruth.push_back(posesInfo[i].orientationQuat);
    }

    gdr::rotationOperations::applyRotationToAllFromLeft(absoluteRotationsQuatFromGroundTruth,
                                                        absoluteRotationsQuatFromGroundTruth[0].inverse().normalized());

    assert(absoluteRotationsQuat.size() == absoluteRotationsQuatFromGroundTruth.size());

    std::cout << "________________________________________________" << std::endl;

    double sumErrors = 0;
    double sumErrorsSquared = 0;
    double dev = 0;

    for (int i = 0; i < posesInfo.size(); ++i) {
        double currentAngleError = absoluteRotationsQuatFromGroundTruth[i].angularDistance(absoluteRotationsQuat[i]);
        std::cout << i << ":\t" << currentAngleError << std::endl;
        sumErrors += currentAngleError;
        sumErrorsSquared += pow(currentAngleError, 2);

    }
    double meanError = sumErrors / posesInfo.size();
    double meanSquaredError = sumErrorsSquared / posesInfo.size();

    std::cout << "E(error) = " << meanError << std::endl;
    std::cout << "standard deviation(error) = " << meanSquaredError - pow(meanError, 2) << std::endl;


    ASSERT_LE(meanError, 1e-5);
}
TEST(testRotationAveraging, computeAbsoluteRotationsDatasetPlant_19) {

    gdr::CorrespondenceGraph correspondenceGraph("../../data/plantFirst_20_2/rgb", "../../data/plantFirst_20_2/depth",
                                                 527.3,
                                                 318.6, 516.5, 255.3);
    correspondenceGraph.computeRelativePoses();
    std::vector<Eigen::Quaterniond> computedAbsoluteOrientations = correspondenceGraph.performRotationAveraging();

    std::string absolutePoses = "../../data/files/absolutePoses_19.txt";
    std::vector<gdr::poseInfo> posesInfo = gdr::GTT::getPoseInfoTimeTranslationOrientation(absolutePoses);
    std::vector<Eigen::Quaterniond> absoluteRotationsQuatFromGroundTruth;

    for (int i = 0; i < posesInfo.size(); ++i) {
        absoluteRotationsQuatFromGroundTruth.push_back(posesInfo[i].orientationQuat);
    }

    gdr::rotationOperations::applyRotationToAllFromLeft(absoluteRotationsQuatFromGroundTruth,
                                                        absoluteRotationsQuatFromGroundTruth[0].inverse().normalized());


    double sumErrors = 0;
    double sumErrorsSquared = 0;
    double dev = 0;

    for (int i = 0; i < posesInfo.size(); ++i) {
        double currentAngleError = absoluteRotationsQuatFromGroundTruth[i].angularDistance(computedAbsoluteOrientations[i]);
        std::cout << i << ":\t" << currentAngleError << std::endl;
        sumErrors += currentAngleError;
        sumErrorsSquared += pow(currentAngleError, 2);

    }
    double meanError = sumErrors / posesInfo.size();
    double meanSquaredError = sumErrorsSquared / posesInfo.size();

    std::cout << "E(error) = " << meanError << std::endl;
    std::cout << "standard deviation(error) = " << meanSquaredError - pow(meanError, 2) << std::endl;
    ASSERT_LE(meanError, 0.15);
}

int main(int argc, char *argv[]) {

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}