#include <opencv2/opencv.hpp>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>

namespace py = pybind11;


struct Annotation {
	unsigned long int id;
	unsigned int image_id;
	unsigned int category_id;
	std::array<float, 4> bbox;
	float area;
	unsigned short int iscrowd;
	std::vector<std::vector<float> > segmentation;
};


class DisplayUtils {
private:
	float transparency = 0.3f;
	int box_width = 3;

private:
	cv::Mat convert_ann_to_mask(Annotation &ann, const int &height, int &width);
	void draw_box_on_image(cv::Mat &image, const std::vector<std::string> &categories, const Annotation &ann, const std::array<int, 3> &color);
	void overlay_mask_on_image(cv::Mat &image, const cv::Mat &gray_mask, const std::array<int, 3> &color);

	// 写俩函数来转换python传进来类型为array的图像到CV的Mat，以及这个拟操作
	cv::Mat numpy_uint8_2_mat(const py::array_t<uchar> &input_arr);
	py::array_t<uchar> mat_2_numpy_uint8(const cv::Mat &input_mat);

public:
	DisplayUtils() = default;

	void increase_transparency();
	void decrease_transparency();

	// 本来是返回cv::Mat，得改回到 py::array_t<uchar>
	py::array_t<uchar> draw_annotations(const py::array_t<uchar> &image_, const py::list &categories_, py::list &annotations_, const py::list &colors_);
	py::array_t<uchar> draw_points(const py::array_t<uchar> &image, py::list &points, py::list &labels, const int &radius);
};
