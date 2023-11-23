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



std::mutex mtx;   // 加锁保证线程安全
bool gExit = false;

std::string get_timestamp() {
	/*std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
	std::time_t time = ms.count();
	char str[100];
	std::strftime(str, sizeof(str), "%Y-%m-%d %H:%M:%S", std::localtime(&time));*/

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

// 如果传进来的是智能指针，这里的参数也要给智能指针，不能是 std::queue<cv::Mat>* que
void get_image(const char* rtsp_path, std::shared_ptr<std::queue<cv::Mat>> que, int video_stride=15) {
	cv::VideoCapture cap;
	cap.open(rtsp_path);
	while (1) {
		if (cap.isOpened()) break;
		else {
			std::cerr << rtsp_path << " 打开失败，正在重试..." << std::endl;
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
					std::cerr << get_timestamp() <<": 摄像头读取失败，30秒后再次尝试..." << std::endl;
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
					std::cerr << get_timestamp() << ": 摄像头读取失败，30秒后再次尝试..." << std::endl;
					std::this_thread::sleep_for(std::chrono::seconds(30));
				}
			}
			continue;
		}

		// 必须加锁保证线程安全
		std::unique_lock<std::mutex> lock(mtx);
		if (que->size() >= 2)
			que->pop();
		que->push(frame);
		lock.unlock();

		std::cout << "现在的size: " << que->size() << std::endl;
		if (gExit) break;
	}
	cap.release();
	std::cout << "子线程退出" << std::endl;
}


int main(int argc, char** argv) {
	const char* video_path = "rtsp://192.168.108.131:554/user=admin&password=&channel=1&stream=0.sdp?";
	cv::namedWindow("hello");

	// 图像队列
	std::shared_ptr<std::queue<cv::Mat>> que = std::make_shared<std::queue<cv::Mat>>();
	// 下面不管哪种方式，函数哪怕有默认参数，也一定要给默认参数的值，不然是找不到对应函数的。
	/*方式一：使用 bind 绑定
	auto f = std::bind(get_image, video_path, que, 15);
	std::thread img_thread(f);
	*/
	/*方式二：使用lambda
	std::thread img_thread([video_path, que]() {get_image(video_path, que, 1); });  // 值捕获
	std::thread img_thread([&video_path, &que]() {get_image(video_path, que, 1); });  // 引用捕获，
	*/
	std::thread img_thread([&]() {get_image(video_path, que, 1); });  // 这也是引用捕获
	std::this_thread::sleep_for(std::chrono::seconds(1));
	

	while (true) {

		if (que && !que->empty()) {
			std::cout << "123" << std::endl;
			cv::Mat frame;

			// 必须加锁保证线程安全
			std::unique_lock<std::mutex> lock(mtx);
			frame = que->front();
			que->pop();
			lock.unlock();

			// 模拟检测用时
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