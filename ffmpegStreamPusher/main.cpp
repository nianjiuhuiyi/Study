#include <iostream>
#include "StreamPusher.h"


int main(int argc, char* argv[]) {
	srand((int)time(NULL));
	//ffmpeg RTSP推流 https://www.jianshu.com/p/a9c7b08be46e
	//ffmpeg 推流参数  https://blog.csdn.net/qq_17368865/article/details/79101659

	const char* srcUrl = "rtsp://192.168.108.134:554/user=admin&password=&channel=1&stream=1.sdp?";
	//const char* srcUrl = "C:\\Users\\Administrator\\Videos\\source.200kbps.768x320.flv";

	//const char* dstUrl = "rtmp://192.168.125.128/live/456";
	//const char* dstUrl = "rtsp://localhost/test";
	const char* dstUrl = "rtmp://192.168.108.218/live/123";

	int dstVideoFps = 20;
	int dstVideoWidth = 800;
	int dstVideoHeight = 448;      // 副码流
	//int dstVideoWidth = 1920;         
	//int dstVideoHeight = 1080;

	StreamPusher pusher(srcUrl, dstUrl, dstVideoWidth, dstVideoHeight, dstVideoFps);
	pusher.start();

    std::cout << "Hello World!\n";
	system("pause");
	return 0;
}
