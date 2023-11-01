//#include <stdio.h>
//
//#include <string>
//#include <vector>
#include <iostream>


//extern "C" {
//#include <libavcodec/avcodec.h>
//#pragma comment(lib, "avcodec.lib")
//
//#include <libavformat/avformat.h>
//#pragma comment(lib, "avformat.lib")
//
//#include <libavutil/imgutils.h>
//#pragma comment(lib, "avutil.lib")
//
//	// 彩色画面要的
//#include <libswscale/swscale.h>
//#pragma comment(lib, "swscale.lib")
//}


//struct Color_RGB {
//	uint8_t r;
//	uint8_t g;
//	uint8_t b;
//};
//
//
//// 把获取第一帧那个函数拆分了一下
//struct DecoderParam {
//	AVFormatContext *fmtCtx;
//	AVCodecContext *vcodecCtx;
//	int width;
//	int height;
//	int VideoStreamIndex;
//};
//
//void InitDecoder(const char* filepath, DecoderParam &param) {
//	AVFormatContext *fmtCtx = nullptr;
//	avformat_open_input(&fmtCtx, filepath, NULL, NULL);
//	avformat_find_stream_info(fmtCtx, NULL);
//
//	AVCodecContext *vcodecCtx = nullptr;
//	for (int i = 0; i < fmtCtx->nb_streams; i++) {
//		const AVCodec *codec = avcodec_find_decoder(fmtCtx->streams[i]->codecpar->codec_id);
//		if (codec->type == AVMEDIA_TYPE_VIDEO) {
//			param.VideoStreamIndex = i;
//			vcodecCtx = avcodec_alloc_context3(codec);
//			avcodec_parameters_to_context(vcodecCtx, fmtCtx->streams[i]->codecpar);
//			avcodec_open2(vcodecCtx, codec, NULL);
//		}
//	}
//	param.fmtCtx = fmtCtx;
//	param.vcodecCtx = vcodecCtx;
//	param.width = vcodecCtx->width;
//	param.height = vcodecCtx->height;
//}

//AVFrame* RequestFrame(DecoderParam &param) {
//	//auto &fmtCtx = param.fmtCtx;
//	//auto &vcodecCtx = param.vcodecCtx;
//	//auto &VideoStreamIndex = param.VideoStreamIndex;
//
//	while (1) {
//		AVPacket *packet = av_packet_alloc();
//		//int ret = av_read_frame(fmtCtx, packet);
//		int ret = av_read_frame(param.fmtCtx, packet);
//		if (ret == 0 && packet->stream_index == param.VideoStreamIndex) {
//			//ret = avcodec_send_packet(vcodecCtx, packet);
//			ret = avcodec_send_packet(param.vcodecCtx, packet);
//			if (ret == 0) {
//				AVFrame *frame = av_frame_alloc();
//				//ret = avcodec_receive_frame(vcodecCtx, frame);
//				ret = avcodec_receive_frame(param.vcodecCtx, frame);
//				if (ret == 0) {
//					av_packet_unref(packet);
//					return frame;
//				}
//				else if (ret == AVERROR(EAGAIN)) {
//					av_frame_unref(frame);
//				}
//			}
//		}
//
//		av_packet_unref(packet);
//	}
//	return nullptr;
//}



//// 写一个转换颜色编码的函数
//std::vector<Color_RGB> GetRGBPixels(AVFrame *frame, std::vector<Color_RGB> &buffer) {
//	static SwsContext *swsctx = nullptr;
//	swsctx = sws_getCachedContext(swsctx,
//		frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
//		frame->width, frame->height, AVPixelFormat::AV_PIX_FMT_BGR24, NULL, NULL, NULL, NULL
//	);  // 这里原来的类型转换是用的 (AVPixelFormat)frame->format
//
//	// 每次循环调用这个函数，都会重新分配这个vector，debug下就很慢
//	//std::vector<Color_RGB> buffer(frame->width * frame->height);
//
//	//uint8_t* data[] = {(uint8_t*)&buffer[0]};
//	uint8_t* data[] = { reinterpret_cast<uint8_t*>(&buffer[0]) };  // c++类型的指针风格转换
//	int linesize[] = { frame->width * 3 };
//	// sws_scale 函数可以对画面进行缩放，同时还能改变颜色编码，
//	sws_scale(swsctx, frame->data, frame->linesize, 0, frame->height, data, linesize);
//	return buffer;
//}




int main() {
	////std::string file_path = "C:\\Users\\Administrator\\Pictures\\keypoint_result.mp4";
	//std::string file_path = "rtsp://192.168.108.11:554/user=admin&password=&channel=1&stream=1.sdp?";
	//DecoderParam decoderParam;
	//InitDecoder(file_path.c_str(), decoderParam);
	//int width = decoderParam.width;
	//int height = decoderParam.height;
	//auto &fmtCtx = decoderParam.fmtCtx;   // 不知道它这都习惯定义变量时用 & 引用
	//auto &vcodecCtx = decoderParam.vcodecCtx;


	//// 进入循环，debug下，很慢，原来是每次调用 GetRGBPixels 函数，里面就要分配一个很大的vector
	//// 所以在循环前创建好，直接传进去，避免每次循环都去重新分配
	//std::vector<Color_RGB> buffer(width * height);


	//AVFrame *frame = RequestFrame(decoderParam);
	//std::vector<Color_RGB> pixels = GetRGBPixels(frame, buffer);  // 解码调用
	//av_frame_free(&frame);
	////StretchBits(window, pixels, width, height);
	//std::cout << pixels.size() << std::endl;
	//std::cout << pixels.max_size() << std::endl;


	//std::cout << "hello world" << std::endl;
	int i = 45;
	system("pause");
	return 0;
}






/*     上面的代码全部放开应该就可以播放       */