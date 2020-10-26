#pragma once
#ifndef TEST_SIFTGPU_CG_H
#define TEST_SIFTGPU_CG_H

#include "vertexCG.h"
#include "essentialMatrix.h"
#include "cameraRGBD.h"
#include "siftModule.h"
#include <opencv2/opencv.hpp>

typedef struct Match {
    int frameNumber;
    std::vector<std::pair<int, int>> matchNumbers;
//    int keypointNumber;
//    int correspondingKeypointNumber;

    Match(int newFrameNumber, const std::vector<std::pair<int, int>> &newMatchNumbers) :
            frameNumber(newFrameNumber),
            matchNumbers(newMatchNumbers) {};
} Match;

typedef struct CorrespondenceGraph {
    CameraRGBD cameraRgbd;
    SiftModule siftModule;
    std::vector<vertexCG> verticesOfCorrespondence;
//    std::vector<std::vector<int>> correspondences;
    int maxVertexDegree = 5;
//    std::vector<imageDescriptor> allKeysDescriptors;
//    std::vector<std::vector<std::vector<std::pair<int, int>>>> keypointsMatches;
    std::vector<std::vector<Match>> matches;
    // = (cv::Mat_<double>(3, 3) << fx, 0, cx, 0, fy, cy, 0, 0, 1);
    std::vector<std::vector<essentialMatrix>> essentialMatrices;
    CorrespondenceGraph(const std::string &pathToImageDirectoryRGB, const std::string &pathToImageDirectoryD, float fx, float cx, float fy, float cy);

    int findCorrespondences();
    int findEssentialMatrices();
    void decreaseDensity();
    cv::Mat getEssentialMatrixTwoImages(const CorrespondenceGraph &CG, const vertexCG &frame1, const vertexCG &frame2);
    cv::Mat getEssentialMatrixTwoImagesMatched(int vertexFrom, int vertexTo);
} CorrespondenceGraph;

#endif //TEST_SIFTGPU_CG_H
