//
// Copyright (c) Leonid Seniukov. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#include "fileProc.h"
#include "printer.h"
#include "errors.h"

#include <algorithm>
#include <fstream>

std::vector<std::string> gdr::readRgbData(std::string pathToRGB) {
    DIR *pDIR;
    struct dirent *entry;
    std::vector<std::string> RgbImages;
    PRINT_PROGRESS("start reading images");
    if ((pDIR = opendir(pathToRGB.data())) != nullptr) {
        while ((entry = readdir(pDIR)) != nullptr) {
            if (std::string(entry->d_name) == "." || std::string(entry->d_name) == "..") {
                continue;
            }
            std::string absolutePathToRgbImage = pathToRGB + "/" + entry->d_name;
            RgbImages.emplace_back(absolutePathToRgbImage);
        }
        closedir(pDIR);
    } else {
        std::cerr << "Unable to open" << std::endl;
        exit(ERROR_OPENING_FILE_READ);
    }
    std::sort(RgbImages.begin(), RgbImages.end());
    return RgbImages;
}

std::vector<std::vector<double>> gdr::parseAbsoluteRotationsFile(const std::string &pathToRotationsFile) {
    std::ifstream in(pathToRotationsFile);
    std::vector<std::vector<double>> quaternions;
    int numOfEmptyLines = 0;
    int numbersInLine = 8;

    if (in) {
        for (int i = 0; i < numOfEmptyLines; ++i) {
            std::string s;
            std::getline(in, s);
        }
        double currVal = -1;
        while (true) {
            std::string s;
            in >> s;
            std::vector<double> stamps;
            for (int i = 0; i < numbersInLine; ++i) {
                if (in >> currVal) {
                    if (i > 3) {
                        stamps.push_back(currVal);
                    }
                } else {
                    return quaternions;
                }
            }
            assert(stamps.size() == numbersInLine - 1);
            quaternions.push_back(stamps);
        }
    } else {
        std::cerr << "ERROR opening file" << std::endl;
        exit(ERROR_OPENING_FILE_READ);
    }
}
