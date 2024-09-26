#include <iostream>
#include "StreamPusher.h"


int main(int argc, char* argv[]) {
	srand((int)time(NULL));
	//ffmpeg RTSP推流 https://www.jianshu.com/p/a9c7b08be46e
	//ffmpeg 推流参数  https://blog.csdn.net/qq_17368865/article/details/79101659

	const char* srcUrl = "rtsp://192.168.108.135:554/user=admin&password=&channel=1&stream=1.sdp?";
	// const char* srcUrl = "C:\\Users\\Administrator\\Videos\\sintel_trailer-480p.webm";

	const char* dstUrl = "rtmp://192.168.108.218/live/123";

	int dstVideoFps = 20;
	int dstVideoWidth = 800;
	int dstVideoHeight = 448;      // 副码流
	//int dstVideoWidth = 1920;         
	//int dstVideoHeight = 1080;

	StreamPusher pusher(srcUrl, dstUrl, dstVideoWidth, dstVideoHeight, dstVideoFps);
	pusher.start();

	return 0;
}
