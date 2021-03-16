//
// Copyright (c) Leonid Seniukov. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#include "rotationAveraging.h"
#include "ConnectedComponent.h"
#include "Point3d.h"
#include "BundleAduster.h"
#include "RotationOptimizationRobust.h"
#include "translationMeasurement.h"
#include "translationAveraging.h"

#include <fstream>
#include <boost/filesystem.hpp>

namespace gdr {

    ConnectedComponentPoseGraph::ConnectedComponentPoseGraph(
            const std::vector<VertexCG> &newAbsolutePoses,
            const std::vector<std::vector<RelativePoseSE3>> &edgesLocalIndicesRelativePoses,
            const CameraRGBD &newDefaultCamera,
            const std::vector<std::vector<std::pair<std::pair<int, int>, KeyPointInfo>>> &newInlierPointCorrespondences,
            const std::string &newRelativeRotationsFile,
            const std::string &newAbsoluteRotationsFile,
            int componentNumber
    ) : absolutePoses(newAbsolutePoses),
        relativePoses(edgesLocalIndicesRelativePoses),
        cameraRgbd(newDefaultCamera),
        inlierPointCorrespondences(newInlierPointCorrespondences),
        relativeRotationsFile(newRelativeRotationsFile),
        absoluteRotationsFile(newAbsoluteRotationsFile),
        componentGlobalNumberOptional(componentNumber) {


        std::vector<VertexCG *> posesForCloudProjector;
        assert(getNumberOfPoses() > 0);
        posesForCloudProjector.reserve(getNumberOfPoses());

        for (int i = 0; i < getNumberOfPoses(); ++i) {
            posesForCloudProjector.push_back(&absolutePoses[i]);
        }
        assert(getNumberOfPoses() == absolutePoses.size());
        assert(posesForCloudProjector.size() == getNumberOfPoses());
        cloudProjector.setPoses(posesForCloudProjector);
        pointMatcher.setNumberOfPoses(getNumberOfPoses());
    }

    void ConnectedComponentPoseGraph::computePointClasses() {

        const auto &matchesBetweenPoints = inlierPointCorrespondences;
        for (const auto &vectorOfMatches: matchesBetweenPoints) {

            std::vector<std::pair<int, int>> poseAndLocalIndices;
            for (const std::pair<std::pair<int, int>, KeyPointInfo> &fullPointInfo: vectorOfMatches) {
                poseAndLocalIndices.push_back(fullPointInfo.first);
            }
            pointMatcher.insertPointsWithNewClasses(poseAndLocalIndices);
        }


        // unordered map's Key is local index
        std::vector<std::unordered_map<int, KeyPointInfo>> keyPointInfoByPoseNumAndLocalInd(
                pointMatcher.getNumberOfPoses());


        for (const auto &vectorOfMatches: matchesBetweenPoints) {

            for (const std::pair<std::pair<int, int>, KeyPointInfo> &fullPointInfo: vectorOfMatches) {
                const auto &poseNumAndLocalInd = fullPointInfo.first;
                const auto &foundIt = keyPointInfoByPoseNumAndLocalInd[poseNumAndLocalInd.first].find(
                        poseNumAndLocalInd.second);
                if (foundIt != keyPointInfoByPoseNumAndLocalInd[poseNumAndLocalInd.first].end()) {
                    assert(foundIt->second == fullPointInfo.second);
                } else {
                    keyPointInfoByPoseNumAndLocalInd[poseNumAndLocalInd.first].insert(
                            std::make_pair(poseNumAndLocalInd.second, fullPointInfo.second));
                }
            }
        }

        auto pointClasses = pointMatcher.assignPointClasses();

        for (int pointIncrementor = 0; pointIncrementor < pointClasses.size(); ++pointIncrementor) {
            int pointClassNumber = pointClasses[pointIncrementor];
            std::pair<int, int> poseNumberAndLocalIndex = pointMatcher.getPoseNumberAndLocalIndex(pointIncrementor);
            std::vector<KeyPointInfo> keyPointInfo;
            keyPointInfo.push_back(
                    keyPointInfoByPoseNumAndLocalInd[poseNumberAndLocalIndex.first][poseNumberAndLocalIndex.second]);
            cloudProjector.addPoint(pointClassNumber, keyPointInfo);
        }
    }


    std::vector<Eigen::Quaterniond> ConnectedComponentPoseGraph::performRotationAveraging() {

        printRelativeRotationsToFile(relativeRotationsFile);

        std::vector<Eigen::Quaterniond> absoluteRotationsQuats =
                rotationAverager::shanonAveraging(relativeRotationsFile,
                                                  absoluteRotationsFile);

        for (int i = 0; i < getNumberOfPoses(); ++i) {
            absolutePoses[i].setRotation(absoluteRotationsQuats[i]);
        }

        return absoluteRotationsQuats;
    }

    std::vector<Eigen::Quaterniond> ConnectedComponentPoseGraph::optimizeRotationsRobust() {

        std::vector<Rotation3d> shonanOptimizedAbsolutePoses;

        for (const auto &vertexPose: absolutePoses) {
            shonanOptimizedAbsolutePoses.push_back(Rotation3d(vertexPose.getRotationQuat()));
        }

        assert(shonanOptimizedAbsolutePoses.size() == getNumberOfPoses());


        std::vector<rotationMeasurement> relativeRotationsAfterICP;

        assert(getNumberOfPoses() == relativePoses.size());
        for (int indexFrom = 0; indexFrom < getNumberOfPoses(); ++indexFrom) {
            for (const auto &knownRelativePose: relativePoses[indexFrom]) {
                assert(indexFrom == knownRelativePose.getIndexFrom());

                if (knownRelativePose.getIndexFrom() < knownRelativePose.getIndexTo()) {
                    relativeRotationsAfterICP.push_back(
                            rotationMeasurement(knownRelativePose.getRelativeRotationSO3Quatd(),
                                                knownRelativePose.getIndexFrom(),
                                                knownRelativePose.getIndexTo()));
                }
            }
        }

        RotationOptimizer rotationOptimizer(shonanOptimizedAbsolutePoses, relativeRotationsAfterICP);
        std::vector<Eigen::Quaterniond> optimizedPosesRobust = rotationOptimizer.getOptimizedOrientation();

        assert(getNumberOfPoses() == optimizedPosesRobust.size());

        for (int i = 0; i < getNumberOfPoses(); ++i) {
            absolutePoses[i].setRotation(optimizedPosesRobust[i]);
        }

        return optimizedPosesRobust;
    }

    std::vector<Eigen::Matrix4d> ConnectedComponentPoseGraph::getAbsolutePosesEigenMatrix4d() const {
        std::vector<Eigen::Matrix4d> poses;

        for (const auto &pose: absolutePoses) {
            poses.push_back(pose.getEigenMatrixAbsolutePose4d());
        }

        return poses;
    }

    std::vector<Eigen::Vector3d> ConnectedComponentPoseGraph::optimizeAbsoluteTranslations(int indexFixedToZero) {

        std::vector<translationMeasurement> relativeTranslations;
        std::vector<Eigen::Matrix4d> absolutePosesMatrix4d = getAbsolutePosesEigenMatrix4d();

        for (int indexFrom = 0; indexFrom < getNumberOfPoses(); ++indexFrom) {
            for (const auto &knownRelativePose: relativePoses[indexFrom]) {
                assert(indexFrom == knownRelativePose.getIndexFrom());

                if (knownRelativePose.getIndexFrom() < knownRelativePose.getIndexTo()) {
                    relativeTranslations.push_back(
                            translationMeasurement(knownRelativePose.getRelativeTranslationV3(),
                                                   knownRelativePose.getIndexFrom(),
                                                   knownRelativePose.getIndexTo()));
                }
            }
        }

        std::vector<Eigen::Vector3d> optimizedAbsoluteTranslationsIRLS = translationAverager::recoverTranslations(
                relativeTranslations,
                absolutePosesMatrix4d).toVectorOfVectors();


        bool successIRLS = true;


        // Now run IRLS with PCG answer as init solution

        optimizedAbsoluteTranslationsIRLS = translationAverager::recoverTranslationsIRLS(
                relativeTranslations,
                absolutePosesMatrix4d,
                optimizedAbsoluteTranslationsIRLS,
                successIRLS).toVectorOfVectors();


        Eigen::Vector3d zeroTranslation = optimizedAbsoluteTranslationsIRLS[indexFixedToZero];
        for (auto &translation: optimizedAbsoluteTranslationsIRLS) {
            translation -= zeroTranslation;
        }


        assert(getNumberOfPoses() == optimizedAbsoluteTranslationsIRLS.size());
        for (int i = 0; i < getNumberOfPoses(); ++i) {
            absolutePoses[i].setTranslation(optimizedAbsoluteTranslationsIRLS[i]);
        }
        return optimizedAbsoluteTranslationsIRLS;
    }

    std::vector<Sophus::SE3d> ConnectedComponentPoseGraph::performBundleAdjustmentUsingDepth(int indexFixedToZero) {
        int maxNumberOfPointsToShow = -1;
        computePointClasses();
        std::vector<Point3d> observedPoints = cloudProjector.setComputedPointsGlobalCoordinates();
        std::cout << "ready " << std::endl;
        std::vector<std::pair<Sophus::SE3d, CameraRGBD>> posesAndCameraParams;
        for (const auto &vertexPose: absolutePoses) {
            posesAndCameraParams.push_back({vertexPose.absolutePose, cameraRgbd});
        }
        std::cout << "BA depth create BundleAdjuster" << std::endl;

        std::vector<double> errorsBefore;
        auto shownResidualsBefore = cloudProjector.showPointsReprojectionError(observedPoints, "before", errorsBefore,
                                                                               absolutePoses[0].getCamera(),
                                                                               maxNumberOfPointsToShow);
        //vizualize points and matches
//        cloudProjector.showPointsProjection(observedPoints);

        BundleAdjuster bundleAdjuster(observedPoints, posesAndCameraParams,
                                      cloudProjector.getKeyPointInfoByPoseNumberAndPointClass());

        std::vector<Sophus::SE3d> posesOptimized = bundleAdjuster.optimizePointsAndPosesUsingDepthInfo(
                indexFixedToZero);

        assert(posesOptimized.size() == getNumberOfPoses());

        for (int i = 0; i < getNumberOfPoses(); ++i) {
            auto &vertexPose = absolutePoses[i];
            vertexPose.setRotationTranslation(posesOptimized[i]);
        }

        std::vector<double> errorsAfter;

        auto shownResidualsAfter = cloudProjector.showPointsReprojectionError(observedPoints, "after", errorsAfter,
                                                                              absolutePoses[0].getCamera(),
                                                                              maxNumberOfPointsToShow);

        assert(shownResidualsAfter.size() == shownResidualsBefore.size());

        std::string pathToRGBDirectoryToSave = "shownResiduals";
        boost::filesystem::path pathToRemove(pathToRGBDirectoryToSave);

        std::cout << "path [" << pathToRemove.string() << "]" << " exists? Answer: "
                  << boost::filesystem::exists(pathToRemove) << std::endl;
        boost::filesystem::remove_all(pathToRemove);
        std::cout << "removed from " << pathToRemove.string() << std::endl;
        boost::filesystem::create_directories(pathToRemove);

        int counterMedianErrorGotWorse = 0;
        int counterMedianErrorGotBetter = 0;
        int counterSumMedianError = 0;

        for (int i = 0; i < shownResidualsAfter.size(); ++i) {

            boost::filesystem::path pathToSave = pathToRemove;
            bool medianErrorGotLessAfterBA = (errorsBefore[i] > errorsAfter[i]);

            if (medianErrorGotLessAfterBA) {
                ++counterMedianErrorGotBetter;
            } else {
                ++counterMedianErrorGotWorse;
            }
            ++counterSumMedianError;

            std::string betterOrWorseResults = medianErrorGotLessAfterBA ? " " : " [WORSE] ";
            std::string nameImageReprojErrors =
                    std::to_string(i) + betterOrWorseResults + " quantils: " + std::to_string(errorsBefore[i]) +
                    " -> " + std::to_string(errorsAfter[i]) + ".png";
            pathToSave.append(nameImageReprojErrors);
            cv::Mat stitchedImageResiduals;
            std::vector<cv::DMatch> matches1to2;
            std::vector<cv::KeyPoint> keyPointsToShowFirst;
            std::vector<cv::KeyPoint> keyPointsToShowSecond;
            cv::Mat stitchedImage;
            cv::drawMatches(shownResidualsBefore[i], {},
                            shownResidualsAfter[i], {},
                            matches1to2,
                            stitchedImage);
            cv::imwrite(pathToSave.string(), stitchedImage);
        }

        std::cout << "BETTER #median error: " << counterMedianErrorGotBetter
                  << " vs WORSE: " << counterMedianErrorGotWorse << " of total " << counterSumMedianError << std::endl;
        std::cout << "percentage better median error is "
                  << 1.0 * counterMedianErrorGotBetter / counterSumMedianError << std::endl;

        assert(counterMedianErrorGotWorse + counterMedianErrorGotBetter == counterSumMedianError);

        // visualize point correspondences:
//        cloudProjector.showPointsProjection(bundleAdjuster.getPointsGlobalCoordinatesOptimized());
        return posesOptimized;
    }

    int ConnectedComponentPoseGraph::getNumberOfPoses() const {
        return absolutePoses.size();
    }

    std::set<int> ConnectedComponentPoseGraph::initialIndices() const {
        std::set<int> initialIndices;
        for (const auto &pose: absolutePoses) {
            std::cout << " #index " << pose.getIndex() << " became " << pose.initialIndex << std::endl;
            initialIndices.insert(pose.initialIndex);
        }
        return initialIndices;
    }


    int
    ConnectedComponentPoseGraph::printRelativeRotationsToFile(const std::string &pathToFileRelativeRotations) const {

        std::ofstream file(pathToFileRelativeRotations);

        if (file.is_open()) {
            int numPoses = getNumberOfPoses();
            for (int i = 0; i < numPoses; ++i) {
                std::string s1 = "VERTEX_SE3:QUAT ";
                std::string s2 = std::to_string(i) + " 0.000000 0.000000 0.000000 0.0 0.0 0.0 1.0\n";
                file << s1 + s2;
            }
            for (int i = 0; i < relativePoses.size(); ++i) {
                for (int j = 0; j < relativePoses[i].size(); ++j) {

                    const auto &transformation = relativePoses[i][j];
                    if (i >= transformation.getIndexTo()) {
                        continue;
                    }
                    std::string noise = "   10000.000000 0.000000 0.000000 0.000000 0.000000 0.000000   10000.000000 0.000000 0.000000 0.000000 0.000000   10000.000000 0.000000 0.000000 0.000000   10000.000000 0.000000 0.000000   10000.000000 0.000000   10000.000000";

                    int indexTo = transformation.getIndexTo();
                    int indexFrom = i;
                    //order of vertices in the EDGE_SE3:QUAT representation is reversed (bigger_indexTo less_indexFrom)(gtsam format) TODO: actually seems like NOT
                    file << "EDGE_SE3:QUAT " << indexFrom << ' ' << indexTo << ' ';
                    auto translationVector = transformation.getRelativeTranslationR3();
                    file << ' ' << std::to_string(translationVector.col(0)[0]) << ' '
                         << std::to_string(translationVector.col(0)[1]) << ' '
                         << std::to_string(translationVector.col(0)[2]) << ' ';

                    Eigen::Quaterniond qR = transformation.getRelativeRotationSO3Quatd();
                    file << std::to_string(qR.x()) << ' ' << std::to_string(qR.y()) << ' '
                         << std::to_string(qR.z()) << ' ' << std::to_string(qR.w()) << noise << '\n';
                }
            }
        } else {
            return 1;
        }

        return 0;
    }

    std::vector<VertexCG *> ConnectedComponentPoseGraph::getVerticesPointers() {
        std::vector<VertexCG *> verticesPointers;

        for (int i = 0; i < absolutePoses.size(); ++i) {
            VertexCG *vertex = &absolutePoses[i];
            verticesPointers.push_back(vertex);
        }

        return verticesPointers;
    }

    int ConnectedComponentPoseGraph::size() const {
        return absolutePoses.size();
    }
}