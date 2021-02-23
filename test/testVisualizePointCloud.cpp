//
// Copyright (c) Leonid Seniukov. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.
//


#include <iostream>
#include <gtest/gtest.h>
#include <vector>
#include <fstream>

#include <pcl/visualization/cloud_viewer.h>
#include "poseEstimation.h"
#include "CorrespondenceGraph.h"
#include "groundTruthTransformer.h"
#include "SmoothPointCloud.h"


void visualizeSimple() {

    pcl::PointCloud<pcl::PointXYZRGB>::Ptr input_cloud(new pcl::PointCloud<pcl::PointXYZRGB>);

    std::vector<std::pair<std::vector<double>, std::vector<int>>> XYZ_RGBs;

    for (int i = 0; i < 200; ++i) {
        int rgb = (i * (256 / 20)) % 256;
        XYZ_RGBs.push_back({{1.0 * i, 1.0 * i, 1.0 * i},
                            {rgb,     rgb,     rgb}});
    }

    for (int i = 0; i < XYZ_RGBs.size(); ++i) {

        auto &pointXYZ = XYZ_RGBs[i].first;
        auto &pointRGB = XYZ_RGBs[i].second;
        pcl::PointXYZRGB pointToBeAdded;
        pointToBeAdded.x = pointXYZ[0];
        pointToBeAdded.y = pointXYZ[1];
        pointToBeAdded.z = pointXYZ[2];

        pointToBeAdded.r = pointRGB[0];
        pointToBeAdded.g = pointRGB[1];
        pointToBeAdded.b = pointRGB[2];

        input_cloud->push_back(pointToBeAdded);


//        if (i % 3 == 1 || i == XYZ_RGBs.size()) {
//            // Filtering input scan to roughly 10% of original size to increase speed of registration.
//            pcl::PointCloud<pcl::PointXYZRGB>::Ptr filtered_cloud(new pcl::PointCloud<pcl::PointXYZRGB>);
//            pcl::ApproximateVoxelGrid<pcl::PointXYZRGB> approximate_voxel_filter;
//            approximate_voxel_filter.setLeafSize(0.1, 0.1, 0.1);
//            approximate_voxel_filter.setInputCloud(input_cloud);
//            approximate_voxel_filter.filter(*filtered_cloud);
//            std::swap(filtered_cloud, input_cloud);
//        }
    }
//
//    pcl::visualization::PCLVisualizer::Ptr viewer(new pcl::visualization::PCLVisualizer("3D Viewer"));
//    viewer->setBackgroundColor(0, 0, 0);
//    pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGB> rgb(input_cloud);
//    viewer->addPointCloud<pcl::PointXYZRGB>(input_cloud, rgb, "sample cloud");
//    viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 3, "sample cloud");
//    viewer->addCoordinateSystem(1.0);
//    viewer->initCameraParameters();
//    while (!viewer->wasStopped()) {
//    }
    pcl::visualization::CloudViewer viewer("Simple Cloud Viewer");
    viewer.showCloud(input_cloud);

    while (!viewer.wasStopped()) {
    }

}


TEST(testVisualizationGT, SmoothedPointCloudGroundTruthPosesNumber2) {

    std::string pathToImageDirectoryRGB = "../../data/360_dataset_sampled/each5/rgb";
    std::string pathToImageDirectoryD = "../../data/360_dataset_sampled/each5/depth";

    auto imagesRgb = gdr::readRgbData(pathToImageDirectoryRGB);
    auto imagesD = gdr::readRgbData(pathToImageDirectoryD);
//    gdr::CorrespondenceGraph correspondenceGraph("../../data/360_dataset_sampled/each5/rgb",
//                                                 "../../data/360_dataset_sampled/each5/depth",
//                                                 517.3,318.6,
//                                                 516.5, 255.3);
    gdr::CameraRGBD camera(517.3, 318.6,
                           516.5, 255.3);
//    correspondenceGraph.computeRelativePoses();
//    std::vector<Eigen::Quaterniond> computedAbsoluteOrientationsNoRobust = correspondenceGraph.performRotationAveraging();
//    std::vector<Eigen::Quaterniond> computedAbsoluteOrientationsRobust = correspondenceGraph.optimizeRotationsRobust();
//    std::vector<Eigen::Vector3d> computedAbsoluteTranslationsIRLS = correspondenceGraph.optimizeAbsoluteTranslations();

//    std::vector<Sophus::SE3d> bundleAdjustedPoses = correspondenceGraph.performBundleAdjustmentUsingDepth();


    std::string absolutePoses = "../../data/360_dataset_sampled/each5/groundtruth_new.txt";
//    std::string absolutePoses = "../../data/360_dataset_sampled/each5/BA_19.txt";
    std::vector<gdr::poseInfo> posesInfo = gdr::GTT::getPoseInfoTimeTranslationOrientation(absolutePoses);
    std::vector<gdr::VertexCG> vertices;
    std::vector<gdr::VertexCG*> verticesPointers;

    Sophus::SE3d poseZero = posesInfo[0].getSophusPose();

    ASSERT_EQ(posesInfo.size(), imagesRgb.size());
    ASSERT_EQ(posesInfo.size(), imagesD.size());
    for (int i = 0; i < posesInfo.size(); ++i) {
        vertices.push_back(gdr::VertexCG(i, camera, imagesRgb[i], imagesD[i], poseZero.inverse() * posesInfo[i].getSophusPose()));
    }

    for (int i = 0; i < posesInfo.size(); ++i) {
        verticesPointers.push_back(&vertices[i]);
    }
    gdr::SmoothPointCloud smoothCloud;
    smoothCloud.registerPointCloudFromImage(verticesPointers);

}


TEST(testVisualizationGT, SmoothedPointCloudGroundTruth) {

    gdr::CorrespondenceGraph correspondenceGraph("../../data/plantDataset_19_3/rgb",
                                                 "../../data/plantDataset_19_3/depth",
                                                 517.3,
                                                 318.6, 516.5, 255.3);
    correspondenceGraph.computeRelativePoses();
//    std::vector<Eigen::Quaterniond> computedAbsoluteOrientationsNoRobust = correspondenceGraph.performRotationAveraging();
//    std::vector<Eigen::Quaterniond> computedAbsoluteOrientationsRobust = correspondenceGraph.optimizeRotationsRobust();
//    std::vector<Eigen::Vector3d> computedAbsoluteTranslationsIRLS = correspondenceGraph.optimizeAbsoluteTranslations();

//    std::vector<Sophus::SE3d> bundleAdjustedPoses = correspondenceGraph.performBundleAdjustmentUsingDepth();


    std::string absolutePoses = "../../data/files/absolutePoses_19.txt";
    std::vector<gdr::poseInfo> poses = gdr::GTT::getPoseInfoTimeTranslationOrientation(absolutePoses);
    std::vector<gdr::VertexCG *> vertices;

    assert(poses.size() == correspondenceGraph.verticesOfCorrespondence.size());
    assert(poses.size() == 19);

    Sophus::SE3d pose0;
    pose0.setQuaternion(poses[0].getOrientationQuat());
    pose0.translation() = poses[0].getTranslation();
    for (int i = 0; i < poses.size(); ++i) {
        auto *vertex = &correspondenceGraph.verticesOfCorrespondence[i];
        Sophus::SE3d poseI;
        poseI.setQuaternion(poses[i].getOrientationQuat());
        poseI.translation() = poses[i].getTranslation();

        vertex->setRotationTranslation(pose0.inverse() * poseI);

        std::cout << "pose " << i << " qx qy qz qw: " << vertex->getRotationQuat().coeffs().transpose() << " tx ty tz: "
                  << vertex->getEigenMatrixAbsolutePose4d().topRightCorner<3, 1>().transpose() << std::endl;

        auto se3 = vertex->getEigenMatrixAbsolutePose4d();
        vertices.push_back(vertex);
    }
    gdr::SmoothPointCloud smoothCloud;
    smoothCloud.registerPointCloudFromImage(vertices);

}


TEST(testVisualization, SmoothedPointCloud) {

    gdr::CorrespondenceGraph correspondenceGraph("../../data/plantDataset_19_3/rgb",
                                                 "../../data/plantDataset_19_3/depth",
                                                 517.3,318.6,
                                                 516.5, 255.3);
    correspondenceGraph.computeRelativePoses();
    std::vector<Eigen::Quaterniond> computedAbsoluteOrientationsNoRobust = correspondenceGraph.performRotationAveraging();
    std::vector<Eigen::Quaterniond> computedAbsoluteOrientationsRobust = correspondenceGraph.optimizeRotationsRobust();
    std::vector<Eigen::Vector3d> computedAbsoluteTranslationsIRLS = correspondenceGraph.optimizeAbsoluteTranslations();
    std::vector<Sophus::SE3d> bundleAdjustedPoses = correspondenceGraph.performBundleAdjustmentUsingDepth();
    std::vector<gdr::VertexCG *> vertices;
    for (auto &vertex: correspondenceGraph.verticesOfCorrespondence) {
        vertices.push_back(&vertex);
    }


    std::cout << "total Umeyama poses " << correspondenceGraph.totalMeausedRelativePoses << std::endl;
    std::cout << " ICP refined poses " << correspondenceGraph.refinedPoses << " percentage:  "
              << 1.0 * correspondenceGraph.refinedPoses / correspondenceGraph.totalMeausedRelativePoses << std::endl;

    gdr::SmoothPointCloud smoothCloud;
    smoothCloud.registerPointCloudFromImage(vertices);

}

TEST(testVisualization, ShonanConverges) {

    int iterations = 10;
    for (int i = 0; i < iterations; ++i) {
        gdr::CorrespondenceGraph correspondenceGraph("../../data/360_dataset_sampled/each5/rgb",
                                                     "../../data/360_dataset_sampled/each5/depth",
                                                     517.3, 318.6,
                                                     516.5, 255.3);
        correspondenceGraph.computeRelativePoses();
        std::vector<Eigen::Quaterniond> computedAbsoluteOrientationsNoRobust = correspondenceGraph.performRotationAveraging();
        std::vector<Eigen::Quaterniond> computedAbsoluteOrientationsRobust = correspondenceGraph.optimizeRotationsRobust();
        std::vector<Eigen::Vector3d> computedAbsoluteTranslationsIRLS = correspondenceGraph.optimizeAbsoluteTranslations();
        std::vector<Sophus::SE3d> bundleAdjustedPoses = correspondenceGraph.performBundleAdjustmentUsingDepth();
        std::vector<gdr::VertexCG *> vertices;
        for (auto &vertex: correspondenceGraph.verticesOfCorrespondence) {
            vertices.push_back(&vertex);
        }
    }
    std::cout << "shonan converged " << iterations << std::endl;
    ASSERT_TRUE(true);

}


TEST(testVisualization, SmoothedPointCloud360OfficeEach2) {

    std::set<int> sampledIndices;
    for (int i = 0; i < 755; i += 2) {
        sampledIndices.insert(i);
    }
    gdr::GTT::prepareDataset("/home/leoneed/Desktop/360dataset", "/home/leoneed/testGDR1/GDR/data/360_2", sampledIndices, "");
    gdr::CorrespondenceGraph correspondenceGraph("../../data/360_2/rgb",
                                                 "../../data/360_2/depth",
                                                 517.3,318.6,
                                                 516.5, 255.3);
    correspondenceGraph.computeRelativePoses();
    bool isConnected = true;
    correspondenceGraph.bfs(0, isConnected);
    ASSERT_TRUE(isConnected);
    std::vector<Eigen::Quaterniond> computedAbsoluteOrientationsNoRobust = correspondenceGraph.performRotationAveraging();
    std::vector<Eigen::Quaterniond> computedAbsoluteOrientationsRobust = correspondenceGraph.optimizeRotationsRobust();
    std::vector<Eigen::Vector3d> computedAbsoluteTranslationsIRLS = correspondenceGraph.optimizeAbsoluteTranslations();
    std::vector<Sophus::SE3d> bundleAdjustedPoses = correspondenceGraph.performBundleAdjustmentUsingDepth();
    std::vector<gdr::VertexCG *> vertices;
    for (auto &vertex: correspondenceGraph.verticesOfCorrespondence) {
        vertices.push_back(&vertex);
    }

    std::string absolutePosesGT = "../../data/360_2/groundtruth_new.txt";
    std::vector<gdr::poseInfo> posesInfo = gdr::GTT::getPoseInfoTimeTranslationOrientation(absolutePosesGT);
    std::string outputName = "/home/leoneed/Desktop/evaluate_ate_scale/360_sampled/BA_378.txt";
    std::ofstream computedPoses(outputName);

    assert(posesInfo.size() == correspondenceGraph.verticesOfCorrespondence.size());
    assert(!posesInfo.empty());
    for (int i = 0; i < correspondenceGraph.verticesOfCorrespondence.size(); ++i) {
        const auto& poseBA = bundleAdjustedPoses[i];
        Sophus::SE3d movedPose = posesInfo[0].getSophusPose() * poseBA;
        const auto to = movedPose.translation();
        computedPoses.precision(std::numeric_limits<double>::max_digits10);
        computedPoses << posesInfo[i].getTimestamp() << ' ';
        for (int j = 0; j < 3; ++j) {
            computedPoses << to[j] << ' ';
        }
        auto quatComputed = movedPose.unit_quaternion();

        computedPoses << quatComputed.x() << ' ' << quatComputed.y() << ' ' << quatComputed.z() << ' '
                      << quatComputed.w() << std::endl;
    }

    std::cout << "total Umeyama poses " << correspondenceGraph.totalMeausedRelativePoses << std::endl;
    std::cout << " ICP refined poses " << correspondenceGraph.refinedPoses << " percentage:  "
              << 1.0 * correspondenceGraph.refinedPoses / correspondenceGraph.totalMeausedRelativePoses << std::endl;

    gdr::SmoothPointCloud smoothCloud;
    smoothCloud.registerPointCloudFromImage(vertices);

}




TEST(testVisualization, SmoothedPointCloud360Office) {

    std::set<int> sampledIndices;
    for (int i = 0; i < 755; i += 5) {
        sampledIndices.insert(i);
    }
//    gdr::GTT::prepareDataset("/home/leoneed/Desktop/360dataset", "/home/leoneed/testGDR1/GDR/data/360_dataset_sampled", sampledIndices, "each5");
    gdr::CorrespondenceGraph correspondenceGraph("../../data/360_dataset_sampled/each5/rgb",
                                                 "../../data/360_dataset_sampled/each5/depth",
                                                 517.3, 318.6,
                                                 516.5, 255.3);
    correspondenceGraph.computeRelativePoses();
    bool isConnected = true;
    correspondenceGraph.bfs(0, isConnected);
    ASSERT_TRUE(isConnected);
    std::vector<Eigen::Quaterniond> computedAbsoluteOrientationsNoRobust = correspondenceGraph.performRotationAveraging();
    std::vector<Eigen::Quaterniond> computedAbsoluteOrientationsRobust = correspondenceGraph.optimizeRotationsRobust();
    std::vector<Eigen::Vector3d> computedAbsoluteTranslationsIRLS = correspondenceGraph.optimizeAbsoluteTranslations();
    std::vector<Sophus::SE3d> bundleAdjustedPoses = correspondenceGraph.performBundleAdjustmentUsingDepth();
    std::vector<gdr::VertexCG *> vertices;
    for (auto &vertex: correspondenceGraph.verticesOfCorrespondence) {
        vertices.push_back(&vertex);
    }

    std::string absolutePosesGT = "../../data/360_dataset_sampled/each5/groundtruth_new.txt";
    std::vector<gdr::poseInfo> posesInfo = gdr::GTT::getPoseInfoTimeTranslationOrientation(absolutePosesGT);
    std::string outputName = "/home/leoneed/Desktop/evaluate_ate_scale/360_sampled/BA_150.txt";
    std::ofstream computedPoses(outputName);

    assert(posesInfo.size() == correspondenceGraph.verticesOfCorrespondence.size());
    assert(!posesInfo.empty());
    for (int i = 0; i < correspondenceGraph.verticesOfCorrespondence.size(); ++i) {
        const auto& poseBA = bundleAdjustedPoses[i];
        Sophus::SE3d movedPose = posesInfo[0].getSophusPose() * poseBA;
        const auto to = movedPose.translation();
        computedPoses.precision(std::numeric_limits<double>::max_digits10);
        computedPoses << posesInfo[i].getTimestamp() << ' ';
        for (int j = 0; j < 3; ++j) {
            computedPoses << to[j] << ' ';
        }
        auto quatComputed = movedPose.unit_quaternion();

        computedPoses << quatComputed.x() << ' ' << quatComputed.y() << ' ' << quatComputed.z() << ' '
                      << quatComputed.w() << std::endl;
    }

    std::cout << "total Umeyama poses " << correspondenceGraph.totalMeausedRelativePoses << std::endl;
    std::cout << " ICP refined poses " << correspondenceGraph.refinedPoses << " percentage:  "
              << 1.0 * correspondenceGraph.refinedPoses / correspondenceGraph.totalMeausedRelativePoses << std::endl;

    gdr::SmoothPointCloud smoothCloud;
    smoothCloud.registerPointCloudFromImage(vertices);

}


TEST(testBundleAdjustment, BundleAdjustedUsingDepthPosesAreBetterThanAveraged) {

    gdr::CorrespondenceGraph correspondenceGraph("../../data/plantDataset_19_3/rgb",
                                                 "../../data/plantDataset_19_3/depth",
                                                 517.3,
                                                 318.6, 516.5, 255.3);
    correspondenceGraph.computeRelativePoses();
    std::vector<Eigen::Quaterniond> computedAbsoluteOrientationsNoRobust = correspondenceGraph.performRotationAveraging();
    std::vector<Eigen::Quaterniond> computedAbsoluteOrientationsRobust = correspondenceGraph.optimizeRotationsRobust();
    std::vector<Eigen::Vector3d> computedAbsoluteTranslationsIRLS = correspondenceGraph.optimizeAbsoluteTranslations();
    std::vector<Sophus::SE3d> bundleAdjustedPoses = correspondenceGraph.performBundleAdjustmentUsingDepth();


    for (int i = 0; i < bundleAdjustedPoses.size(); ++i) {
        ASSERT_LE(bundleAdjustedPoses[i].unit_quaternion().angularDistance(
                correspondenceGraph.verticesOfCorrespondence[i].getRotationQuat().normalized()), 1e-10);
    }
    std::string absolutePoses = "../../data/files/absolutePoses_19.txt";
    std::vector<gdr::poseInfo> posesInfo = gdr::GTT::getPoseInfoTimeTranslationOrientation(absolutePoses);

    std::vector<Eigen::Vector3d> absoluteTranslationsFromGroundTruth;

    for (int i = 0; i < posesInfo.size(); ++i) {
        absoluteTranslationsFromGroundTruth.push_back(posesInfo[i].getTranslation());
    }
    assert(posesInfo.size() == correspondenceGraph.verticesOfCorrespondence.size());
    assert(posesInfo.size() == correspondenceGraph.verticesOfCorrespondence.size());


    double errorRotRobust = 0;
    for (int i = 0; i < computedAbsoluteOrientationsRobust.size(); ++i) {
        const auto &quat = computedAbsoluteOrientationsRobust[i];
        double dErrorRobust = quat.angularDistance(
                posesInfo[0].getOrientationQuat().inverse().normalized() * posesInfo[i].getOrientationQuat());
        std::cout << i << " -- pose error robust is: " << dErrorRobust << std::endl;
        errorRotRobust += dErrorRobust;
    }


    double errorRotBA = 0;
    for (int i = 0; i < correspondenceGraph.verticesOfCorrespondence.size(); ++i) {
        auto quatBA = correspondenceGraph.verticesOfCorrespondence[i].getRotationQuat();
        double dError = quatBA.angularDistance(
                posesInfo[0].getOrientationQuat().inverse().normalized() * posesInfo[i].getOrientationQuat());
        std::cout << i << " -- pose error BA is: " << dError << std::endl;
        errorRotBA += dError;
    }


    auto zeroT = absoluteTranslationsFromGroundTruth[0];

    for (auto &translations: absoluteTranslationsFromGroundTruth) {
        translations -= zeroT;
    }

    std::cout << "_______________________VS_______________________________________" << std::endl;
    for (int i = 0; i < absoluteTranslationsFromGroundTruth.size(); ++i) {
        const auto &t = absoluteTranslationsFromGroundTruth[i];
        const auto &to = computedAbsoluteTranslationsIRLS[i];
        std::cout << i << ": \t" << t[0] << " \t" << t[1] << " \t" << t[2] << std::endl;
        std::cout << " : \t" << to[0] << " \t" << to[1] << " \t" << to[2] << std::endl;
    }

    std::cout << "______________________________________________________________" << std::endl;


    double sumErrors = 0;
    double sumErrorsSquared = 0;
    double dev = 0;

//    std::string outputName = "absolutePoses_19_BA.txt";
    std::string outputName = "/home/leoneed/Desktop/evaluate_ate_scale/b/absolutePoses_19_BA_usingDepth.txt";
    std::ofstream computedPoses(outputName);

    assert(computedAbsoluteTranslationsIRLS.size() == absoluteTranslationsFromGroundTruth.size());
    for (int i = 0; i < posesInfo.size(); ++i) {
        double currentL2Error = (absoluteTranslationsFromGroundTruth[i] - computedAbsoluteTranslationsIRLS[i]).norm();
        computedPoses.precision(std::numeric_limits<double>::max_digits10);
        std::cout << i << ":\t" << currentL2Error << std::endl;
        computedPoses << posesInfo[i].getTimestamp() << ' ';
        const auto to = bundleAdjustedPoses[i].translation();
        for (int j = 0; j < 3; ++j) {
            computedPoses << to[j] << ' ';
        }
        auto quatComputed = bundleAdjustedPoses[i].unit_quaternion();

        computedPoses << quatComputed.x() << ' ' << quatComputed.y() << ' ' << quatComputed.z() << ' '
                      << quatComputed.w() << std::endl;
        sumErrors += currentL2Error;
        sumErrorsSquared += pow(currentL2Error, 2);

    }
    double meanError = sumErrors / posesInfo.size();
    double meanSquaredError = sumErrorsSquared / posesInfo.size();


    correspondenceGraph.printConnectionsRelative(std::cout);
    std::cout << "IRLS for translations result" << std::endl;
    std::cout << "E(error) = " << meanError << std::endl;
    std::cout << "standard deviation(error) = " << meanSquaredError - pow(meanError, 2) << std::endl;
    std::cout << "______________________ROTATION REPORT______________________" << std::endl;
    std::cout << "Mean Rot angle error BA " << errorRotBA / correspondenceGraph.verticesOfCorrespondence.size()
              << std::endl;
    std::cout << "Mean Rot angle error robust " << errorRotRobust / correspondenceGraph.verticesOfCorrespondence.size()
              << std::endl;

    ASSERT_LE(errorRotBA, errorRotRobust);
    ASSERT_LE(meanError, 0.15);
}


int main(int argc, char *argv[]) {

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

