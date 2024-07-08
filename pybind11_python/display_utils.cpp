#include "display_utils.h"
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

void DisplayUtils::draw_box_on_image(cv::Mat &image, const std::vector<std::string> &categories, const Annotation &ann, const std::array<int, 3> &color) {
	auto &bbox = ann.bbox;
	int x = static_cast<int>(bbox.at(0));
	int y = static_cast<int>(bbox.at(1));
	int w = static_cast<int>(bbox.at(2));
	int h = static_cast<int>(bbox.at(3));
	cv::rectangle(image, cv::Point2i(x, y), cv::Point2i(x + w, y + h), cv::Scalar(color[0], color[1], color[2]), this->box_width);

	int baseline = 0;
	baseline += this->box_width;

	std::string text = std::to_string(ann.id) + " " + std::to_string(ann.category_id);
	cv::Size txt_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 0.6, 1, &baseline);
	//cv::putText(image, text, cv::Point(x, y), cv::FONT_HERSHEY_SIMPLEX, 1., cv::Scalar::all(255), 1);   // ::all(255) 就是 (255, 255, 255)，就是白色
	cv::putText(image, text, cv::Point(x, y - this->box_width), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 1);
}

void DisplayUtils::overlay_mask_on_image(cv::Mat &image, const cv::Mat &gray_mask, const std::array<int, 3> &color = { 0, 0, 255 }) {

	cv::Mat mask, color_mask, masked_image, overlay_on_masked_image, background;

	cv::merge(std::array<cv::Mat, 3>{gray_mask, gray_mask, gray_mask}, mask);
	cv::bitwise_and(mask, color, color_mask);
	cv::bitwise_and(image, color_mask, masked_image);
	cv::addWeighted(masked_image, this->transparency, color_mask, 1 - this->transparency, 0, overlay_on_masked_image);

	cv::bitwise_not(color_mask, color_mask);
	cv::bitwise_and(image, color_mask, background);
	cv::add(background, overlay_on_masked_image, image);
}

cv::Mat DisplayUtils::numpy_uint8_2_mat(const py::array_t<uchar>& input_arr) {
	int c = 0;
	if (input_arr.ndim() == 2) c = CV_8UC1;
	else if (input_arr.ndim() == 3) c = CV_8UC3;
	else throw std::runtime_error("image channel is neither 3 nor 2");

	py::buffer_info buf = input_arr.request();
	cv::Mat mat(buf.shape[0], buf.shape[1], c, (unsigned char*)buf.ptr);

	return mat;
}

py::array_t<uchar> DisplayUtils::mat_2_numpy_uint8(const cv::Mat & input_mat) {
	/*  单通道的图，网上的实例代码是：就没有通道那个参数
	py::array_t<uchar> dst = py::array_t<uchar>({input_mat.rows, input_mat.cols}, input_mat.data);
	*/

	py::array_t<uchar> dst({ input_mat.rows, input_mat.cols, input_mat.channels() }, input_mat.data);
	return dst;
}

void DisplayUtils::increase_transparency() {
	this->transparency = fminf(1.0, this->transparency + 0.05);
}

void DisplayUtils::decrease_transparency() {
	this->transparency = fmaxf(0.0, this->transparency - 0.05);
}

py::array_t<uchar> DisplayUtils::draw_annotations(const py::array_t<uchar> &image_, const py::list &categories_, py::list &annotations_, const py::list &colors_) {

	// 应该只把这暴露给外界的两个函数用 pybind11 的数据，然后这里做数据转换
	// 然后传递给 private的 内部函数就用自己的数据类型，就更加清晰明确

	// python传进来的是array，先变成mat
	cv::Mat image = this->numpy_uint8_2_mat(image_);
	std::vector<std::string> categories = py::cast<std::vector<std::string>>(categories_);
	 // 要用标准容器，py::cast无法转到 cv::Scalar
	// std::vector<cv::Scalar> colors = py::cast<std::vector<cv::Scalar>>(colors_);
	std::vector<std::array<int, 3> > colors = py::cast<std::vector<std::array<int, 3> >>(colors_);

	for (size_t i = 0; i < annotations_.size(); ++i) {
		auto &&annotation_ = annotations_[i];
		Annotation annotation = {
			py::cast<unsigned long>(annotation_["id"]),
			py::cast<unsigned int>(annotation_["image_id"]),
			py::cast<unsigned int>(annotation_["category_id"]),
			py::cast<std::array<float, 4>>(annotation_["bbox"]),
			py::cast<float>(annotation_["area"]),
			py::cast<unsigned short int>(annotation_["iscrowd"]),
			py::cast<std::vector<std::vector<float>>>(annotation_["segmentation"])
		};

		this->draw_box_on_image(image, categories, annotation, colors[i]);
		cv::Mat gray_mask = this->convert_ann_to_mask(annotation, image.rows, image.cols);
		this->overlay_mask_on_image(image, gray_mask, colors[i]);
	}

	// 要返回给python，所以要mat转成array
	py::array_t<uchar> result = this->mat_2_numpy_uint8(image);
	return result;
}

py::array_t<uchar> DisplayUtils::draw_points(const py::array_t<uchar> &image_, py::list &points, py::list &labels, const int &radius) {

	std::unordered_map<float, cv::Scalar> colors = { {1., (0, 255, 0)}, {0., (0, 0, 255)} };

	cv::Mat image = this->numpy_uint8_2_mat(image_);
	for (size_t i = 0; i < points.size(); ++i) {
		std::array<int, 2> &&point = py::cast<std::array<int, 2> >(points[i]);
		const float label = py::cast<float>(labels[i]);
		cv::circle(image, cv::Point2i(point[0], point[1]), radius, colors.at(label), -1);
	}
	py::array_t<uchar> result = this->mat_2_numpy_uint8(image);
	return result;
}


// 这是写在bind.cpp中的，放这里应该也是OK
PYBIND11_MODULE(display_utils, m) {
	py::class_<DisplayUtils>(m, "DisplayUtils")
		.def(py::init<>())
		.def("increase_transparency", &DisplayUtils::increase_transparency)
		.def("decrease_transparency", &DisplayUtils::decrease_transparency)
		.def("draw_annotations", &DisplayUtils::draw_annotations)
		.def("draw_points", &DisplayUtils::draw_points, "画点击的圆点");   // 最后记得分号
}