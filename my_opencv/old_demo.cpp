//#include <iostream>
//#include <string>
//#include <opencv2/opencv.hpp>
//
//int main(int argc, char **argv) {
//	const static std::string windowName = "image";
//	cv::Mat img = cv::imread("C:/Users/Administrator/Pictures/bingbing.png");
//	if (!img.data) {
//		std::cout << "图片读取错误，检查该路径，或者Release|debug模式对应的属性中的lib库是对应的版本" << std::endl;
//		system("pasue");
//		return -1;
//	}
//
//	cv::imshow("image", img);
//	return 0;
//}


/*---------------------------------------------------------分割线,对比度--------------------------------------------------------------------------------*/

//#include <iostream>
//#include <string>
//#include <opencv2/core/core.hpp>
//#include <opencv2/highgui/highgui.hpp>
//
//static void on_ContrastAndBright(int, void *);
//
//int g_nContrastValue = 80;  // 对比度
////int g_nBrightValue = 80;   // 亮度
//int g_nBrightValue = 80;   // 亮度
//const std::string src_name = "【原始图窗口】";
//const std::string dst_name = "【效果图窗口】";
//cv::Mat g_srcImage, g_dstImage;
//
//const std::string video_path = "rtsp://192.168.108.11:554/user=admin&password=&channel=1&stream=1.sdp?";
//int main(int argc, char** argv) {
//	// 创建效果图窗口
//	cv::namedWindow(dst_name, 1);
//
//	// 创建轨迹条
//	//cv::createTrackbar("对比度: ", dst_name, &g_nContrastValue, 300, on_ContrastAndBright);
//	cv::createTrackbar("亮  度: ", dst_name, &g_nBrightValue, 200, on_ContrastAndBright);
//
//	cv::VideoCapture cap(video_path);
//	while (cap.isOpened()) {
//		bool ret = cap.read(g_srcImage);
//		if (!ret) {
//			break;
//		}
//
//		g_dstImage = cv::Mat::zeros(g_srcImage.size(), g_srcImage.type());
//		// 进行回调函数初始化
//		//on_ContrastAndBright(g_nContrastValue, 0);
//		on_ContrastAndBright(g_nBrightValue, 0);
//
//		// 显示图像
//		cv::imshow(src_name, g_srcImage);
//		cv::imshow(dst_name, g_dstImage);
//
//		//if (char(cv::waitKey(1)) == 'q') {
//		// 注意前面的 & 运算要用括号括起来 
//		if ((cv::waitKey(1) & 0xff) != 255) {
//			break;
//		}
//	}
//
//	cap.release();
//	cv::destroyAllWindows();
//
//	return 0;
//}
//
//static void on_ContrastAndBright(int, void *) {
//	// 改成0，图像可让拉拽缩放(但可能一开始图像的显示就不那么完全)
//	cv::namedWindow(src_name, 1);
//	// 3个for循环，执行运算g_dstImage(i, j) = a*g_srcImage(i,j) + b
//	for (int y = 0; y < g_srcImage.rows; ++y) {
//		for (int x = 0; x < g_srcImage.cols; ++x) {
//			for (int c = 0; c < 3; ++c) {
//				g_dstImage.at<cv::Vec3b>(y, x)[c] =
//					cv::saturate_cast<uchar> ((g_nContrastValue*0.01) * (g_srcImage.at<cv::Vec3b>(y, x)[c]) + g_nBrightValue);
//				// 上面这个函数是用于溢出保护，大致是if(data<0) data=0; else if(data>255) data=255;
//			}
//		}
//	}
//}




/* ------------------ camshift ----------------------------------------- */

//#include "opencv2/core/utility.hpp"
//#include "opencv2/video/tracking.hpp"
//#include "opencv2/imgproc.hpp"
//#include "opencv2/videoio.hpp"
//#include "opencv2/highgui.hpp"
//
//#include <iostream>
//#include <ctype.h>
//
//using namespace cv;
//using namespace std;
//
//Mat image;
//
//bool backprojMode = false;
//bool selectObject = false;
//int trackObject = 0;
//bool showHist = true;
//Point origin;
//Rect selection;
//int vmin = 10, vmax = 256, smin = 30;
//
//// User draws box around object to track. This triggers CAMShift to start tracking
//static void onMouse(int event, int x, int y, int, void*)
//{
//	if (selectObject) {
//		selection.x = MIN(x, origin.x);
//		selection.y = MIN(y, origin.y);
//		selection.width = std::abs(x - origin.x);
//		selection.height = std::abs(y - origin.y);
//
//		selection &= Rect(0, 0, image.cols, image.rows);
//	}
//
//	switch (event) {
//	case EVENT_LBUTTONDOWN:
//		origin = Point(x, y);
//		selection = Rect(x, y, 0, 0);
//		selectObject = true;
//		break;
//	case EVENT_LBUTTONUP:
//		selectObject = false;
//		if (selection.width > 0 && selection.height > 0)
//			trackObject = -1;   // Set up CAMShift properties in main() loop
//		break;
//	}
//}
//
//string hot_keys =
//"\n\nHot keys: \n"
//"\tESC - quit the program\n"
//"\tc - stop the tracking\n"
//"\tb - switch to/from backprojection view\n"
//"\th - show/hide object histogram\n"
//"\tp - pause video\n"
//"To initialize tracking, select the object with mouse\n";
//
//static void help(const char** argv)
//{
//	cout << "\nThis is a demo that shows mean-shift based tracking\n"
//		"You select a color objects such as your face and it tracks it.\n"
//		"This reads from video camera (0 by default, or the camera number the user enters\n"
//		"Usage: \n\t";
//	cout << argv[0] << " [camera number]\n";
//	cout << hot_keys;
//}
//
//const char* keys =
//{
//	"{help h | | show help message}{@camera_number| 0 | camera number}"
//};
//
//int main(int argc, const char** argv)
//{
//	VideoCapture cap;
//	Rect trackWindow;
//	int hsize = 16;
//	float hranges[] = { 0,180 };
//	const float* phranges = hranges;
//	CommandLineParser parser(argc, argv, keys);
//	if (parser.has("help")) {
//		help(argv);
//		return 0;
//	}
//	int camNum = parser.get<int>(0);
//	cap.open(camNum);
//
//	if (!cap.isOpened()) {
//		help(argv);
//		cout << "***Could not initialize capturing...***\n";
//		cout << "Current parameter's value: \n";
//		parser.printMessage();
//		return -1;
//	}
//	cout << hot_keys;
//	namedWindow("Histogram", 0);
//	namedWindow("CamShift Demo", 0);
//	setMouseCallback("CamShift Demo", onMouse, 0);
//	createTrackbar("Vmin", "CamShift Demo", &vmin, 256, 0);
//	createTrackbar("Vmax", "CamShift Demo", &vmax, 256, 0);
//	createTrackbar("Smin", "CamShift Demo", &smin, 256, 0);
//
//	Mat frame, hsv, hue, mask, hist, histimg = Mat::zeros(200, 320, CV_8UC3), backproj;
//	bool paused = false;
//
//	for (;;) {
//		if (!paused) {
//			cap >> frame;
//			if (frame.empty())
//				break;
//		}
//
//		frame.copyTo(image);
//
//		if (!paused) {
//			cvtColor(image, hsv, COLOR_BGR2HSV);
//
//			if (trackObject) {
//				int _vmin = vmin, _vmax = vmax;
//
//				inRange(hsv, Scalar(0, smin, MIN(_vmin, _vmax)),
//					Scalar(180, 256, MAX(_vmin, _vmax)), mask);
//				int ch[] = { 0, 0 };
//				hue.create(hsv.size(), hsv.depth());
//				mixChannels(&hsv, 1, &hue, 1, ch, 1);
//
//				if (trackObject < 0) {
//					// Object has been selected by user, set up CAMShift search properties once
//					Mat roi(hue, selection), maskroi(mask, selection);
//					calcHist(&roi, 1, 0, maskroi, hist, 1, &hsize, &phranges);
//					normalize(hist, hist, 0, 255, NORM_MINMAX);
//
//					trackWindow = selection;
//					trackObject = 1; // Don't set up again, unless user selects new ROI
//
//					histimg = Scalar::all(0);
//					int binW = histimg.cols / hsize;
//					Mat buf(1, hsize, CV_8UC3);
//					for (int i = 0; i < hsize; i++)
//						buf.at<Vec3b>(i) = Vec3b(saturate_cast<uchar>(i*180. / hsize), 255, 255);
//					cvtColor(buf, buf, COLOR_HSV2BGR);
//
//					for (int i = 0; i < hsize; i++) {
//						int val = saturate_cast<int>(hist.at<float>(i)*histimg.rows / 255);
//						rectangle(histimg, Point(i*binW, histimg.rows),
//							Point((i + 1)*binW, histimg.rows - val),
//							Scalar(buf.at<Vec3b>(i)), -1, 8);
//					}
//				}
//
//				// Perform CAMShift
//				calcBackProject(&hue, 1, 0, hist, backproj, &phranges);
//				backproj &= mask;
//				RotatedRect trackBox = CamShift(backproj, trackWindow,
//					TermCriteria(TermCriteria::EPS | TermCriteria::COUNT, 10, 1));
//				if (trackWindow.area() <= 1) {
//					int cols = backproj.cols, rows = backproj.rows, r = (MIN(cols, rows) + 5) / 6;
//					trackWindow = Rect(trackWindow.x - r, trackWindow.y - r,
//						trackWindow.x + r, trackWindow.y + r) &
//						Rect(0, 0, cols, rows);
//				}
//
//				if (backprojMode)
//					cvtColor(backproj, image, COLOR_GRAY2BGR);
//				ellipse(image, trackBox, Scalar(0, 0, 255), 3, LINE_AA);
//			}
//		}
//		else if (trackObject < 0)
//			paused = false;
//
//		if (selectObject && selection.width > 0 && selection.height > 0) {
//			Mat roi(image, selection);
//			bitwise_not(roi, roi);
//		}
//
//		imshow("CamShift Demo", image);
//		imshow("Histogram", histimg);
//
//		char c = (char)waitKey(10);
//		if (c == 27)
//			break;
//		switch (c) {
//		case 'b':
//			backprojMode = !backprojMode;
//			break;
//		case 'c':
//			trackObject = 0;
//			histimg = Scalar::all(0);
//			break;
//		case 'h':
//			showHist = !showHist;
//			if (!showHist)
//				destroyWindow("Histogram");
//			else
//				namedWindow("Histogram", 1);
//			break;
//		case 'p':
//			paused = !paused;
//			break;
//		default:
//			;
//		}
//	}
//
//	cap.release();
//	destroyAllWindows();
//	return 0;
//}

