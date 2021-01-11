//
// Copyright (c) Leonid Seniukov. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#ifndef GDR_POINTCLOUD_H
#define GDR_POINTCLOUD_H

#include <Eigen/Eigen>
#include <opencv2/opencv.hpp>

#include "cameraRGBD.h"

namespace gdr {
    std::vector<std::vector<double>> getPointCloudFromImage(const std::string& pathToImageDepth);
    cv::Mat getProjectedPointCloud(const std::string& pathToImageDepth, const Eigen::Matrix4d &transformation,
                                   const CameraRGBD &cameraRgbd);
    cv::Mat visualizeTransformedCloud(const Eigen::Matrix4Xd &pointCloud, const Eigen::Matrix4d &transformation,
                                      const CameraRGBD &cameraRgbd);
    Eigen::Matrix4Xd getPointCloudBeforeProjection(const std::vector<std::vector<double>> &pointsFromImage, const CameraRGBD &cameraRgbd);
//    Eigen::Matrix4Xd getPointCoudXYZnonZeroDepth(const std::string& imageDepth, const CameraRGBD& camera);

//    Eigen::Vector3d mirrorPoint(const Eigen::Vector3d &point, double mirrorParamH = 480, double mirrorParamW = 640);
//    Eigen::Vector4d mirrorPoint(const Eigen::Vector4d &point, double mirrorParamH = 480, double mirrorParamW = 640);
}

#endif
