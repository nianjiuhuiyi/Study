#include "DisplayUtils.h"
#include <cmath>


cv::Mat DisplayUtils::convert_ann_to_mask(Annotation &ann, const int &height, int &width) {
	auto &polys = ann.segmentation;

	 cv::Mat mask = cv::Mat::ones(height, width, CV_8U);
	 cv::Mat temp_mask = cv::Mat::zeros(height, width, CV_8U);

	for (auto &poly : polys) {
		if (poly.size() == 4) {
			poly.push_back(poly.at(0));
			poly.push_back(poly.at(1));
		}
		temp_mask = cv::Mat::zeros(height, width, CV_8U);
		
		std::vector<cv::Point2i> pts;
		for (int i = 0; i < poly.size(); ) {
			pts.push_back(cv::Point2i((int)poly.at(i), (int)poly.at(i + 1)));
			i += 2;
		}
		cv::fillPoly(temp_mask, pts, cv::Scalar::all(1));
		cv::bitwise_xor(mask, temp_mask, mask);  // 逻辑亦或
	}

	// opencv的bitwise_not这个是讲 0换成255，将255换成0，而np.logical_not，是将0换成1,1换成0
	// 这里的mask里都是0、1的值，所以要先*255  （这是为了跟python版本保持一致，不然可以直接指定mask为zeros矩阵，画的时候填充0）
	mask *= 255;    
	cv::bitwise_not(mask, mask);
	return mask;
}

void DisplayUtils::draw_box_on_image(cv::Mat &image, const std::vector<std::string> &categories, const Annotation &ann, const cv::Scalar &color) {
	auto &bbox = ann.bbox;
	int x = static_cast<int>(bbox.at(0));
	int y = static_cast<int>(bbox.at(1));
	int w = static_cast<int>(bbox.at(2));
	int h = static_cast<int>(bbox.at(3));
	cv::rectangle(image, cv::Point2i(x, y), cv::Point2i(x + w, y + h), color, this->box_width);

	int baseline = 0;
	baseline += this->box_width;

	// opencv暂时不能显示中文，我先这样处理了
	//std::string text = "";
	//std::string category = categories.at(ann.category_id);
	//bool flag_zn = false;
	//for (auto &c : category) {
	//	try {
	//		if (!std::isalnum(0)) {    // 中文是没办法用这个来处理的
	//			flag_zn = true;
	//			break;
	//		}
	//	}
	//	catch (...) {
	//		flag_zn = true;
	//	}
	//}
	//if (flag_zn) {
	//	text = std::to_string(ann.id) + " " + std::to_string(ann.category_id);
	//}
	//else {
	//	text = std::to_string(ann.id) + " " + category;
	//}

	std::string text = std::to_string(ann.id) + " " + std::to_string(ann.category_id);
	cv::Size txt_size =  cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 0.6, 1, &baseline);
	//cv::putText(image, text, cv::Point(x, y), cv::FONT_HERSHEY_SIMPLEX, 1., cv::Scalar::all(255), 1);   // ::all(255) 就是 (255, 255, 255)，就是白色
	cv::putText(image, text, cv::Point(x, y- this->box_width), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 1);
}

void DisplayUtils::overlay_mask_on_image(cv::Mat &image, const cv::Mat &gray_mask, const cv::Scalar &color=(0, 0, 255)) {

	cv::Mat mask, color_mask, masked_image, overlay_on_masked_image, background;
	
	cv::merge(std::array<cv::Mat, 3>{gray_mask, gray_mask, gray_mask}, mask);
	cv::bitwise_and(mask, color, color_mask);
	cv::bitwise_and(image, color_mask, masked_image);
	cv::addWeighted(masked_image, this->transparency, color_mask , 1 - this->transparency, 0, overlay_on_masked_image);

	cv::bitwise_not(color_mask, color_mask);
	cv::bitwise_and(image, color_mask, background);
	cv::add(background, overlay_on_masked_image, image);
}

void DisplayUtils::increase_transparency() {
	this->transparency = fminf(1.0, this->transparency + 0.05);
}

void DisplayUtils::decrease_transparency() {
	this->transparency = fmaxf(0.0, this->transparency - 0.05);
}

cv::Mat DisplayUtils::draw_annotations(const cv::Mat &image, const std::vector<std::string> &categories,  std::vector<Annotation> &annotations, const std::vector<cv::Scalar> &colors) {
	cv::Mat result;
	image.copyTo(result);
	for (size_t i = 0; i < annotations.size(); ++i) {
		this->draw_box_on_image(result, categories, annotations[i], colors.at(i));
		cv::Mat gray_mask = this->convert_ann_to_mask(annotations[i], result.rows, result.cols);
		this->overlay_mask_on_image(result, gray_mask, colors.at(i));
	}
	return result;
}

cv::Mat DisplayUtils::draw_points(const cv::Mat &image, std::vector<std::array<int, 2>> &points, std::vector<float> &labels, const std::unordered_map<float, cv::Scalar> &colors = { {1., (0, 255, 0)}, {0., (0, 0, 255)} }, const int &radius=5) {
	cv::Mat result;
	image.copyTo(result);
	for (size_t i = 0; i < points.size(); ++i) {
		std::array<int, 2> &point = points.at(i);
		cv::circle(result, cv::Point2i(point[0], point[1]), radius, colors.at(labels[i]), -1);
	}
	return result;
}
