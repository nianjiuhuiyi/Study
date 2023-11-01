#ifndef STREAMPUSHER_H
#define STREAMPUSHER_H

#include <stdio.h>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

class StreamPusher {
public:
	explicit StreamPusher(const char* srcUrl, const char* dstUrl, int dstVideoWidth, int dstVideoHeight, int dstVideoFps);
	StreamPusher() = delete;
	~StreamPusher();
public:
	void start();
private:
	bool connectSrc();
	void closeConnectSrc();
	bool connectDst();
	void closeConnectDst();

private:
	std::string mSrcUrl;
	std::string mDstUrl;

	// 源头
	AVFormatContext *mSrcFmtCtx = nullptr;
	AVCodecContext  *mSrcVideoCodecCtx = nullptr;
	AVStream        *mSrcVideoStream = nullptr;
	int              mSrcVideoIndex = -1;
	// 以下拉流时更新
	int       mSrcVideoFps = 25;
	int       mSrcVideoWidth = 1920;
	int       mSrcVideoHeight = 1080;
	int       mSrcVideoChannel = 3;

	//目的
	AVFormatContext *mDstFmtCtx = nullptr;
	AVCodecContext  *mDstVideoCodecCtx = nullptr;
	AVStream        *mDstVideoStream = nullptr;
	int              mDstVideoIndex = -1;
	int       mDstVideoFps = 25;//转码后默认fps
	int       mDstVideoWidth = 1920;//转码后默认分辨率宽
	int       mDstVideoHeight = 1080;//转码后默认分辨率高
	int       mDstVideoChannel = 3;
};

#endif // !STREAMPUSHER_H

