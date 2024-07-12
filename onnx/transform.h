#pragma once

#include <opencv2/opencv.hpp>

#include "SAMOnnxRunner.h"

std::tuple<int, int> GetPreProcessShape(const int old_h, const int old_w, int long_side_length) {
	/*
		Compute the output size given input size and target long side length.
	*/
	double scale = long_side_length * 1.0 / MAX(old_h, old_w);
	int new_h = static_cast<int>(old_h * scale + 0.5);
	int new_w = static_cast<int>(old_w * scale + 0.5);

	return std::tuple<int, int>(new_h, new_w);
}

cv::Mat ResizeLongestSide_apply_image(const cv::Mat &Image, const int encoder_input_size) {
	cv::Mat resizeImage;
	const unsigned int h = Image.rows;
	const unsigned int w = Image.cols;
	std::tuple<int, int> newShape = GetPreProcessShape(h, w, encoder_input_size);

	cv::resize(Image, resizeImage, cv::Size(std::get<1>(newShape), std::get<0>(newShape)), cv::INTER_AREA);
	return resizeImage;
}

ClickInfo ResizeLongestSide_apply_coord(const cv::Mat &Image, ClickInfo clickinfo, const int encoder_input_size) {
	const unsigned int h = Image.rows;
	const unsigned int w = Image.cols;
	std::tuple<int, int> newShape = GetPreProcessShape(h, w, encoder_input_size);

	float new_w = static_cast<float>(std::get<1>(newShape));
	float new_h = static_cast<float>(std::get<0>(newShape));

	clickinfo.pt.x *= float(new_w / w);
	clickinfo.pt.y *= float(new_h / h);

	return clickinfo;
}

BoxInfo ResizeLongestSide_apply_box(const cv::Mat &Image, BoxInfo boxinfo, const int encoder_input_size) {
	const unsigned int h = Image.rows;
	const unsigned int w = Image.cols;
	std::tuple<int, int> newShape = GetPreProcessShape(h, w, encoder_input_size);

	float new_w = static_cast<float>(std::get<1>(newShape));
	float new_h = static_cast<float>(std::get<0>(newShape));

	boxinfo.left_top.x *= float(new_w / w);
	boxinfo.left_top.y *= float(new_h / h);
	boxinfo.right_bot.x *= float(new_w / w);
	boxinfo.right_bot.y = float(new_h / h);

	return boxinfo;
}
