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
//	// ��ɫ����Ҫ��
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
//// �ѻ�ȡ��һ֡�Ǹ����������һ��
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



//// дһ��ת����ɫ����ĺ���
//std::vector<Color_RGB> GetRGBPixels(AVFrame *frame, std::vector<Color_RGB> &buffer) {
//	static SwsContext *swsctx = nullptr;
//	swsctx = sws_getCachedContext(swsctx,
//		frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
//		frame->width, frame->height, AVPixelFormat::AV_PIX_FMT_BGR24, NULL, NULL, NULL, NULL
//	);  // ����ԭ��������ת�����õ� (AVPixelFormat)frame->format
//
//	// ÿ��ѭ����������������������·������vector��debug�¾ͺ���
//	//std::vector<Color_RGB> buffer(frame->width * frame->height);
//
//	//uint8_t* data[] = {(uint8_t*)&buffer[0]};
//	uint8_t* data[] = { reinterpret_cast<uint8_t*>(&buffer[0]) };  // c++���͵�ָ����ת��
//	int linesize[] = { frame->width * 3 };
//	// sws_scale �������ԶԻ���������ţ�ͬʱ���ܸı���ɫ���룬
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
	//auto &fmtCtx = decoderParam.fmtCtx;   // ��֪�����ⶼϰ�߶������ʱ�� & ����
	//auto &vcodecCtx = decoderParam.vcodecCtx;


	//// ����ѭ����debug�£�������ԭ����ÿ�ε��� GetRGBPixels �����������Ҫ����һ���ܴ��vector
	//// ������ѭ��ǰ�����ã�ֱ�Ӵ���ȥ������ÿ��ѭ����ȥ���·���
	//std::vector<Color_RGB> buffer(width * height);


	//AVFrame *frame = RequestFrame(decoderParam);
	//std::vector<Color_RGB> pixels = GetRGBPixels(frame, buffer);  // �������
	//av_frame_free(&frame);
	////StretchBits(window, pixels, width, height);
	//std::cout << pixels.size() << std::endl;
	//std::cout << pixels.max_size() << std::endl;


	//std::cout << "hello world" << std::endl;
	int i = 45;
	system("pause");
	return 0;
}






/*     ����Ĵ���ȫ���ſ�Ӧ�þͿ��Բ���       */