
#include "ZCore.h"

void ApplyLbp(const cv::Mat& src, cv::Mat& dst) {
	dst = cv::Mat::zeros(src.rows - 2, src.cols - 2, CV_8UC1);
	for (int i = 1; i < src.rows - 1; i++) {
		for (int j = 1; j < src.cols - 1; j++) {
			uchar center = src.at<uchar>(i, j);
			uchar code = 0;
			code |= (src.at<uchar>(i - 1, j - 1) >= center) << 7;
			code |= (src.at<uchar>(i - 1, j) >= center) << 6;
			code |= (src.at<uchar>(i - 1, j + 1) >= center) << 5;
			code |= (src.at<uchar>(i, j + 1) >= center) << 4;
			code |= (src.at<uchar>(i + 1, j + 1) >= center) << 3;
			code |= (src.at<uchar>(i + 1, j) >= center) << 2;
			code |= (src.at<uchar>(i + 1, j - 1) >= center) << 1;
			code |= (src.at<uchar>(i, j - 1) >= center) << 0;
			dst.at<uchar>(i - 1, j - 1) = code;
		}
	}
}


std::vector<cv::Mat> InitializeGaborBank() {
	std::vector<cv::Mat> Bank;
	int NumOrientations = 6;

	int KSize = 15;
	double Sigma = 3.0;
	double Lambda = 10.0;
	double Gamma = 0.5;

	for (int I = 0; I < NumOrientations; I++) {
		double Theta = I * CV_PI / NumOrientations;

		cv::Mat Kernel = cv::getGaborKernel(
			cv::Size(KSize, KSize),
			Sigma,
			Theta,
			Lambda,
			Gamma,
			0,
			CV_32F
		);
		Bank.push_back(Kernel);
	}
	return Bank;
}


void ExtractCompCode(const cv::Mat& Src, const std::vector<cv::Mat>& GaborBank, cv::Mat& Dest) {
	cv::Mat SrcFloat, InvertedSrc;

	cv::bitwise_not(Src, InvertedSrc);
	InvertedSrc.convertTo(SrcFloat, CV_32F);

	cv::Mat MaxResponse = cv::Mat::zeros(SrcFloat.size(), CV_32F);
	Dest = cv::Mat::zeros(SrcFloat.size(), CV_8UC1);

	for (int I = 0; I < GaborBank.size(); I++) {
		cv::Mat Response;
		cv::filter2D(SrcFloat, Response, CV_32F, GaborBank[I]);

		for (int Y = 0; Y < SrcFloat.rows; Y++) {
			const float* RespRow = Response.ptr<float>(Y);
			float* MaxRow = MaxResponse.ptr<float>(Y);
			uchar* DestRow = Dest.ptr<uchar>(Y);

			for (int X = 0; X < SrcFloat.cols; X++) {
				if (RespRow[X] > MaxRow[X]) {
					MaxRow[X] = RespRow[X];
					DestRow[X] = I;
				}
			}
		}
	}
}


void VisualizeCompCode(const cv::Mat& Src, const cv::Mat& Dest) {
	Src.convertTo(Dest, CV_8UC1, 50);
}


void ExtractVeinSkeleton(const cv::Mat& Src, cv::Mat& Dest) {
	cv::Mat Enhanced;
	cv::Ptr<cv::CLAHE> Clahe = cv::createCLAHE(3.0, cv::Size(8, 8));
	Clahe->apply(Src, Enhanced);

	cv::Mat Kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(15, 15));
	cv::morphologyEx(Enhanced, Dest, cv::MORPH_BLACKHAT, Kernel);

	cv::threshold(Dest, Dest, 10, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
}


void ExtractFrangiVeins(const cv::Mat& Src, cv::Mat& Dest, float Sigma) {
	cv::Mat Inverted;
	cv::bitwise_not(Src, Inverted);

	cv::Mat Smoothed;
	cv::GaussianBlur(Inverted, Smoothed, cv::Size(0, 0), Sigma);

	cv::Mat Dxx, Dyy, Dxy;
	cv::Sobel(Smoothed, Dxx, CV_32F, 2, 0, 3);
	cv::Sobel(Smoothed, Dyy, CV_32F, 0, 2, 3);
	cv::Sobel(Smoothed, Dxy, CV_32F, 1, 1, 3);

	cv::Mat Vesselness = cv::Mat::zeros(Src.size(), CV_32F);

	float Beta = 0.5f;
	float C = 10.0f;

	for (int Y = 0; Y < Src.rows; Y++) {
		const float* DxxRow = Dxx.ptr<float>(Y);
		const float* DyyRow = Dyy.ptr<float>(Y);
		const float* DxyRow = Dxy.ptr<float>(Y);
		float* VRow = Vesselness.ptr<float>(Y);

		for (int X = 0; X < Src.cols; X++) {
			float DxxVal = DxxRow[X];
			float DyyVal = DyyRow[X];
			float DxyVal = DxyRow[X];

			float Tmp1 = (DxxVal + DyyVal) * 0.5f;
			float Tmp2 = std::sqrt((DxxVal - DyyVal) * (DxxVal - DyyVal) * 0.25f + DxyVal * DxyVal);

			float Lambda1 = Tmp1 + Tmp2;
			float Lambda2 = Tmp1 - Tmp2;

			if (std::abs(Lambda1) > std::abs(Lambda2)) {
				std::swap(Lambda1, Lambda2);
			}

			if (Lambda2 >= 0) {
				VRow[X] = 0.0f;
				continue;
			}

			float Rb = Lambda1 / Lambda2;
			float S2 = Lambda1 * Lambda1 + Lambda2 * Lambda2;

			float Blobness = std::exp(-(Rb * Rb) / (2.0f * Beta * Beta));
			float Structure = 1.0f - std::exp(-S2 / (2.0f * C * C));

			VRow[X] = Blobness * Structure;
		}
	}

	cv::normalize(Vesselness, Dest, 0, 255, cv::NORM_MINMAX, CV_8UC1);
}

void XimgprocSkeletonize(const cv::Mat& Src, cv::Mat& Dest)
{
	cv::ximgproc::thinning(Src, Dest, cv::ximgproc::THINNING_ZHANGSUEN);
}


cv::Mat ExtractDistanceTransformRoi(const cv::Mat& frame, cv::Point& centerPoint, int& dynamicRoiSize) {
	cv::Mat blurred, thresh, distMap;
	cv::Mat outFrame = frame.clone();

	cv::GaussianBlur(frame, blurred, cv::Size(7, 7), 0);
	cv::threshold(blurred, thresh, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

	std::vector<std::vector<cv::Point>> contours;
	cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

	if (contours.empty()) return outFrame;

	double maxArea = 0;
	int largestContourIndex = -1;
	for (size_t i = 0; i < contours.size(); i++) {
		double area = cv::contourArea(contours[i]);
		if (area > maxArea) {
			maxArea = area;
			largestContourIndex = i;
		}
	}

	if (largestContourIndex == -1) return outFrame;

	cv::Mat handMask = cv::Mat::zeros(thresh.size(), CV_8UC1);
	cv::drawContours(handMask, contours, largestContourIndex, cv::Scalar(255), cv::FILLED);

	cv::distanceTransform(handMask, distMap, cv::DIST_L2, 5);

	double minVal, maxVal;
	cv::Point minLoc, maxLoc;
	cv::minMaxLoc(distMap, &minVal, &maxVal, &minLoc, &maxLoc);
	centerPoint = maxLoc;

	double scaleFactor = 2.5;
	dynamicRoiSize = static_cast<int>(maxVal * scaleFactor);

	if (dynamicRoiSize % 2 != 0) dynamicRoiSize++;

	int halfSize = dynamicRoiSize / 2;
	int x1 = std::max(0, centerPoint.x - halfSize);
	int y1 = std::max(0, centerPoint.y - halfSize);
	int x2 = std::min(outFrame.cols, centerPoint.x + halfSize);
	int y2 = std::min(outFrame.rows, centerPoint.y + halfSize);

	cv::rectangle(outFrame, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(255), 2);

	int crossSize = 10;
	cv::line(outFrame, cv::Point(centerPoint.x - crossSize, centerPoint.y),
		cv::Point(centerPoint.x + crossSize, centerPoint.y),
		cv::Scalar(255), 2);
	cv::line(outFrame, cv::Point(centerPoint.x, centerPoint.y - crossSize),
		cv::Point(centerPoint.x, centerPoint.y + crossSize),
		cv::Scalar(255), 2);

	if (!outFrame.isContinuous()) {
		outFrame = outFrame.clone();
	}

	return outFrame;
}


cv::Mat AnnotateMomentsRoi(const cv::Mat& frame, int roiSize, cv::Point& centerPoint) {
	cv::Mat blurred, thresh;
	cv::Mat outFrame = frame.clone();

	cv::GaussianBlur(frame, blurred, cv::Size(7, 7), 0);
	cv::threshold(blurred, thresh, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

	std::vector<std::vector<cv::Point>> contours;
	cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

	if (contours.empty()) return outFrame;

	double maxArea = 0;
	int largestContourIndex = -1;
	for (size_t i = 0; i < contours.size(); i++) {
		double area = cv::contourArea(contours[i]);
		if (area > maxArea) {
			maxArea = area;
			largestContourIndex = i;
		}
	}

	if (largestContourIndex == -1) return outFrame;

	cv::Moments M = cv::moments(contours[largestContourIndex]);
	if (M.m00 != 0) {
		centerPoint.x = static_cast<int>(M.m10 / M.m00);
		centerPoint.y = static_cast<int>(M.m01 / M.m00);
	}
	else {
		return outFrame;
	}

	int halfSize = roiSize / 2;
	int x1 = std::max(0, centerPoint.x - halfSize);
	int y1 = std::max(0, centerPoint.y - halfSize);
	int x2 = std::min(outFrame.cols, centerPoint.x + halfSize);
	int y2 = std::min(outFrame.rows, centerPoint.y + halfSize);

	cv::rectangle(outFrame, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(255), 2);

	int crossSize = 10;
	cv::line(outFrame, cv::Point(centerPoint.x - crossSize, centerPoint.y),
		cv::Point(centerPoint.x + crossSize, centerPoint.y), cv::Scalar(255), 2);
	cv::line(outFrame, cv::Point(centerPoint.x, centerPoint.y - crossSize),
		cv::Point(centerPoint.x, centerPoint.y + crossSize), cv::Scalar(255), 2);

	if (!outFrame.isContinuous()) outFrame = outFrame.clone();
	return outFrame;
}



cv::Mat AnnotateConvexityDefectRoi(const cv::Mat& frame, cv::Point& centerPoint, int& dynamicRoiSize) {
	cv::Mat blurred, thresh;
	cv::Mat outFrame = frame.clone();

	cv::GaussianBlur(frame, blurred, cv::Size(7, 7), 0);
	cv::threshold(blurred, thresh, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

	std::vector<std::vector<cv::Point>> contours;
	cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

	if (contours.empty()) return outFrame;

	double maxArea = 0;
	int largestIndex = -1;
	for (size_t i = 0; i < contours.size(); i++) {
		double area = cv::contourArea(contours[i]);
		if (area > maxArea) { maxArea = area; largestIndex = i; }
	}

	if (largestIndex == -1) return outFrame;
	const std::vector<cv::Point>& handContour = contours[largestIndex];

	std::vector<int> hullIndices;
	cv::convexHull(handContour, hullIndices, false, false);

	std::vector<cv::Vec4i> defects;
	if (hullIndices.size() > 3) {
		cv::convexityDefects(handContour, hullIndices, defects);
	}

	std::vector<cv::Point> valleyPoints;
	cv::Rect boundingRect = cv::boundingRect(handContour);

	float minDepthThreshold = boundingRect.height / 8.0f;

	for (const auto& defect : defects) {
		float depth = defect[3] / 256.0f;
		if (depth > minDepthThreshold) {
			int farIdx = defect[2];
			cv::Point valley = handContour[farIdx];

			if (valley.y < boundingRect.y + (boundingRect.height * 0.6f)) {
				valleyPoints.push_back(valley);
				cv::circle(outFrame, valley, 4, cv::Scalar(255), -1);
			}
		}
	}

	if (valleyPoints.size() >= 2) {
		int sumX = 0, sumY = 0;
		for (const auto& pt : valleyPoints) {
			sumX += pt.x;
			sumY += pt.y;
		}
		int avgX = sumX / valleyPoints.size();
		int avgY = sumY / valleyPoints.size();

		int shiftDown = static_cast<int>(boundingRect.height * 0.25f);
		centerPoint = cv::Point(avgX, avgY + shiftDown);
	}
	else {
		cv::Moments M = cv::moments(handContour);
		if (M.m00 != 0) {
			centerPoint = cv::Point(static_cast<int>(M.m10 / M.m00), static_cast<int>(M.m01 / M.m00));
		}
		else {
			return outFrame;
		}
	}

	dynamicRoiSize = static_cast<int>(boundingRect.width * 0.6f);
	if (dynamicRoiSize % 2 != 0) dynamicRoiSize++;

	int halfSize = dynamicRoiSize / 2;
	int x1 = std::max(0, centerPoint.x - halfSize);
	int y1 = std::max(0, centerPoint.y - halfSize);
	int x2 = std::min(outFrame.cols, centerPoint.x + halfSize);
	int y2 = std::min(outFrame.rows, centerPoint.y + halfSize);

	cv::rectangle(outFrame, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(255), 2);

	int crossSize = 10;
	cv::line(outFrame, cv::Point(centerPoint.x - crossSize, centerPoint.y),
		cv::Point(centerPoint.x + crossSize, centerPoint.y), cv::Scalar(255), 2);
	cv::line(outFrame, cv::Point(centerPoint.x, centerPoint.y - crossSize),
		cv::Point(centerPoint.x, centerPoint.y + crossSize), cv::Scalar(255), 2);

	if (!outFrame.isContinuous()) outFrame = outFrame.clone();
	return outFrame;
}


cv::Mat DrawPcaDistanceRoi(const cv::Mat& frame) {
	cv::Mat blurred, thresh, distMap;
	cv::Mat outFrame = frame.clone();

	cv::GaussianBlur(frame, blurred, cv::Size(7, 7), 0);
	cv::threshold(blurred, thresh, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

	std::vector<std::vector<cv::Point>> contours;
	cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
	if (contours.empty()) return outFrame;

	int largestIndex = -1;
	double maxArea = 0;
	for (size_t i = 0; i < contours.size(); i++) {
		double area = cv::contourArea(contours[i]);
		if (area > maxArea) { maxArea = area; largestIndex = i; }
	}
	if (maxArea < 5000) return outFrame;

	cv::Mat data(contours[largestIndex].size(), 2, CV_64F);
	for (int i = 0; i < data.rows; i++) {
		data.at<double>(i, 0) = contours[largestIndex][i].x;
		data.at<double>(i, 1) = contours[largestIndex][i].y;
	}
	cv::PCA pca_analysis(data, cv::Mat(), cv::PCA::DATA_AS_ROW);

	cv::Point2f majorVector(pca_analysis.eigenvectors.at<double>(0, 0), pca_analysis.eigenvectors.at<double>(0, 1));
	double angleRad = atan2(majorVector.y, majorVector.x);
	double angleDeg = angleRad * 180.0 / CV_PI;

	cv::Mat handMask = cv::Mat::zeros(thresh.size(), CV_8UC1);
	cv::drawContours(handMask, contours, largestIndex, cv::Scalar(255), cv::FILLED);

	cv::distanceTransform(handMask, distMap, cv::DIST_L2, 5);

	double minVal, maxVal;
	cv::Point minLoc, maxLoc;
	cv::minMaxLoc(distMap, &minVal, &maxVal, &minLoc, &maxLoc);

	cv::Point2f palmCenter = maxLoc;
	double palmRadius = maxVal;
	double boxSize = palmRadius * 2.2;

	cv::RotatedRect roiRect(palmCenter, cv::Size2f(boxSize, boxSize), angleDeg + 90);
	cv::Point2f vertices[4];
	roiRect.points(vertices);
	for (int i = 0; i < 4; i++) {
		cv::line(outFrame, vertices[i], vertices[(i + 1) % 4], cv::Scalar(255), 2);
	}

	int crossSize = 10;
	cv::line(outFrame, cv::Point(palmCenter.x - crossSize, palmCenter.y),
		cv::Point(palmCenter.x + crossSize, palmCenter.y), cv::Scalar(255), 2);
	cv::line(outFrame, cv::Point(palmCenter.x, palmCenter.y - crossSize),
		cv::Point(palmCenter.x, palmCenter.y + crossSize), cv::Scalar(255), 2);

	if (!outFrame.isContinuous()) {
		outFrame = outFrame.clone();
	}

	return outFrame;
}


#include <cmath>

cv::Mat DrawDistanceMomentsRoi(const cv::Mat& frame) {
	cv::Mat blurred, thresh, distMap;
	cv::Mat outFrame = frame.clone();

	cv::GaussianBlur(frame, blurred, cv::Size(7, 7), 0);
	cv::threshold(blurred, thresh, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

	cv::rectangle(thresh, cv::Point(0, 0), cv::Point(thresh.cols - 1, thresh.rows - 1), cv::Scalar(0), 1);

	cv::distanceTransform(thresh, distMap, cv::DIST_L2, 5);

	double minVal, maxVal;
	cv::Point minLoc, maxLoc;
	cv::minMaxLoc(distMap, &minVal, &maxVal, &minLoc, &maxLoc);

	if (maxVal < 20) return outFrame;

	cv::Point2f palmCenter = maxLoc;
	double boxSize = maxVal * 2.2;

	cv::Moments m = cv::moments(thresh, true);
	double angleDeg = 0.0;

	if (m.m00 != 0) {
		double mu20 = m.mu20 / m.m00;
		double mu02 = m.mu02 / m.m00;
		double mu11 = m.mu11 / m.m00;

		double angleRad = 0.5 * std::atan2(2 * mu11, mu20 - mu02);
		angleDeg = angleRad * 180.0 / CV_PI;
	}

	cv::RotatedRect roiRect(palmCenter, cv::Size2f(boxSize, boxSize), angleDeg);
	cv::Point2f vertices[4];
	roiRect.points(vertices);

	for (int i = 0; i < 4; i++) {
		cv::line(outFrame, vertices[i], vertices[(i + 1) % 4], cv::Scalar(255), 2);
	}

	int crossSize = 10;
	cv::line(outFrame, cv::Point(palmCenter.x - crossSize, palmCenter.y),
		cv::Point(palmCenter.x + crossSize, palmCenter.y), cv::Scalar(255), 2);
	cv::line(outFrame, cv::Point(palmCenter.x, palmCenter.y - crossSize),
		cv::Point(palmCenter.x, palmCenter.y + crossSize), cv::Scalar(255), 2);

	if (!outFrame.isContinuous()) outFrame = outFrame.clone();

	return outFrame;
}




cv::Point2f lastCenter(0, 0);
float lastAngle = 0.0f;
float lastSize = 100.0f;
bool isLocked = false;

cv::Mat DrawStickyDistanceRoi(const cv::Mat& frame) {
	cv::Mat thresh, distMap;
	cv::Mat outFrame = frame.clone();

	cv::GaussianBlur(frame, thresh, cv::Size(5, 5), 0);
	cv::threshold(thresh, thresh, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

	cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));

	cv::morphologyEx(thresh, thresh, cv::MORPH_OPEN, kernel);
	cv::morphologyEx(thresh, thresh, cv::MORPH_CLOSE, kernel);

	std::vector<std::vector<cv::Point>> contours;
	cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

	cv::Mat cleanMask = cv::Mat::zeros(thresh.size(), CV_8UC1);
	int handIdx = -1;
	double maxArea = 0;

	for (int i = 0; i < contours.size(); i++) {
		double area = cv::contourArea(contours[i]);
		if (area > 5000 && area > maxArea) { 
			maxArea = area;
			handIdx = i;
		}
	}

	if (handIdx != -1) {
		cv::drawContours(cleanMask, contours, handIdx, cv::Scalar(255), cv::FILLED);

		cv::copyMakeBorder(cleanMask, cleanMask, 1, 1, 1, 1, cv::BORDER_CONSTANT, cv::Scalar(0));

		cv::distanceTransform(cleanMask, distMap, cv::DIST_L2, 5);
		double maxVal; cv::Point maxLoc;
		cv::minMaxLoc(distMap, nullptr, &maxVal, nullptr, &maxLoc);

		if (maxVal > 25) {
			isLocked = true;
			cv::Point2f targetCenter(maxLoc.x - 1, maxLoc.y - 1);

			cv::Moments m = cv::moments(cleanMask, true);
			float targetAngle = 0.5f * std::atan2(2 * m.mu11, m.mu20 - m.mu02) * 180.0f / (float)CV_PI;

			lastCenter.x += (targetCenter.x - lastCenter.x) * 0.25f;
			lastCenter.y += (targetCenter.y - lastCenter.y) * 0.25f;
			lastAngle += (targetAngle - lastAngle) * 0.15f;
			lastSize += ((float)maxVal * 2.3f - lastSize) * 0.2f;
		}
	}
	else {
		isLocked = false;
	}

	if (isLocked || lastSize > 60) {
		cv::RotatedRect roiBox(lastCenter, cv::Size2f(lastSize, lastSize), lastAngle);
		cv::Point2f vtx[4];
		roiBox.points(vtx);
		for (int i = 0; i < 4; i++) {
			cv::line(outFrame, vtx[i], vtx[(i + 1) % 4], cv::Scalar(255), 2);
		}
	}

	if (!outFrame.isContinuous()) outFrame = outFrame.clone();
	return outFrame;
}
