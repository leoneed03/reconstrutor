//
// Copyright (c) Leonid Seniukov. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#include <mutex>

#include <tbb/parallel_for.h>

#include "keyPoints/KeyPointsDepthDescriptor.h"
#include "keyPointDetectionAndMatching/FeatureDetector.h"
#include "relativePoseEstimators/EstimatorRobustLoRANSAC.h"
#include "relativePoseRefinement/ICP.h"
#include "computationHandlers/CorrespondenceGraphHandler.h"

namespace gdr {

    CorrespondenceGraphHandler::CorrespondenceGraphHandler(const std::string &pathToImageDirectoryRGB,
                                                           const std::string &pathToImageDirectoryD,
                                                           const CameraRGBD &cameraDefault) {
        correspondenceGraph = std::make_unique<CorrespondenceGraph>(pathToImageDirectoryRGB,
                                                                    pathToImageDirectoryD,
                                                                    cameraDefault);

        siftModule = FeatureDetector::getFeatureDetector(FeatureDetector::SiftDetectorMatcher::SIFTGPU);
        relativePoseEstimatorRobust = std::make_unique<EstimatorRobustLoRANSAC>();
        relativePoseRefiner = std::make_unique<ProcessorICP>();
        threadPool = std::make_unique<ThreadPool>(numberOfThreadsCPU);
    }

    const CorrespondenceGraph &CorrespondenceGraphHandler::getCorrespondenceGraph() const {
        return *correspondenceGraph;
    }

    void CorrespondenceGraphHandler::setNumberOfThreadsCPU(int numberOfThreadsCPUToSet) {
        numberOfThreadsCPU = numberOfThreadsCPUToSet;
    }

    std::vector<std::vector<RelativeSE3>> CorrespondenceGraphHandler::computeRelativePoses() {

        std::cout << "start computing descriptors" << std::endl;

        std::vector<std::pair<std::vector<KeyPoint2D>, std::vector<float>>>
                keysDescriptorsAll = siftModule->getKeypoints2DDescriptorsAllImages(
                correspondenceGraph->getPathsRGB(),
                {0});

        const auto &imagesRgb = correspondenceGraph->getPathsRGB();
        const auto &imagesD = correspondenceGraph->getPathsD();

        for (int currentImage = 0; currentImage < keysDescriptorsAll.size(); ++currentImage) {

            keyPointsDepthDescriptor keyPointsDepthDescriptor = keyPointsDepthDescriptor::filterKeypointsByKnownDepth(
                    keysDescriptorsAll[currentImage], imagesD[currentImage]);
            VertexCG currentVertex(currentImage,
                                   correspondenceGraph->getCameraDefault(),
                                   keyPointsDepthDescriptor,
                                   imagesRgb[currentImage],
                                   imagesD[currentImage]);

            correspondenceGraph->addVertex(currentVertex);

            assert(currentVertex.depths.size() ==
                   currentVertex.keypoints.size());
        }

        std::vector<KeyPointsDescriptors> keyPointsDescriptorsToBeMatched;
        keyPointsDescriptorsToBeMatched.reserve(correspondenceGraph->getNumberOfPoses());

        const auto &vertices = correspondenceGraph->getVertices();
        assert(vertices.size() == correspondenceGraph->getNumberOfPoses());
        for (const auto &vertex: vertices) {
            keyPointsDescriptorsToBeMatched.emplace_back(KeyPointsDescriptors(
                    vertex.getKeyPoints(),
                    vertex.getDescriptors(),
                    vertex.getDepths()
            ));
        }

        assert(keyPointsDescriptorsToBeMatched.size() == vertices.size());
        correspondenceGraph->setPointMatchesRGB(siftModule->findCorrespondences(keyPointsDescriptorsToBeMatched));

        correspondenceGraph->decreaseDensity();
        std::vector<std::vector<std::pair<std::pair<int, int>, KeyPointInfo>>> allInlierKeyPointMatches;
        auto relativePoses = findTransformationRtMatrices(allInlierKeyPointMatches);
        assert(!allInlierKeyPointMatches.empty());
        correspondenceGraph->setInlierPointMatches(allInlierKeyPointMatches);

        correspondenceGraph->setRelativePoses(relativePoses);
        std::string poseFile = getPathRelativePose();
        correspondenceGraph->printRelativePosesFile(poseFile);

        return relativePoses;
    }

    void CorrespondenceGraphHandler::setPathRelativePoseFile(const std::string &relativePoseFilePath) {
        relativePoseFileG2o = relativePoseFilePath;
    }

    const std::string &CorrespondenceGraphHandler::getPathRelativePose() const {
        return relativePoseFileG2o;
    }

    std::vector<std::vector<RelativeSE3>> CorrespondenceGraphHandler::findTransformationRtMatrices(
            std::vector<std::vector<std::pair<std::pair<int, int>, KeyPointInfo>>> &allInlierKeyPointMatches) const {

        std::mutex output;
        int numberOfVertices = getNumberOfVertices();
        tbb::concurrent_vector<tbb::concurrent_vector<RelativeSE3>> transformationMatricesConcurrent(numberOfVertices);

        tbb::concurrent_vector<std::vector<std::pair<std::pair<int, int>, KeyPointInfo>>> allInlierKeyPointMatchesTBB;

        int matchFromIndex = 0;
        std::mutex indexFromMutex;

        const auto &vertices = correspondenceGraph->getVertices();
        const auto &keyPointMatches = correspondenceGraph->getKeyPointMatches();
        assert(keyPointMatches.size() == numberOfVertices);
        assert(numberOfVertices == vertices.size());

        tbb::parallel_for(0, static_cast<int>(numberOfVertices),
                          [&numberOfVertices, &matchFromIndex, &indexFromMutex, this, &allInlierKeyPointMatchesTBB,
                                  &keyPointMatches, &vertices,
                                  &transformationMatricesConcurrent, &output](int vertexNumberFrom) {
                              int i = -1;
                              {
                                  std::unique_lock<std::mutex> lockCounterFrom(indexFromMutex);
                                  i = matchFromIndex;
                                  assert(matchFromIndex >= 0 && matchFromIndex < numberOfVertices);
                                  ++matchFromIndex;
                              }
                              int matchToIndex = 0;
                              std::mutex indexToMutex;

                              tbb::parallel_for(0, static_cast<int>(keyPointMatches[i].size()),
                                                [&matchToIndex, &indexToMutex, i,
                                                        &transformationMatricesConcurrent, this,
                                                        &keyPointMatches, &vertices,
                                                        &allInlierKeyPointMatchesTBB,
                                                        &output](int) {

                                                    int jPos = -1;
                                                    {
                                                        //TODO: use tbb incrementer
                                                        std::unique_lock<std::mutex> lockCounterTo(indexToMutex);
                                                        jPos = matchToIndex;
                                                        assert(matchToIndex >= 0 &&
                                                               matchToIndex < keyPointMatches[i].size());
                                                        ++matchToIndex;
                                                    }
                                                    int j = jPos;
                                                    const auto &match = keyPointMatches[i][j];
                                                    const auto &frameFromDestination = vertices[i];
                                                    const auto &frameToToBeTransformed = vertices[match.getFrameNumber()];

                                                    assert(frameToToBeTransformed.getIndex() >
                                                           frameFromDestination.getIndex());
                                                    bool success = true;
                                                    bool successICP = true;
                                                    std::vector<std::vector<std::pair<std::pair<int, int>, KeyPointInfo>>> inlierKeyPointMatches;
                                                    auto cameraMotion = getTransformationRtMatrixTwoImages(i,
                                                                                                           j,
                                                                                                           inlierKeyPointMatches,
                                                                                                           success,
                                                                                                           ParamsRANSAC());


                                                    if (success) {

                                                        for (const auto &matchPair: inlierKeyPointMatches) {
                                                            allInlierKeyPointMatchesTBB.emplace_back(matchPair);
                                                        }

                                                        // fill info about relative pairwise transformations Rt
                                                        transformationMatricesConcurrent[i].push_back(
                                                                RelativeSE3(
                                                                        cameraMotion,
                                                                        frameFromDestination,
                                                                        frameToToBeTransformed));
                                                        transformationMatricesConcurrent[frameToToBeTransformed.index].push_back(
                                                                RelativeSE3(
                                                                        cameraMotion.inverse(),
                                                                        frameToToBeTransformed,
                                                                        frameFromDestination));

                                                    } else {}
                                                });
                          });

        std::vector<std::vector<RelativeSE3>> pairwiseTransformations(numberOfVertices);
//        assert(pairwiseTransformations.size() == transformationRtMatrices.size());
//        assert(transformationMatricesConcurrent.size() == transformationRtMatrices.size());

        for (int i = 0; i < transformationMatricesConcurrent.size(); ++i) {
            for (const auto &transformation: transformationMatricesConcurrent[i]) {
                pairwiseTransformations[i].emplace_back(transformation);
//                transformationRtMatrices[i].push_back(transformation);
            }
        }
        allInlierKeyPointMatches.clear();
        allInlierKeyPointMatches.reserve(allInlierKeyPointMatchesTBB.size());
        for (const auto &matchPair: allInlierKeyPointMatchesTBB) {
            allInlierKeyPointMatches.emplace_back(matchPair);
        }
        assert(allInlierKeyPointMatchesTBB.size() == allInlierKeyPointMatches.size());

        return pairwiseTransformations;
    }

    int CorrespondenceGraphHandler::getNumberOfVertices() const {
        return correspondenceGraph->getNumberOfPoses();
    }

    SE3 CorrespondenceGraphHandler::getTransformationRtMatrixTwoImages(int vertexFromDestDestination,
                                                                       int vertexInListToBeTransformedCanBeComputed,
                                                                       std::vector<std::vector<std::pair<std::pair<int, int>, KeyPointInfo>>> &keyPointMatches,
                                                                       bool &success,
                                                                       const ParamsRANSAC &paramsRansac,
                                                                       bool showMatchesOnImages) const {
        SE3 cR_t_umeyama;
        success = true;

        double inlierCoeff = paramsRansac.getInlierCoeff();

        if (inlierCoeff >= 1.0) {
            inlierCoeff = 1.0;
        }
        if (inlierCoeff < 0) {
            success = false;
            return cR_t_umeyama;
        }

        const auto &match = correspondenceGraph->getMatch(vertexFromDestDestination,
                                                          vertexInListToBeTransformedCanBeComputed);
        int minSize = match.getSize();

        if (minSize < paramsRansac.getInlierNumber()) {
            success = false;
            return cR_t_umeyama;
        }


        std::vector<Point3d> toBeTransformedPointsVector;
        std::vector<Point3d> destinationPointsVector;
        int vertexToBeTransformed = match.getFrameNumber();
        const auto &vertices = correspondenceGraph->getVertices();

        for (int i = 0; i < minSize; ++i) {

            double x_destination, y_destination, z_destination;
            int localIndexDestination = match.getKeyPointIndexDestinationAndToBeTransformed(i).first;
            const auto &siftKeyPointDestination = vertices[vertexFromDestDestination].keypoints[localIndexDestination];
            x_destination = siftKeyPointDestination.getX();
            y_destination = siftKeyPointDestination.getY();
            z_destination = vertices[vertexFromDestDestination].depths[localIndexDestination];
            destinationPointsVector.emplace_back(Point3d(x_destination, y_destination, z_destination));

            double x_toBeTransformed, y_toBeTransformed, z_toBeTransformed;
            int localIndexToBeTransformed = match.getKeyPointIndexDestinationAndToBeTransformed(i).second;

            const auto &siftKeyPointToBeTransformed = vertices[vertexToBeTransformed].keypoints[localIndexToBeTransformed];
            x_toBeTransformed = siftKeyPointToBeTransformed.getX();
            y_toBeTransformed = siftKeyPointToBeTransformed.getY();
            z_toBeTransformed = vertices[vertexToBeTransformed].depths[localIndexToBeTransformed];
            toBeTransformedPointsVector.emplace_back(Point3d(x_toBeTransformed, y_toBeTransformed, z_toBeTransformed));

            std::vector<std::pair<int, int>> points = {{vertexFromDestDestination, localIndexDestination},
                                                       {vertexToBeTransformed,     localIndexToBeTransformed}};

        }
        assert(toBeTransformedPointsVector.size() == minSize);
        assert(destinationPointsVector.size() == minSize);


        Eigen::Matrix4Xd toBeTransformedPoints = vertices[vertexFromDestDestination].getCamera()
                .getPointCloudXYZ1BeforeProjection(toBeTransformedPointsVector);
        Eigen::Matrix4Xd destinationPoints = vertices[match.getFrameNumber()].getCamera().
                getPointCloudXYZ1BeforeProjection(destinationPointsVector);

        assert(toBeTransformedPoints.cols() == minSize);
        assert(destinationPoints.cols() == minSize);

        const auto &cameraToBeTransformed = vertices[vertexFromDestDestination].getCamera();
        const auto &cameraDest = vertices[match.getFrameNumber()].getCamera();
        std::vector<int> inliersAgain;
        SE3 relativePoseLoRANSAC = relativePoseEstimatorRobust->estimateRelativePose(toBeTransformedPoints,
                                                                                     destinationPoints,
                                                                                     cameraToBeTransformed,
                                                                                     cameraDest,
                                                                                     paramsRansac,
                                                                                     success,
                                                                                     inliersAgain);
        if (!success) {
            return cR_t_umeyama;
        }

        auto inlierMatchesCorrespondingKeypointsLoRansac = findInlierPointCorrespondences(vertexFromDestDestination,
                                                                                          vertexInListToBeTransformedCanBeComputed,
                                                                                          relativePoseLoRANSAC,
                                                                                          paramsRansac);
        std::cout << "NEW INLIERS " << inliersAgain.size() << std::endl;
        std::cout << "OLD INLIERS == NEW INLIERS " << inlierMatchesCorrespondingKeypointsLoRansac.size() << std::endl;
        assert(inliersAgain.size() == inlierMatchesCorrespondingKeypointsLoRansac.size());
        assert(inliersAgain.size() >= inlierCoeff * toBeTransformedPoints.cols());

        bool successRefine = true;
        SE3 refinedByICPRelativePose = relativePoseLoRANSAC;
        refineRelativePose(vertices[vertexToBeTransformed],
                           vertices[vertexFromDestDestination],
                           refinedByICPRelativePose,
                           successRefine);

        auto inlierMatchesCorrespondingKeypointsAfterRefinement =
                findInlierPointCorrespondences(vertexFromDestDestination,
                                               vertexInListToBeTransformedCanBeComputed,
                                               refinedByICPRelativePose,
                                               paramsRansac);

        int ransacInliers = inliersAgain.size();
        int ICPinliers = inlierMatchesCorrespondingKeypointsAfterRefinement.size();
        std::cout << "              ransac got " << ransacInliers << "/" << toBeTransformedPoints.cols()
                  << " vs " << ICPinliers << std::endl;

        if (ransacInliers > ICPinliers) {
            // ICP did not refine the relative pose -- return umeyama result
            cR_t_umeyama = relativePoseLoRANSAC;

        } else {
            cR_t_umeyama = refinedByICPRelativePose;
            std::cout << "REFINED________________________________________________" << std::endl;
            std::swap(inlierMatchesCorrespondingKeypointsAfterRefinement, inlierMatchesCorrespondingKeypointsLoRansac);
        }


        std::vector<std::pair<int, int>> matchesForVisualization;
        keyPointMatches = inlierMatchesCorrespondingKeypointsLoRansac;
        const auto &poseFrom = vertices[vertexFromDestDestination];
        const auto &poseTo = vertices[vertexToBeTransformed];

        return cR_t_umeyama;
    }

    std::vector<std::pair<double, double>>
    CorrespondenceGraphHandler::getReprojectionErrorsXY(const Eigen::Matrix4Xd &destinationPoints,
                                                        const Eigen::Matrix4Xd &transformedPoints,
                                                        const CameraRGBD &cameraIntrinsics) {
        Eigen::Matrix3d intrinsicsMatrix = cameraIntrinsics.getIntrinsicsMatrix3x3();

        std::vector<std::pair<double, double>> errorsReprojection;
        errorsReprojection.reserve(destinationPoints.cols());
        assert(destinationPoints.cols() == transformedPoints.cols());

        for (int i = 0; i < destinationPoints.cols(); ++i) {
            Eigen::Vector3d homCoordDestination = intrinsicsMatrix * destinationPoints.col(i).topLeftCorner<3, 1>();
            Eigen::Vector3d homCoordTransformed = intrinsicsMatrix * transformedPoints.col(i).topLeftCorner<3, 1>();

            for (int j = 0; j < 2; ++j) {
                homCoordDestination[j] /= homCoordDestination[2];
                homCoordTransformed[j] /= homCoordTransformed[2];
            }
            double errorX = std::abs(homCoordTransformed[0] - homCoordDestination[0]);
            double errorY = std::abs(homCoordTransformed[1] - homCoordDestination[1]);
            errorsReprojection.emplace_back(std::make_pair(errorX, errorY));
        }

        return errorsReprojection;
    }

    std::vector<std::vector<std::pair<std::pair<int, int>, KeyPointInfo>>>
    CorrespondenceGraphHandler::findInlierPointCorrespondences(int vertexFrom, int vertexInList,
                                                               const SE3 &transformation,
                                                               const ParamsRANSAC &paramsRansac) const {

        std::vector<std::vector<std::pair<std::pair<int, int>, KeyPointInfo>>> correspondencesBetweenTwoImages;
        const auto &match = correspondenceGraph->getMatch(vertexFrom, vertexInList);
        const auto &vertices = correspondenceGraph->getVertices();
        int minSize = match.getSize();

        bool useProjectionError = paramsRansac.getProjectionUsage();
        int p = paramsRansac.getLpMetricParam();


        std::vector<Point3d> toBeTransformedPointsVector;
        std::vector<Point3d> destinationPointsVector;

        for (int i = 0; i < minSize; ++i) {

            double x_destination, y_destination, z_destination;
            int localIndexDestination = match.getKeyPointIndexDestinationAndToBeTransformed(i).first;
            const auto &siftKeyPointDestination = vertices[vertexFrom].keypoints[localIndexDestination];
            x_destination = siftKeyPointDestination.getX();
            y_destination = siftKeyPointDestination.getY();
            z_destination = vertices[vertexFrom].depths[localIndexDestination];

            destinationPointsVector.emplace_back(Point3d(x_destination, y_destination, z_destination));
            KeyPointInfo keyPointInfoDestination(siftKeyPointDestination, z_destination, vertexFrom);

            std::pair<std::pair<int, int>, KeyPointInfo> infoDestinationKeyPoint = {{vertexFrom, localIndexDestination},
                                                                                    KeyPointInfo(
                                                                                            siftKeyPointDestination,
                                                                                            z_destination,
                                                                                            vertexFrom)};

            double x_toBeTransformed, y_toBeTransformed, z_toBeTransformed;
            int localIndexToBeTransformed = match.getKeyPointIndexDestinationAndToBeTransformed(i).second;
            int vertexToBeTransformed = match.getFrameNumber();
            const auto &siftKeyPointToBeTransformed = vertices[vertexToBeTransformed].keypoints[localIndexToBeTransformed];
            x_toBeTransformed = siftKeyPointToBeTransformed.getX();
            y_toBeTransformed = siftKeyPointToBeTransformed.getY();
            z_toBeTransformed = vertices[vertexToBeTransformed].depths[localIndexToBeTransformed];

            toBeTransformedPointsVector.emplace_back(Point3d(x_toBeTransformed, y_toBeTransformed, z_toBeTransformed));


            std::pair<std::pair<int, int>, KeyPointInfo> infoToBeTransformedKeyPoint = {
                    {vertexToBeTransformed, localIndexToBeTransformed},
                    KeyPointInfo(siftKeyPointToBeTransformed,
                                 z_toBeTransformed,
                                 vertexToBeTransformed)};
            correspondencesBetweenTwoImages.push_back({infoDestinationKeyPoint, infoToBeTransformedKeyPoint});

        }
        assert(toBeTransformedPointsVector.size() == minSize);
        assert(destinationPointsVector.size() == minSize);

        const auto &poseToDestination = vertices[match.getFrameNumber()];

        Eigen::Matrix4Xd toBeTransformedPoints = vertices[vertexFrom].getCamera().
                getPointCloudXYZ1BeforeProjection(toBeTransformedPointsVector);
        Eigen::Matrix4Xd destinationPoints = poseToDestination.getCamera().getPointCloudXYZ1BeforeProjection(
                destinationPointsVector);

        Eigen::Matrix4Xd transformedPoints = transformation.getSE3().matrix() * toBeTransformedPoints;

        std::vector<std::vector<std::pair<std::pair<int, int>, KeyPointInfo>>> inlierCorrespondences;

        if (useProjectionError) {
            std::vector<std::pair<double, double>> errorsReprojection =
                    getReprojectionErrorsXY(destinationPoints,
                                            transformedPoints,
                                            poseToDestination.getCamera());

            assert(errorsReprojection.size() == destinationPoints.cols());

            for (int i = 0; i < errorsReprojection.size(); ++i) {
                Sophus::Vector2d errorReprojectionVector(errorsReprojection[i].first, errorsReprojection[i].second);
                double errorNormLp = 0;
                if (p == 1) {
                    errorNormLp = errorReprojectionVector.lpNorm<1>();
                } else if (p == 2) {
                    errorNormLp = errorReprojectionVector.lpNorm<2>();
                } else {
                    assert(false && "only p=1 and p=2 L_p norms for reprojection error can be used");
                }
                if (errorNormLp < paramsRansac.getMaxProjectionErrorPixels()) {
                    inlierCorrespondences.emplace_back(correspondencesBetweenTwoImages[i]);
                }
            }

        } else {
            Eigen::Matrix4Xd residuals = destinationPoints - transformedPoints;
            for (int i = 0; i < residuals.cols(); ++i) {

                double normResidual = residuals.col(i).norm();
                if (normResidual < paramsRansac.getMax3DError()) {
                    inlierCorrespondences.push_back(correspondencesBetweenTwoImages[i]);
                }
            }
        }

        return inlierCorrespondences;
    }

    int CorrespondenceGraphHandler::refineRelativePose(const VertexCG &vertexToBeTransformed,
                                                       const VertexCG &vertexDestination, SE3 &initEstimationRelPos,
                                                       bool &refinementSuccess) const {

        MatchableInfo poseToBeTransformed(vertexToBeTransformed.getPathRGBImage(),
                                          vertexToBeTransformed.getPathDImage(),
                                          vertexToBeTransformed.getKeyPoints2D(),
                                          vertexToBeTransformed.getCamera());

        MatchableInfo poseDestination(vertexDestination.getPathRGBImage(),
                                      vertexDestination.getPathDImage(),
                                      vertexDestination.getKeyPoints2D(),
                                      vertexDestination.getCamera());
        refinementSuccess = relativePoseRefiner->refineRelativePose(poseToBeTransformed,
                                                                    poseDestination,
                                                                    initEstimationRelPos);
        assert(refinementSuccess);

        return 0;
    }

    std::vector<std::vector<int>>
    CorrespondenceGraphHandler::bfsComputeConnectedComponents(std::vector<int> &componentNumberByPoseIndex) const {

        int totalNumberOfPoses = correspondenceGraph->getNumberOfPoses();
        std::vector<std::vector<int>> connectedComponents =
                GraphTraverser::bfsComputeConnectedComponents(*correspondenceGraph, componentNumberByPoseIndex);

        assert(componentNumberByPoseIndex.size() == totalNumberOfPoses);
        assert(totalNumberOfPoses == correspondenceGraph->getNumberOfPoses());

        int sumNumberOfPoses = 0;
        for (const auto &component: connectedComponents) {
            sumNumberOfPoses += component.size();
        }
        assert(sumNumberOfPoses == totalNumberOfPoses);

        return connectedComponents;
    }

    std::vector<ConnectedComponentPoseGraph> CorrespondenceGraphHandler::splitGraphToConnectedComponents() const {
        return GraphTraverser::splitGraphToConnectedComponents(*correspondenceGraph);
    }
}
