#include <array>
#include <vector>
#include <unordered_map>
#include <opencv2/opencv.hpp>

struct Annotation {
	unsigned long int id;
	unsigned int image_id;
	unsigned int category_id;
	std::array<float, 4> bbox;
	float area;
	unsigned short int iscrow;
	std::vector<std::vector<float> > segmentation;
};


class DisplayUtils {
private:
	float transparency = 0.3f;
	int box_width = 3;

private:
	cv::Mat convert_ann_to_mask(Annotation &ann, const int &height, int &width);
	void draw_box_on_image(cv::Mat &image, const std::vector<std::string> &categories, const Annotation &ann, const cv::Scalar &color);
	void overlay_mask_on_image(cv::Mat &image, const cv::Mat &gray_mask, const cv::Scalar &color);

public:
	DisplayUtils() = default;

	void increase_transparency();
	void decrease_transparency();

	cv::Mat draw_annotations(const cv::Mat &image, const std::vector<std::string> &categories, std::vector<Annotation> &annotations, const std::vector<cv::Scalar> &colors);
	cv::Mat draw_points(const cv::Mat &image, std::vector<std::array<int, 2> > &points, std::vector<float> &labels, const std::unordered_map<float, cv::Scalar> &colors, const int &radius);
};
