//
// Copyright (c) Leonid Seniukov. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#include "CorrespondenceGraph.h"
#include "groundTruthTransformer.h"

#include <vector>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>
#include <random>

#define DEBUG_PRINT 0
#define SHOW_DEPTH_IMAGES_WITH_KEYPOINTS 0
#define PRINT_RANSAC 0

int CorrespondenceGraph::findCorrespondences() {

    for (int i = 0; i < verticesOfCorrespondence.size(); ++i) {
        for (int j = i + 1; j < verticesOfCorrespondence.size(); ++j) {

            if (DEBUG_PRINT) {
                std::cout << "currently " << i << " " << j << std::endl;
            }
            std::vector<std::pair<int, int>> matchingNumbers = getNumbersOfMatchesKeypoints(
                    std::make_pair(verticesOfCorrespondence[i].keypoints, verticesOfCorrespondence[i].descriptors),
                    std::make_pair(verticesOfCorrespondence[j].keypoints, verticesOfCorrespondence[j].descriptors),
                    siftModule.matcher.get());
            if (DEBUG_PRINT) {
                std::cout << "total matches " << matchingNumbers.size() << std::endl;
            }
            matches[i].push_back({j, matchingNumbers});
        }
    }

    return 0;
}

void MyLine(cv::Mat &img, cv::Point start, cv::Point end) {
    int thickness = 2;
    int lineType = cv::LINE_8;
    line(img,
         start,
         end,
         cv::Scalar(255, 255, 255),
         thickness,
         lineType);
}

void MyLine2(cv::Mat &img, cv::Point start, cv::Point end) {
    int thickness = 2;
    int lineType = cv::LINE_8;
    line(img,
         start,
         end,
         cv::Scalar(215, 215, 215),
         thickness,
         lineType);
}

int CorrespondenceGraph::findTransformationRtMatrices() {
    bool f = true;

    for (int i = 0; i < matches.size(); ++i) {
        for (int j = 0; j < matches[i].size(); ++j) {

            const auto &match = matches[i][j];
            const auto &frameFrom = verticesOfCorrespondence[i];
            const auto &frameTo = verticesOfCorrespondence[match.frameNumber];
            if (DEBUG_PRINT) {
                std::cout << "check this " << frameFrom.index << " -> " << frameTo.index << std::endl;
            }
            assert(frameTo.index > frameFrom.index);
            bool success = true;
            auto cameraMotion = getTransformationRtMatrixTwoImages(i, j, success);

            if (DEBUG_PRINT) {
                std::cout << "out of Transformation calculation" << std::endl;
                std::cout << frameFrom.index << " -> " << frameTo.index << std::endl;
            }

            if (success) {
                int spaceIO = 18;

                Eigen::Matrix3d m3d = cameraMotion.block(0, 0, 3, 3);
                Eigen::Quaterniond qRelatived(m3d);

                if (DEBUG_PRINT) {
                    std::cout << std::setw(2 * spaceIO) << qRelatived.x() << std::setw(2 * spaceIO) << qRelatived.y()
                              << std::setw(2 * spaceIO) << qRelatived.z() << std::setw(2 * spaceIO) << qRelatived.w()
                              << std::endl;
                }

                tranformationRtMatrices[i].push_back(transformationRtMatrix(cameraMotion, frameFrom, frameTo));
                tranformationRtMatrices[frameTo.index].push_back(
                        transformationRtMatrix(cameraMotion.inverse(), frameTo, frameFrom));
            } else {

                if (DEBUG_PRINT) {
                    std::cout << "transformation matrix not found" << std::endl;
                }
            }
        }
    }

    return 0;
}

void CorrespondenceGraph::decreaseDensity() {
    for (std::vector<Match> &correspondenceList: matches) {

        std::sort(correspondenceList.begin(), correspondenceList.end(), [](const auto &lhs, const auto &rhs) {
            return lhs.matchNumbers.size() > rhs.matchNumbers.size();
        });

        if (correspondenceList.size() > maxVertexDegree) {
            std::vector<Match> newMatchList(correspondenceList.begin(), correspondenceList.begin() + maxVertexDegree);
            std::swap(correspondenceList, newMatchList);
        }
    }
}

void CorrespondenceGraph::showKeypointsOnDephtImage(int vertexFrom) {
    auto &image = verticesOfCorrespondence[vertexFrom];
    cv::Mat depthImage = cv::imread(image.pathToDimage, cv::IMREAD_ANYDEPTH);
    std::cout << depthImage.cols << " " << depthImage.rows << std::endl;

    cv::Mat imageDepth1(480, 640, CV_16UC1);
    for (uint x = 0; x < depthImage.cols; ++x) {
        for (uint y = 0; y < depthImage.rows; ++y) {
            auto currentDepth = depthImage.ptr<ushort>(y)[x];
            assert(currentDepth == depthImage.at<ushort>(y, x));
            imageDepth1.at<ushort>(y, x) = currentDepth;
        }
    }

    for (int i = 0; i < image.keypoints.size(); ++i) {
        int x = image.keypoints[i].x;
        int y = image.keypoints[i].y;
        std::cout << ((int) (image.depths[i] * 5000)) << " vs " << depthImage.at<ushort>(y, x) << std::endl;
        std::cout << image.depths[i] << " vs " << depthImage.at<ushort>(y, x) * 1.0 / 5000 << std::endl;
        assert(abs((image.depths[i]) - depthImage.at<ushort>(y, x) / 5000.0) < std::numeric_limits<float>::epsilon());
        imageDepth1.at<ushort>(y, x) = std::numeric_limits<ushort>::max();
    }
    cv::imshow("Made Depths ?", imageDepth1);
    cv::waitKey(0);
    cv::imshow("Known Depths high", depthImage);
    cv::waitKey(0);
    cv::destroyAllWindows();
}

Eigen::Matrix4d
CorrespondenceGraph::getTransformationRtMatrixTwoImages(int vertexFrom, int vertexInList,
                                                        bool &success, double inlierCoeff) {
    Eigen::Matrix4d cR_t_umeyama;
    success = true;
    if (inlierCoeff >= 1.0) {
        inlierCoeff = 1.0;
    }
    if (inlierCoeff < 0) {
        success = false;
        return cR_t_umeyama;
    }
    {
        int dim = 3;
        const auto &match = matches[vertexFrom][vertexInList];
        int minSize = match.matchNumbers.size();
        if (minSize < minNumberOfInliersAfterRobust / inlierCoeff) {
            success = false;
            return cR_t_umeyama;
        }
        MatrixX toBeTransformedPoints = MatrixX(dim + 1, minSize);
        MatrixX originPoints = MatrixX(dim + 1, minSize);

        double mx = 1000, my = 1000, mz = 1000;
        double Mx = -1000, My = -1000, Mz = -1000;
        int num_elements = minSize;
        for (int i = 0; i < minSize; ++i) {
            {
                double x1, y1, z1;
                x1 = verticesOfCorrespondence[vertexFrom].keypoints[match.matchNumbers[i].first].x;
                y1 = verticesOfCorrespondence[vertexFrom].keypoints[match.matchNumbers[i].first].y;
                z1 = verticesOfCorrespondence[vertexFrom].depths[match.matchNumbers[i].first];

                double mirrorParameterH = verticesOfCorrespondence[vertexFrom].heightMirrorParameter;
                assert(y1 < mirrorParameterH && y1 > 0);
                y1 = mirrorParameterH - y1;

                double mirrorParameterW = verticesOfCorrespondence[vertexFrom].widthMirrorParameter;
                assert(x1 < mirrorParameterW && x1 > 0);
                x1 = mirrorParameterW - x1;

                z1 = z1;
                x1 = 1.0 * (x1 - cameraRgbd.cx) * z1 / cameraRgbd.fx;
                y1 = 1.0 * (y1 - cameraRgbd.cy) * z1 / cameraRgbd.fy;

                if (z1 < mz) {
                    mx = x1;
                    my = y1;
                    mz = z1;
                }
                if (z1 > Mz) {
                    Mx = x1;
                    My = y1;
                    Mz = z1;
                }

                originPoints.col(i) << x1, y1, z1, 1;

                assert(originPoints.col(i)[0] == x1);
                assert(originPoints.col(i)[1] == y1);
                assert(originPoints.col(i)[2] == z1);
                assert(originPoints.col(i)[3] == 1);
            }

            {
                double x2, y2, z2;
                x2 = verticesOfCorrespondence[match.frameNumber].keypoints[match.matchNumbers[i].second].x;
                y2 = verticesOfCorrespondence[match.frameNumber].keypoints[match.matchNumbers[i].second].y;
                z2 = verticesOfCorrespondence[match.frameNumber].depths[match.matchNumbers[i].second];

                double mirrorParameterH = verticesOfCorrespondence[vertexFrom].heightMirrorParameter;
                assert(y2 < mirrorParameterH && y2 >= 0);
                y2 = mirrorParameterH - y2;

                double mirrorParameterW = verticesOfCorrespondence[vertexFrom].widthMirrorParameter;
                assert(x2 < mirrorParameterW && x2 > 0);
                x2 = mirrorParameterW - x2;

                z2 = z2;
                x2 = 1.0 * (x2 - cameraRgbd.cx) * z2 / cameraRgbd.fx;
                y2 = 1.0 * (y2 - cameraRgbd.cy) * z2 / cameraRgbd.fy;

                toBeTransformedPoints.col(i) << x2, y2, z2, 1;

                assert(toBeTransformedPoints.col(i)[0] == x2);
                assert(toBeTransformedPoints.col(i)[1] == y2);
                assert(toBeTransformedPoints.col(i)[2] == z2);
                assert(toBeTransformedPoints.col(i)[3] == 1);
            }
        }


        if (DEBUG_PRINT) {
            std::cout << "Points are min" << std::endl;
            std::cout << mx << " " << my << " " << mz << std::endl;

            std::cout << "Points are max" << std::endl;
            std::cout << Mx << " " << My << " " << Mz << std::endl;
        }
        assert(mz > 0);
        assert(Mz > 0);
        Eigen::Matrix4d cR_t_umeyama_1 = umeyama(toBeTransformedPoints.block(0, 0, dim, num_elements),
                                                 originPoints.block(0, 0, dim, num_elements));
        Eigen::Matrix4d cR_t_umeyama_RANSAC = getTransformationMatrixUmeyamaLoRANSAC(toBeTransformedPoints,
                                                                                     originPoints,
                                                                                     numIterations, num_elements,
                                                                                     inlierCoeff);
        cR_t_umeyama = cR_t_umeyama_RANSAC;
        if (DEBUG_PRINT) {
            std::cout << "simple umeyama " << std::endl;
            std::cout << cR_t_umeyama_1 << std::endl;
            std::cout << "RANSAC umeyama " << std::endl;
            std::cout << cR_t_umeyama_RANSAC << std::endl;
            std::cout << "______________________________________________________\n";
            std::cout << "______________________________________________________\n";
        }

        std::vector<double> differences;
        for (int i = 0; i < num_elements; ++i) {
            auto res = cR_t_umeyama * toBeTransformedPoints.col(i);
            double diff = (pow(originPoints.col(i).x() - res[0], 2) + pow(originPoints.col(i).y() - res[1], 2) +
                           pow(originPoints.col(i).z() - res[2], 2));
            differences.push_back(diff);
        }

        std::sort(differences.begin(), differences.end(), [](const auto &lhs, const auto &rhs) { return lhs < rhs; });

        double sum_dif = 0;
        double sum_sq = 0;
        int numOfInliers = 0;
        int aprNumInliers = (int) (differences.size() * inlierCoeff);
        for (int i = 0; i < aprNumInliers; ++i) {
            const auto &e = differences[i];
            if (sqrt(e) < neighbourhoodRadius) {
                ++numOfInliers;
            }

            if (DEBUG_PRINT) {
                std::cout << e << " ";
            }
            sum_dif += e;
            sum_sq += e * e;
        }

        if (numOfInliers < aprNumInliers) {
            success = false;
            return cR_t_umeyama;
        }
        sum_dif /= aprNumInliers;
        sum_sq /= aprNumInliers;

        if (DEBUG_PRINT) {
            std::cout << std::endl << redCode << "MeanEuclidianError = " << sum_dif << "      D="
                      << sum_sq - sum_dif * sum_dif << resetCode << std::endl;
            std::cout << std::endl << redCode << "Inliers " << numOfInliers << resetCode << std::endl;
        }

        std::sort(differences.begin(), differences.end());

        if (DEBUG_PRINT) {
            for (const auto &e: differences) {
                std::cout << e << " ";
            }
            std::cout << std::endl;
        }

        std::vector<double> differences12;
        for (int i = 0; i < num_elements; ++i) {
            double diff = sqrt(pow(originPoints.col(i).x() - toBeTransformedPoints.col(i).x(), 2) +
                               pow(originPoints.col(i).y() - toBeTransformedPoints.col(i).y(), 2) +
                               pow(originPoints.col(i).z() - toBeTransformedPoints.col(i).z(), 2));
            differences12.push_back(diff);
        }

        if (DEBUG_PRINT) {
            std::cout << "__________________________________________\n";
        }
        std::sort(differences12.begin(), differences12.end(),
                  [](const auto &lhs, const auto &rhs) { return lhs > rhs; });

        if (DEBUG_PRINT) {
            for (const auto &e: differences12) {
                std::cout << e << " ";
            }
            std::cout << std::endl;
        }
        std::sort(differences12.begin(), differences12.end());

        if (DEBUG_PRINT) {
            for (const auto &e: differences12) {
                std::cout << e << " ";
            }
            std::cout << std::endl;
            std::cout << "Umeyama\n" << cR_t_umeyama << std::endl;
        }
    }

    if (DEBUG_PRINT) {
        std::cout << "return transformation matrix" << std::endl;
    }
    return cR_t_umeyama;
}

int CorrespondenceGraph::printRelativePosesFile(const std::string &pathOutRelativePoseFile) {

    std::ofstream file(pathOutRelativePoseFile);

    if (file.is_open()) {
        int numPoses = tranformationRtMatrices.size();
        for (int i = 0; i < numPoses; ++i) {
            std::string s1 = "VERTEX_SE3:QUAT ";
            std::string s2 = std::to_string(i) + " 0.000000 0.000000 0.000000 0.0 0.0 0.0 1.0\n";
            file << s1 + s2;
        }
        std::set<std::string> strings;
        for (int i = 0; i < tranformationRtMatrices.size(); ++i) {
            for (int j = 0; j < tranformationRtMatrices[i].size(); ++j) {
                if (i >= tranformationRtMatrices[i][j].vertexTo.index) {
                    continue;
                }
                std::string noise = "   10000.000000 0.000000 0.000000 0.000000 0.000000 0.000000   10000.000000 0.000000 0.000000 0.000000 0.000000   10000.000000 0.000000 0.000000 0.000000   10000.000000 0.000000 0.000000   10000.000000 0.000000   10000.000000";
                std::string edgeId =
                        "EDGE_SE3:QUAT " + std::to_string(tranformationRtMatrices[i][j].vertexTo.index) + " " +
                        std::to_string(i) + " ";
                auto translationVector = tranformationRtMatrices[i][j].t;
                std::string edgeWithTranslation = edgeId + "0.0 0.0 0.0 ";
                const auto &R = tranformationRtMatrices[i][j].R;

                Eigen::Quaterniond qR(R);
                int space = 12;
                std::vector<double> vectorDataRotations = {qR.x(), qR.y(), qR.z(), qR.w()};
                std::string edgeTotal =
                        edgeWithTranslation + std::to_string(qR.x()) + " " + std::to_string(qR.y()) + " " +
                        std::to_string(qR.z()) + " "
                        + std::to_string(qR.w()) + noise + "\n";
                if (strings.find(edgeTotal) != strings.end()) {
                    std::cerr << "Duplicate " << i << " " << j << " j as "
                              << tranformationRtMatrices[i][j].vertexFrom.index << std::endl;
                    std::cout << "ERROR";
                    exit(2);
                }
                strings.insert(edgeTotal);
                file << edgeTotal;
            }
        }
    } else {
        return ERROR_OPENING_FILE_WRITE;
    }

    return 0;
}

int CorrespondenceGraph::performRotationAveraging() {
    std::cout << "first print successfull" << std::endl;
    rotationAverager::shanonAveraging(relativePose, absolutePose);

    std::cout << "Shonan averaging successfull" << std::endl;
    std::vector<std::vector<double>> quaternions = parseAbsoluteRotationsFile(absolutePose);

    std::cout << "read quaternions successfull" << std::endl;
    std::vector<Eigen::Matrix3d> absoluteRotations = getRotationsFromQuaternionVector(quaternions);

    std::cout << "get Rotations from quaternions successfull" << std::endl;
    for (int i = 0; i < verticesOfCorrespondence.size(); ++i) {
        verticesOfCorrespondence[i].setRotation(absoluteRotations[i]);
    }

    std::cout << "set Rotations in vertices successfull" << std::endl;
    return 0;
}

int CorrespondenceGraph::computeRelativePoses() {

    std::vector<std::pair<std::vector<SiftGPU::SiftKeypoint>, std::vector<float>>> keysDescriptorsAll =
            getKeypointsDescriptorsAllImages(
                    siftModule.sift,
                    pathToImageDirectoryRGB);

    verticesOfCorrespondence.reserve(keysDescriptorsAll.size());
    for (int currentImage = 0; currentImage < keysDescriptorsAll.size(); ++currentImage) {
        auto keypointAndDescriptor = keysDescriptorsAll[currentImage];
        std::vector<SiftGPU::SiftKeypoint> &keypoints = keypointAndDescriptor.first;
        std::vector<float> &descriptors = keypointAndDescriptor.second;
        std::vector<SiftGPU::SiftKeypoint> keypointsKnownDepth;
        std::vector<keypointWithDepth> keypointsKnownDepths;
        std::vector<float> descriptorsKnownDepth;
        std::vector<double> depths;

        cv::Mat depthImageLow = cv::imread(imagesD[currentImage], cv::IMREAD_GRAYSCALE);
        cv::Mat depthImage = cv::imread(imagesD[currentImage], cv::IMREAD_ANYDEPTH);
        cv::Mat depthImageS = cv::imread(imagesD[currentImage]);

        std::ofstream myfile;
        int mDepth1 = 0, mDepthLow = 0;

        if (DEBUG_PRINT) {
            std::cout << depthImage.cols << " " << depthImage.rows << std::endl;
        }

        cv::Mat imageDepth1(480, 640, CV_16UC1);
        for (uint x = 0; x < depthImage.cols; ++x) {
            for (uint y = 0; y < depthImage.rows; ++y) {
                auto currentDepth = depthImage.ptr<ushort>(y)[x];
                assert(currentDepth == depthImage.at<ushort>(y, x));
                if (mDepth1 < currentDepth) {
                    mDepth1 = currentDepth;
                }
                if (mDepthLow < depthImageLow.ptr<ushort>(y)[x]) {
                    mDepthLow = currentDepth;
                }
                imageDepth1.at<ushort>(y, x) = 65535 - currentDepth;
            }
        }
        int x = 200, y = 200;

        if (DEBUG_PRINT) {
            std::cout << "depth1 " << depthImage.depth() << " and " << depthImage.channels() << std::endl;
            std::cout << "depthLow " << depthImageLow.depth() << std::endl;
            std::cout << "full value is  ?" << depthImageS.ptr<ushort>(y)[x] << std::endl;
            std::cout << "full value is " << depthImage.ptr<ushort>(y)[x] << std::endl;
            std::cout << "low value is " << depthImageLow.ptr<ushort>(y)[x] << std::endl;
            std::cout << "Max depth  " << mDepth1 << " vs low " << mDepthLow << std::endl;
        }
        for (int i = 0; i < keypoints.size(); ++i) {
            int posInDescriptorVector = 128 * i;
            int currentKeypointDepth = depthImage.at<ushort>(keypoints[i].y, keypoints[i].x);

            if (currentKeypointDepth > 0) {
                assert(currentKeypointDepth < 66000);
                depths.push_back(currentKeypointDepth / 5000.0);
                keypointsKnownDepth.push_back(keypoints[i]);
                std::vector<float> currentDescriptors;
                for (int descriptorCounter = 0; descriptorCounter < 128; ++descriptorCounter) {
                    descriptorsKnownDepth.push_back(descriptors[posInDescriptorVector + descriptorCounter]);
                    currentDescriptors.push_back(descriptors[posInDescriptorVector + descriptorCounter]);
                }
                keypointsKnownDepths.push_back(
                        {keypoints[i], currentKeypointDepth / 5000.0, currentDescriptors});
            }
        }
        VertexCG currentVertex(currentImage, keypointsKnownDepths, keypointsKnownDepth, descriptorsKnownDepth, depths,
                               imagesRgb[currentImage],
                               imagesD[currentImage]);
        verticesOfCorrespondence.push_back(currentVertex);
        assert(keypointsKnownDepth.size() == depths.size());
        assert(verticesOfCorrespondence[verticesOfCorrespondence.size() - 1].depths.size() ==
               verticesOfCorrespondence[verticesOfCorrespondence.size() - 1].keypoints.size());
    }

    if (DEBUG_PRINT) {
        std::cout << "vertices written" << std::endl;
    }
    matches = std::vector<std::vector<Match>>(verticesOfCorrespondence.size());


    if (DEBUG_PRINT) {
        std::cout << "trying to find corr" << std::endl;
    }
    findCorrespondences();
    decreaseDensity();
    findTransformationRtMatrices();


    if (DEBUG_PRINT) {
        for (int i = 0; i < tranformationRtMatrices.size(); ++i) {
            for (int j = 0; j < tranformationRtMatrices[i].size(); ++j) {
                std::cout << "                          " << std::setw(4)
                          << tranformationRtMatrices[i][j].vertexFrom.index
                          << std::setw(4) << tranformationRtMatrices[i][j].vertexTo.index << std::endl;
                std::cout << tranformationRtMatrices[i][j].innerTranformationRtMatrix << std::endl;
                std::cout << "Rotation " << std::endl;
                std::cout << tranformationRtMatrices[i][j].R << std::endl;
                std::cout << "translation " << std::endl;
                std::cout << tranformationRtMatrices[i][j].t << std::endl;
                std::cout
                        << "______________________________________________________________________________________________________"
                        << std::endl;
            }
        }
    }

    std::string poseFile = relativePose;
    printRelativePosesFile(poseFile);

    if (DEBUG_PRINT) {
        printConnectionsRelative(std::cout);
        std::cout << "bfs successfull" << std::endl;
        printConnectionsRelative(std::cout);
    }
    return 0;
};

CorrespondenceGraph::CorrespondenceGraph(const std::string &newPathToImageDirectoryRGB,
                                         const std::string &newPathToImageDirectoryD,
                                         float fx, float cx, float fy, float cy) :
        cameraRgbd({fx, cx, fy, cy}),
        pathToImageDirectoryRGB(newPathToImageDirectoryRGB),
        pathToImageDirectoryD(newPathToImageDirectoryD) {

    imagesRgb = readRgbData(pathToImageDirectoryRGB);
    imagesD = readRgbData(pathToImageDirectoryD);

    std::sort(imagesRgb.begin(), imagesRgb.end());
    std::sort(imagesD.begin(), imagesD.end());


    if (DEBUG_PRINT) {
        std::cout << imagesRgb.size() << " vs " << imagesD.size() << std::endl;
    }
    assert(imagesRgb.size() == imagesD.size());

    tranformationRtMatrices = std::vector<std::vector<transformationRtMatrix>>(imagesD.size());

    if (DEBUG_PRINT) {
        std::cout << "Totally read " << imagesRgb.size() << std::endl;
    }
};

int CorrespondenceGraph::printAbsolutePoses(std::ostream &os, int space) {
    os << "======================NOW 4*4 Matrices of absolute positions=======================\n" << std::endl;

    os << "======================++++++++++++++++=======================\n" << std::endl;
    for (int i = 0; i < verticesOfCorrespondence.size(); ++i) {
        os << "Pose number: " << i << std::endl;
        os << verticesOfCorrespondence[i].absoluteRotationTranslation;
        os << "\n_________________________________________________________________\n";
    }
    return 0;
}

void CorrespondenceGraph::printConnectionsRelative(std::ostream &os, int space) {

    int counter = 0;
    int counterSquared = 0;
    os << "EDGES of the Correspondence Graph:" << std::endl;
    for (int i = 0; i < tranformationRtMatrices.size(); ++i) {
        os << std::setw(space / 5) << i << ":";
        counter += tranformationRtMatrices[i].size();
        counterSquared += tranformationRtMatrices[i].size() * tranformationRtMatrices[i].size();
        for (int j = 0; j < tranformationRtMatrices[i].size(); ++j) {
            const transformationRtMatrix &e = tranformationRtMatrices[i][j];
            assert(i == e.vertexFrom.index);
            os << std::setw(space / 2) << e.vertexTo.index << ",";
        }
        os << std::endl;
    }
    os << "average number of edges " << counter / tranformationRtMatrices.size() << std::endl;

    os << "sq D " << sqrt(counterSquared * 1.0 / tranformationRtMatrices.size() -
                          pow(counter * 1.0 / tranformationRtMatrices.size(), 2)) << std::endl;

}

std::vector<int> CorrespondenceGraph::bfs(int currentVertex) {
    std::vector<bool> visited(verticesOfCorrespondence.size(), false);
    std::vector<int> preds(verticesOfCorrespondence.size(), -1);
    std::queue<int> queueVertices;
    queueVertices.push(currentVertex);
    assert(verticesOfCorrespondence.size() == tranformationRtMatrices.size());
    while (!queueVertices.empty()) {
        int vertex = queueVertices.front();
        std::cout << " entered vertex " << vertex << std::endl;
        queueVertices.pop();
        assert(vertex < visited.size() && vertex >= 0);
        visited[vertex] = true;

        for (int i = 0; i < tranformationRtMatrices[vertex].size(); ++i) {
            int to = tranformationRtMatrices[vertex][i].vertexTo.index;
            if (!visited[to]) {
                queueVertices.push(to);
                visited[to] = true;
                assert(preds[to] == -1);
                preds[to] = vertex;

                ///// get absolute Rotation (R) and Translation (t) with given predecessor Vertex and Relative R & t
                const Eigen::Matrix4d &predAbsoluteRt = verticesOfCorrespondence[vertex].absoluteRotationTranslation;
                Eigen::Matrix3d predR = predAbsoluteRt.block(0, 0, 3, 3);
                Eigen::Vector3d predT = predAbsoluteRt.block(0, 3, 3, 1);

                const Eigen::Matrix4d &relativeRt = tranformationRtMatrices[vertex][i].innerTranformationRtMatrix;
                Eigen::Matrix3d relR = relativeRt.block(0, 0, 3, 3);
                Eigen::Vector3d relT = relativeRt.block(0, 3, 3, 1);

                Eigen::Matrix4d &newAbsoluteRt = verticesOfCorrespondence[to].absoluteRotationTranslation;
                Eigen::Matrix3d newAbsoluteR = newAbsoluteRt.block(0, 0, 3, 3);
                Eigen::Vector3d newAbsoluteT = predR * relT + predT;

                for (int counter = 0; counter < 3; ++counter) {
                    newAbsoluteRt.col(3)[counter] = newAbsoluteT.col(0)[counter];
                }
            }
        }
    }
    return preds;
}
