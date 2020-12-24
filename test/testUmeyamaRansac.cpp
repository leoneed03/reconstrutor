#include <iostream>

#include <gtest/gtest.h>
#include <vector>
#include "CorrespondenceGraph.h"

bool testMSEisLessThanEpsilon(double mse) {
    bool MSEisLessThanEpsilon = mse < 3 * std::numeric_limits<Scalar>::epsilon();
    if (MSEisLessThanEpsilon) {
        std::cout << "error MSE: " << mse << std::endl;
    }
    return MSEisLessThanEpsilon;
}
TEST(testUmeyamaRansac, allInliers) {
    CorrespondenceGraph correspondenceGraph("../../data/plantFirst_20_2/rgb", "../../data/plantFirst_20_2/depth", 525.0,
                                            319.5, 525.0, 239.5);

    int numOfPoints = 100;
    Eigen::Matrix3d rotationMatrix;
    std::vector<double> angles = {30, 50, -87};

    rotationMatrix = Eigen::AngleAxisd(angles[0], Eigen::Vector3d::UnitZ())
                     * Eigen::AngleAxisd(angles[1], Eigen::Vector3d::UnitY())
                     * Eigen::AngleAxisd(angles[2], Eigen::Vector3d::UnitZ());

    Eigen::Matrix4d transformationMatrix;
    transformationMatrix.setIdentity();
    transformationMatrix.block(0, 0, 3, 3) = rotationMatrix;
    std::vector<double> translation = {3, 0.5, -0.5};
    for (int i = 0; i < 3; ++i) {
        transformationMatrix.col(3)[i] = translation[i];
    }

    MatrixX src = MatrixX::Random(4, numOfPoints);
    src.row(3) = Eigen::Matrix<Scalar, 1, Eigen::Dynamic>::Constant(numOfPoints, Scalar(1));

    MatrixX dst = transformationMatrix * src;

    Eigen::Matrix4d umeyamaTransformation = getTransformationMatrixUmeyamaLoRANSAC(
            src, dst, 50, src.cols(), 0.9
    );

    const Scalar error = (dst - umeyamaTransformation * src).squaredNorm();

    auto mse = 1.0 * error / src.cols();
    bool MSEisLessThanEpsilon = testMSEisLessThanEpsilon(mse);

    ASSERT_TRUE(MSEisLessThanEpsilon);
}


TEST(testUmeyamaRansac, Inliers90percent) {
    CorrespondenceGraph correspondenceGraph("../../data/plantFirst_20_2/rgb", "../../data/plantFirst_20_2/depth", 525.0,
                                            319.5, 525.0, 239.5);

    int numOfPoints = 100;
    double outlierCoeff = 0.1;
    int numOutliers = 100 * outlierCoeff;
    Eigen::Matrix3d rotationMatrix;
    std::vector<double> angles = {10, 5, 70};

    rotationMatrix = Eigen::AngleAxisd(angles[0], Eigen::Vector3d::UnitZ())
                     * Eigen::AngleAxisd(angles[1], Eigen::Vector3d::UnitY())
                     * Eigen::AngleAxisd(angles[2], Eigen::Vector3d::UnitZ());

    Eigen::Matrix4d transformationMatrix;
    transformationMatrix.setIdentity();
    transformationMatrix.block(0, 0, 3, 3) = rotationMatrix;
    std::vector<double> translation = {3, 0.5, -0.5};
    for (int i = 0; i < 3; ++i) {
        transformationMatrix.col(3)[i] = translation[i];
    }

    MatrixX src = MatrixX::Random(4, numOfPoints);
    src.row(3) = Eigen::Matrix<Scalar, 1, Eigen::Dynamic>::Constant(numOfPoints, Scalar(1));

    MatrixX outliers = MatrixX::Random(4, numOutliers);
    outliers.row(3) = Eigen::Matrix<Scalar, 1, Eigen::Dynamic>::Constant(numOutliers, Scalar(1));


    srand((unsigned int) time(0));
    for (int i = 0; i < numOutliers; ++i) {
        int pos = rand() % numOfPoints;
        src.col(pos) = outliers.col(i);
    }

    MatrixX dst = transformationMatrix * src;

    Eigen::Matrix4d umeyamaTransformation = getTransformationMatrixUmeyamaLoRANSAC(
            src, dst, 50, src.cols(), 0.8
    );

    MatrixX afterTransformation = umeyamaTransformation * src;

    std::vector<double> errors;
    for (int i = 0; i < afterTransformation.cols(); ++i) {
        errors.push_back((afterTransformation.col(i) - dst.col(i)).squaredNorm());
    }
    std::sort(errors.begin(), errors.end());
    errors.resize(numOfPoints - numOutliers);

    double mse = 0;
    for (const auto &e: errors) {
        mse += e;
    }
    mse /= errors.size();
    bool MSEisLessThanEpsilon = testMSEisLessThanEpsilon(mse);

    ASSERT_TRUE(MSEisLessThanEpsilon);

}

int main(int argc, char *argv[]) {

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}