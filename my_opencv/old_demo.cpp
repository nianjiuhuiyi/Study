/*      ------------------------ 1���򵥶�ȡͼƬ----------------------------------
#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>

int main(int argc, char **argv) {
	const static std::string windowName = "image";
	cv::Mat img = cv::imread("C:/Users/Administrator/Pictures/bingbing.png");
	if (!img.data) {
		std::cout << "ͼƬ��ȡ���󣬼���·��������Release|debugģʽ��Ӧ�������е�lib���Ƕ�Ӧ�İ汾" << std::endl;
		system("pasue");
		return -1;
	}

	cv::imshow("image", img);
	return 0;
}
*/



/*  ---------------------- 2����д���ʼǣ���01. �������켣����GUI�Ĳ����޸ġ� ---------------------------

#include <iostream>
#include <string>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

static void on_ContrastAndBright(int, void *);

int g_nContrastValue = 80;  // �Աȶ�
//int g_nBrightValue = 80;   // ����
int g_nBrightValue = 80;   // ����
const std::string src_name = "��ԭʼͼ���ڡ�";
const std::string dst_name = "��Ч��ͼ���ڡ�";
cv::Mat g_srcImage, g_dstImage;

const std::string video_path = "rtsp://192.168.108.11:554/user=admin&password=&channel=1&stream=1.sdp?";
int main(int argc, char** argv) {
	// ����Ч��ͼ����
	cv::namedWindow(dst_name, 1);

	// �����켣��
	//cv::createTrackbar("�Աȶ�: ", dst_name, &g_nContrastValue, 300, on_ContrastAndBright);
	cv::createTrackbar("��  ��: ", dst_name, &g_nBrightValue, 200, on_ContrastAndBright);

	cv::VideoCapture cap(video_path);
	while (cap.isOpened()) {
		bool ret = cap.read(g_srcImage);
		if (!ret) {
			break;
		}

		g_dstImage = cv::Mat::zeros(g_srcImage.size(), g_srcImage.type());
		// ���лص�������ʼ��
		//on_ContrastAndBright(g_nContrastValue, 0);
		on_ContrastAndBright(g_nBrightValue, 0);

		// ��ʾͼ��
		cv::imshow(src_name, g_srcImage);
		cv::imshow(dst_name, g_dstImage);

		//if (char(cv::waitKey(1)) == 'q') {
		// ע��ǰ��� & ����Ҫ������������
		if ((cv::waitKey(1) & 0xff) != 255) {
			break;
		}
	}

	cap.release();
	cv::destroyAllWindows();

	return 0;
}

static void on_ContrastAndBright(int, void *) {
	// �ĳ�0��ͼ�������ק����(������һ��ʼͼ�����ʾ�Ͳ���ô��ȫ)
	cv::namedWindow(src_name, 1);
	// 3��forѭ����ִ������g_dstImage(i, j) = a*g_srcImage(i,j) + b
	for (int y = 0; y < g_srcImage.rows; ++y) {
		for (int x = 0; x < g_srcImage.cols; ++x) {
			for (int c = 0; c < 3; ++c) {
				g_dstImage.at<cv::Vec3b>(y, x)[c] =
					cv::saturate_cast<uchar> ((g_nContrastValue*0.01) * (g_srcImage.at<cv::Vec3b>(y, x)[c]) + g_nBrightValue);
				// ��������������������������������if(data<0) data=0; else if(data>255) data=255;
			}
		}
	}
}

*/


/*

--------------------- 3������Python�Ķ��̶߳�ȡ����ͷд�ģ��Ѿ�д���ʼ��� --------------------

#include <queue>
#include <thread>
#include <chrono>
#include <iostream>
#include <string>
#include <memory>
#include <mutex>
#include <opencv2/opencv.hpp>

void func() {
	cv::Mat image = cv::imread(R"(C:\Users\Administrator\Pictures\01.png)");

	cv::Mat res;
	cv::Rect rect(10, 10, 400, 400);
	cv::resize(image(rect), res, cv::Size(400, 400));

	cv::imshow("1", res);
	cv::waitKey(0);
	cv::destroyAllWindows();
}



std::mutex mtx;   // ������֤�̰߳�ȫ
bool gExit = false;

std::string get_timestamp() {
	// std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
	// std::time_t time = ms.count();
	// char str[100];
	// std::strftime(str, sizeof(str), "%Y-%m-%d %H:%M:%S", std::localtime(&time));

	std::tm time_info{};
	std::time_t timestamp = std::time(nullptr);
	errno_t err = localtime_s(&time_info, &timestamp);
	if (err) {
		std::cout << "Failed to convert timestamp to time\n";
		return std::string("error");
	}
	char str[100]{};
	std::strftime(str, sizeof(str), "%Y-%m-%d %H:%M:%S", &time_info);
	return std::string(str);
}

// �����������������ָ�룬����Ĳ���ҲҪ������ָ�룬������ std::queue<cv::Mat>* que
void get_image(const char* rtsp_path, std::shared_ptr<std::queue<cv::Mat>> que, int video_stride=15) {
	cv::VideoCapture cap;
	cap.open(rtsp_path);
	while (1) {
		if (cap.isOpened()) break;
		else {
			std::cerr << rtsp_path << " ��ʧ�ܣ���������..." << std::endl;
			cap.open(rtsp_path);
		}
	}

	long long int n = 0;
	while (1) {
		n += 1;
		bool success = cap.grab();     // .read() = .grab() followed by .retrieve()
		if (!success) {
			while (1) {
				bool ret = cap.open(rtsp_path);
				if (ret) break;
				else {
					std::cerr << get_timestamp() <<": ����ͷ��ȡʧ�ܣ�30����ٴγ���..." << std::endl;
					std::this_thread::sleep_for(std::chrono::seconds(30));
				}
			}
			continue;
		}

		if (n % video_stride != 0) continue;

		cv::Mat frame;
		success = cap.retrieve(frame);
		if (!success) {
			while (1) {
				bool ret = cap.open(rtsp_path);
				if (ret) break;
				else {
					std::cerr << get_timestamp() << ": ����ͷ��ȡʧ�ܣ�30����ٴγ���..." << std::endl;
					std::this_thread::sleep_for(std::chrono::seconds(30));
				}
			}
			continue;
		}

		// ���������֤�̰߳�ȫ
		std::unique_lock<std::mutex> lock(mtx);
		if (que->size() >= 2)
			que->pop();
		que->push(frame);
		lock.unlock();

		std::cout << "���ڵ�size: " << que->size() << std::endl;
		if (gExit) break;
	}
	cap.release();
	std::cout << "���߳��˳�" << std::endl;
}


int main(int argc, char** argv) {
	const char* video_path = "rtsp://192.168.108.131:554/user=admin&password=&channel=1&stream=0.sdp?";
	cv::namedWindow("hello");

	// ͼ�����
	std::shared_ptr<std::queue<cv::Mat>> que = std::make_shared<std::queue<cv::Mat>>();
	// ���治�����ַ�ʽ������������Ĭ�ϲ�����Ҳһ��Ҫ��Ĭ�ϲ�����ֵ����Ȼ���Ҳ�����Ӧ�����ġ�
	// ��ʽһ��ʹ�� bind ��
	// auto f = std::bind(get_image, video_path, que, 15);
	// std::thread img_thread(f);

	// ��ʽ����ʹ��lambda
	// std::thread img_thread([video_path, que]() {get_image(video_path, que, 1); });  // ֵ����
	// std::thread img_thread([&video_path, &que]() {get_image(video_path, que, 1); });  // ���ò���

	std::thread img_thread([&]() {get_image(video_path, que, 1); });  // ��Ҳ�����ò���
	std::this_thread::sleep_for(std::chrono::seconds(1));

	while (true) {
		if (que && !que->empty()) {
			std::cout << "123" << std::endl;
			cv::Mat frame;

			// ���������֤�̰߳�ȫ
			std::unique_lock<std::mutex> lock(mtx);
			frame = que->front();
			que->pop();
			lock.unlock();

			// ģ������ʱ
			std::this_thread::sleep_for(std::chrono::milliseconds(200));

			cv::imshow("hello", frame);
			if ((cv::waitKey(1) & 0xFF) != 255) {
				gExit = true;
				break;
			}
		}
		else {
			std::cout << get_timestamp() << "pause" << std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

	}
	std::this_thread::sleep_for(std::chrono::seconds(1));

	cv::destroyAllWindows();

	//cv::VideoCapture cap;
	//cap.open(video_path);
	//cv::Mat frame;

	//while (cap.isOpened()) {
	//	bool ret = cap.read(frame);
	//	if (!ret) break;
	//	cap >> frame;
	//	que->push(frame);
	//	std::cout << frame.size << typeid(frame.size).name() << std::endl;
	//	std::cout << frame.size() << typeid(frame.size()).name() << "\n" << std::endl;

	//	cv::imshow("hello", frame);
	//	if ((cv::waitKey(1) & 0xFF) != 255) break;
	//}
	//cv::destroyAllWindows();
	//cap.release();
	return 0;
}

*/



/* ------------------ 4��camshift����ûд���ʼǣ����ִ��������ҵģ� ------------

#include "opencv2/core/utility.hpp"
#include "opencv2/video/tracking.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/videoio.hpp"
#include "opencv2/highgui.hpp"

#include <iostream>
#include <ctype.h>

using namespace cv;
using namespace std;

Mat image;

bool backprojMode = false;
bool selectObject = false;
int trackObject = 0;
bool showHist = true;
Point origin;
Rect selection;
int vmin = 10, vmax = 256, smin = 30;

// User draws box around object to track. This triggers CAMShift to start tracking
static void onMouse(int event, int x, int y, int, void*)
{
	if (selectObject) {
		selection.x = MIN(x, origin.x);
		selection.y = MIN(y, origin.y);
		selection.width = std::abs(x - origin.x);
		selection.height = std::abs(y - origin.y);

		selection &= Rect(0, 0, image.cols, image.rows);
	}

	switch (event) {
	case EVENT_LBUTTONDOWN:
		origin = Point(x, y);
		selection = Rect(x, y, 0, 0);
		selectObject = true;
		break;
	case EVENT_LBUTTONUP:
		selectObject = false;
		if (selection.width > 0 && selection.height > 0)
			trackObject = -1;   // Set up CAMShift properties in main() loop
		break;
	}
}

string hot_keys =
"\n\nHot keys: \n"
"\tESC - quit the program\n"
"\tc - stop the tracking\n"
"\tb - switch to/from backprojection view\n"
"\th - show/hide object histogram\n"
"\tp - pause video\n"
"To initialize tracking, select the object with mouse\n";

static void help(const char** argv)
{
	cout << "\nThis is a demo that shows mean-shift based tracking\n"
		"You select a color objects such as your face and it tracks it.\n"
		"This reads from video camera (0 by default, or the camera number the user enters\n"
		"Usage: \n\t";
	cout << argv[0] << " [camera number]\n";
	cout << hot_keys;
}

const char* keys =
{
	"{help h | | show help message}{@camera_number| 0 | camera number}"
};

int main(int argc, const char** argv)
{
	VideoCapture cap;
	Rect trackWindow;
	int hsize = 16;
	float hranges[] = { 0,180 };
	const float* phranges = hranges;
	CommandLineParser parser(argc, argv, keys);
	if (parser.has("help")) {
		help(argv);
		return 0;
	}
	int camNum = parser.get<int>(0);
	cap.open(camNum);

	if (!cap.isOpened()) {
		help(argv);
		cout << "***Could not initialize capturing...***\n";
		cout << "Current parameter's value: \n";
		parser.printMessage();
		return -1;
	}
	cout << hot_keys;
	namedWindow("Histogram", 0);
	namedWindow("CamShift Demo", 0);
	setMouseCallback("CamShift Demo", onMouse, 0);
	createTrackbar("Vmin", "CamShift Demo", &vmin, 256, 0);
	createTrackbar("Vmax", "CamShift Demo", &vmax, 256, 0);
	createTrackbar("Smin", "CamShift Demo", &smin, 256, 0);

	Mat frame, hsv, hue, mask, hist, histimg = Mat::zeros(200, 320, CV_8UC3), backproj;
	bool paused = false;

	for (;;) {
		if (!paused) {
			cap >> frame;
			if (frame.empty())
				break;
		}

		frame.copyTo(image);

		if (!paused) {
			cvtColor(image, hsv, COLOR_BGR2HSV);

			if (trackObject) {
				int _vmin = vmin, _vmax = vmax;

				inRange(hsv, Scalar(0, smin, MIN(_vmin, _vmax)),
					Scalar(180, 256, MAX(_vmin, _vmax)), mask);
				int ch[] = { 0, 0 };
				hue.create(hsv.size(), hsv.depth());
				mixChannels(&hsv, 1, &hue, 1, ch, 1);

				if (trackObject < 0) {
					// Object has been selected by user, set up CAMShift search properties once
					Mat roi(hue, selection), maskroi(mask, selection);
					calcHist(&roi, 1, 0, maskroi, hist, 1, &hsize, &phranges);
					normalize(hist, hist, 0, 255, NORM_MINMAX);

					trackWindow = selection;
					trackObject = 1; // Don't set up again, unless user selects new ROI

					histimg = Scalar::all(0);
					int binW = histimg.cols / hsize;
					Mat buf(1, hsize, CV_8UC3);
					for (int i = 0; i < hsize; i++)
						buf.at<Vec3b>(i) = Vec3b(saturate_cast<uchar>(i*180. / hsize), 255, 255);
					cvtColor(buf, buf, COLOR_HSV2BGR);

					for (int i = 0; i < hsize; i++) {
						int val = saturate_cast<int>(hist.at<float>(i)*histimg.rows / 255);
						rectangle(histimg, Point(i*binW, histimg.rows),
							Point((i + 1)*binW, histimg.rows - val),
							Scalar(buf.at<Vec3b>(i)), -1, 8);
					}
				}

				// Perform CAMShift
				calcBackProject(&hue, 1, 0, hist, backproj, &phranges);
				backproj &= mask;
				RotatedRect trackBox = CamShift(backproj, trackWindow,
					TermCriteria(TermCriteria::EPS | TermCriteria::COUNT, 10, 1));
				if (trackWindow.area() <= 1) {
					int cols = backproj.cols, rows = backproj.rows, r = (MIN(cols, rows) + 5) / 6;
					trackWindow = Rect(trackWindow.x - r, trackWindow.y - r,
						trackWindow.x + r, trackWindow.y + r) &
						Rect(0, 0, cols, rows);
				}

				if (backprojMode)
					cvtColor(backproj, image, COLOR_GRAY2BGR);
				ellipse(image, trackBox, Scalar(0, 0, 255), 3, LINE_AA);
			}
		}
		else if (trackObject < 0)
			paused = false;

		if (selectObject && selection.width > 0 && selection.height > 0) {
			Mat roi(image, selection);
			bitwise_not(roi, roi);
		}

		imshow("CamShift Demo", image);
		imshow("Histogram", histimg);

		char c = (char)waitKey(10);
		if (c == 27)
			break;
		switch (c) {
		case 'b':
			backprojMode = !backprojMode;
			break;
		case 'c':
			trackObject = 0;
			histimg = Scalar::all(0);
			break;
		case 'h':
			showHist = !showHist;
			if (!showHist)
				destroyWindow("Histogram");
			else
				namedWindow("Histogram", 1);
			break;
		case 'p':
			paused = !paused;
			break;
		default:
			;
		}
	}

	cap.release();
	destroyAllWindows();
	return 0;
}

*/

