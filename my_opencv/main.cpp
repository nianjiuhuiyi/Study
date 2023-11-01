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


int main(int argc, char** argv) {
	const char* video_path = "rtsp://192.168.108.134:554/user=admin&password=&channel=1&stream=1.sdp?";

	cv::namedWindow("hello");
	cv::VideoCapture cap;
	cap.open(video_path);

	cv::Mat frame;
	while (cap.isOpened()) {
		bool ret = cap.read(frame);
		cap >> frame;
		std::cout << frame.size << typeid(frame.size).name() << std::endl;
		std::cout << frame.size() << typeid(frame.size()).name() << "\n" << std::endl;

		if (!ret) break;
		cv::imshow("hello", frame);
		if ((cv::waitKey(1) & 0xFF) != 255) break;
	}
	cv::destroyAllWindows();
	cap.release();
	return 0;
}