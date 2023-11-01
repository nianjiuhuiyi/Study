extern "C" {
#include <libavcodec/avcodec.h>
#pragma comment(lib, "avcodec.lib")

#include <libavformat/avformat.h>
#pragma comment(lib, "avformat.lib")

#include <libavutil/imgutils.h>
#pragma comment(lib, "avutil.lib")

	// 彩色画面要的
#include <libswscale/swscale.h>
#pragma comment(lib, "swscale.lib")
}

struct Color_RGB {
	uint8_t r;
	uint8_t g;
	uint8_t b;
};


/*
	yuvj×××这个格式被丢弃了，然后转化为yuv格式，
	不然有一个警告 deprecated pixel format used, make sure you did set range correctly，
	这个问题在前面和win32写api时可用，但是不知道其它地方会不会报错，就改过了
*/
AVPixelFormat ConvertDeprecatedFormat(enum AVPixelFormat format)
{
	switch (format) {
	case AV_PIX_FMT_YUVJ420P:
		return AV_PIX_FMT_YUV420P;
		break;
	case AV_PIX_FMT_YUVJ422P:
		return AV_PIX_FMT_YUV422P;
		break;
	case AV_PIX_FMT_YUVJ444P:
		return AV_PIX_FMT_YUV444P;
		break;
	case AV_PIX_FMT_YUVJ440P:
		return AV_PIX_FMT_YUV440P;
		break;
	default:
		return format;
		break;
	}
}


// 把获取第一帧那个函数拆分了一下
struct DecoderParam {
	AVFormatContext *fmtCtx;
	AVCodecContext *vcodecCtx;
	int width;
	int height;
	int VideoStreamIndex;
};

void InitDecoder(const char* filepath, DecoderParam &param) {
	AVFormatContext *fmtCtx = nullptr;
	// ret：0表示成功，-2代表文件不存在，
	int ret = avformat_open_input(&fmtCtx, filepath, NULL, NULL);

	avformat_find_stream_info(fmtCtx, NULL);

	AVCodecContext *vcodecCtx = nullptr;
	for (int i = 0; i < fmtCtx->nb_streams; i++) {
		const AVCodec *codec = avcodec_find_decoder(fmtCtx->streams[i]->codecpar->codec_id);
		if (codec->type == AVMEDIA_TYPE_VIDEO) {
			param.VideoStreamIndex = i;
			vcodecCtx = avcodec_alloc_context3(codec);
			avcodec_parameters_to_context(vcodecCtx, fmtCtx->streams[i]->codecpar);
			avcodec_open2(vcodecCtx, codec, NULL);
		}
	}
	param.fmtCtx = fmtCtx;
	param.vcodecCtx = vcodecCtx;
	param.width = vcodecCtx->width;
	param.height = vcodecCtx->height;
}

AVFrame* RequestFrame(DecoderParam &param) {
	auto &fmtCtx = param.fmtCtx;
	auto &vcodecCtx = param.vcodecCtx;
	auto &VideoStreamIndex = param.VideoStreamIndex;

	while (1) {
		AVPacket *packet = av_packet_alloc();
		int ret = av_read_frame(fmtCtx, packet);
		if (ret == 0 && packet->stream_index == param.VideoStreamIndex) {
			ret = avcodec_send_packet(vcodecCtx, packet);
			if (ret == 0) {
				AVFrame *frame = av_frame_alloc();
				ret = avcodec_receive_frame(vcodecCtx, frame);
				if (ret == 0) {
					av_packet_unref(packet);
					return frame;
				}
				else if (ret == AVERROR(EAGAIN)) {
					av_frame_unref(frame);
				}
			}
		}

		av_packet_unref(packet);
	}
	return nullptr;
}



