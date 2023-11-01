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

	// Դͷ
	AVFormatContext *mSrcFmtCtx = nullptr;
	AVCodecContext  *mSrcVideoCodecCtx = nullptr;
	AVStream        *mSrcVideoStream = nullptr;
	int              mSrcVideoIndex = -1;
	// ��������ʱ����
	int       mSrcVideoFps = 25;
	int       mSrcVideoWidth = 1920;
	int       mSrcVideoHeight = 1080;
	int       mSrcVideoChannel = 3;

	//Ŀ��
	AVFormatContext *mDstFmtCtx = nullptr;
	AVCodecContext  *mDstVideoCodecCtx = nullptr;
	AVStream        *mDstVideoStream = nullptr;
	int              mDstVideoIndex = -1;
	int       mDstVideoFps = 25;//ת���Ĭ��fps
	int       mDstVideoWidth = 1920;//ת���Ĭ�Ϸֱ��ʿ�
	int       mDstVideoHeight = 1080;//ת���Ĭ�Ϸֱ��ʸ�
	int       mDstVideoChannel = 3;
};

#endif // !STREAMPUSHER_H

