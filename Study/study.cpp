#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>
#include <chrono>
#include <opencv2/opencv.hpp>
#include <fstream>
#include <chrono>
#include <cassert>
#include <gdb.h>

#include "cvui.hpp"

#include <tuple>


typedef struct _MV_PIXEL_CONVERT_PARAM_T_ {
	unsigned short      nWidth;             ///< [IN]     \~chinese 图像宽           \~english Width
	unsigned short      nHeight;            ///< [IN]     \~chinese 图像高\~english Height

	unsigned char*      pSrcData;           ///< [IN]     \~chinese 输入数据缓存\~english Input data buffer
	unsigned int        nSrcDataLen;        ///< [IN]     \~chinese 输入数据大小\~english Input data size

	unsigned char*      pDstBuffer;         ///< [OUT]    \~chinese 输出数据缓存\~english Output data buffer
	unsigned int        nDstLen;            ///< [OUT]    \~chinese 输出数据大小\~english Output data size
	unsigned int        nDstBufferSize;     ///< [IN]     \~chinese 提供的输出缓冲区大小\~english Provided outbut buffer size

	unsigned int        nRes[4];
	std::string         name;
	char*               new_name[5];
}MV_CC_PIXEL_CONVERT_PARAM;



#define WINDOW_NAME "CVUI Hello World!"

void gui_show() {
	cv::Mat lena = cv::imread("F:\\learnopencv\\learnopencv-master\\UI-cvui\\lena.jpg");
	cv::Mat frame = lena.clone();
	int low_threshold = 50, high_threshold = 150;
	bool use_canny = false;

	// Init a OpenCV window and tell cvui to use it.
	// If cv::namedWindow() is not used, mouse events will
	// not be captured by cvui.
	cv::namedWindow(WINDOW_NAME);
	cvui::init(WINDOW_NAME);

	while (true) {
		// Should we apply Canny edge?
		if (use_canny) {
			// Yes, we should apply it.
			cv::cvtColor(lena, frame, cv::COLOR_BGR2GRAY);
			cv::Canny(frame, frame, low_threshold, high_threshold, 3);
		}
		else {
			// No, so just copy the original image to the displaying frame.
			lena.copyTo(frame);
		}

		// Render the settings window to house the checkbox
		// and the trackbars below.
		cvui::window(frame, 10, 50, 180, 180, "Settings");

		// Checkbox to enable/disable the use of Canny edge
		cvui::checkbox(frame, 15, 80, "Use Canny Edge", &use_canny);

		// Two trackbars to control the low and high threshold values
		// for the Canny edge algorithm.
		cvui::trackbar(frame, 15, 110, 165, &low_threshold, 5, 150);
		cvui::trackbar(frame, 15, 180, 165, &high_threshold, 80, 300);

		// This function must be called *AFTER* all UI components. It does
		// all the behind the scenes magic to handle mouse clicks, etc.
		cvui::update();

		// Show everything on the screen
		cv::imshow(WINDOW_NAME, frame);

		// Check if ESC was pressed
		if (cv::waitKey(30) == 27) {
			break;
		}
	}
	cv::destroyAllWindows();
}


int main() {
	/*std::thread t1(prodecer);
	std::thread t2(consumer);
	t1.join();
	t2.join();
	--------------------------------------------------
	std::string num = "3.14";
	std::cout << (num.data() == num.c_str()) << std::endl;
	*/



	{
		std::cout << 123 << std::endl;
		unsigned char* data = nullptr;

		//MV_CC_PIXEL_CONVERT_PARAM mydata = {0};
		//mydata.nWidth = 12;
		//std::cout << mydata.nWidth << std::endl;
		//std::cout << mydata.new_name << std::endl;
	}

	/*
	已写进笔记中
	// 1、
	const int a = 2;
	const int b = gdb(3 * a) + 1;  // [example.cpp:18 (main)] 3 * a = 6 (int32_t)

	// 2、
	std::vector<int> numbers{ b, 13, 42 };
	gdb(numbers);  // [example.cpp:21 (main)] numbers = {7, 13, 42} (std::vector<int32_t>)

	// 3、在一个表达式中
	my_func(3);

	// 4、获取当前时间(比较直接简单，获取时间戳和用时还是用笔记里另外的)
	gdb(gdb::time());

	// 5、多个目标（像中间中记得用括号括起来）
	gdb(42, (std::vector<int>{2, 3, 4}), "hello", false);
	*/

	/*
	已写进笔记中
	//std::vector< std::string> images;
	//cv::glob("C:\\Users\\Administrator\\Desktop\\under\\images", images);
	//gdb(images);
	//cv::Mat img = cv::imread(images.at(0));
	//cv::imshow("12", img);
	//cv::waitKey(0);
	*/

	// opencv做的简单的gui界面，已写进笔记中
	//gui_show();

	//uint8_t points[5][2] = { {60, 60}, {40, 10}, {100, 100}, {200, 60}, {300, 50} };

	//std::vector<cv::Point> points = { {60, 60}, {40, 10}, {100, 100}, {200, 60}, {300, 50} };
	//cv::Mat img = cv::Mat::ones(cv::Size(640, 640), CV_8UC3);
	//cv::rectangle(img, cv::Point(10, 10), cv::Point(60, 60), cv::Scalar(0, 0, 255), 2);
	//cv::fillPoly(img, points, cv::Scalar(0, 0, 255));
	//cv::imshow("123", img);
	//cv::waitKey(0);
	//cv::destroyAllWindows();

	using detectType = std::tuple<std::string, float, std::vector<int>>;

	std::vector<detectType> vec;
	vec.push_back({ "YBKC", 0.5, {1, 2, 3, 4} });
	vec.push_back({ "XDSC", 0.78, {4, 5, 6, 7} });

	// 方式一：
	//std::string name;
	//float conf;
	//std::vector<int> position;

	//for (detectType& item : vec) {
	//	std::tie(name, conf, position) = item;
	//	std::cout << name << ": " << conf << std::endl;
	//}

	// 方式二：
	for (detectType& item : vec) {
		std::string name = std::get<0>(item);
		float conf = std::get<1>(item);
		std::vector<int> position = std::get<2>(item);
		std::cout << name << ": " << conf << std::endl;
	}

	system("pause");
	return 0;
}


