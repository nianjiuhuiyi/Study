#include <vector>
#include <opencv2/opencv.hpp>
#include <chrono>
#include <thread>
#include <ctime>

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


struct Color_RGB {
	uint8_t r;
	uint8_t g;
	uint8_t b;
};


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

	// linux上这里一直报错，返回的是 -1330794744，网上搜索一圈了，ffmpeg也重新编译了，还是不行
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
			break;  // 我加的
		}
	}
	param.fmtCtx = fmtCtx;
	param.vcodecCtx = vcodecCtx;
	param.width = vcodecCtx->width;
	param.height = vcodecCtx->height;
	
}

/* 这种写法不能要，会内存泄漏 */
//AVFrame* RequestFrame(DecoderParam &param) {
//	auto &fmtCtx = param.fmtCtx;
//	auto &vcodecCtx = param.vcodecCtx;
//	auto &VideoStreamIndex = param.VideoStreamIndex;
//
//	while (1) {
//		AVPacket *packet = av_packet_alloc();  // 主要是这里一直没释放
//		int ret = av_read_frame(fmtCtx, packet);
//		if (ret == 0 && packet->stream_index == param.VideoStreamIndex) {
//			ret = avcodec_send_packet(vcodecCtx, packet);
//			if (ret == 0) {
//				AVFrame *frame = av_frame_alloc();
//				ret = avcodec_receive_frame(vcodecCtx, frame);
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

/* 都不这么写了，直接把指针串传进去 */

// 写一个转换颜色编码的函数
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


int RequestFrame(DecoderParam &param, AVFrame *frame, AVPacket *packet) {
	auto &fmtCtx = param.fmtCtx;
	auto &vcodecCtx = param.vcodecCtx;
	auto &VideoStreamIndex = param.VideoStreamIndex;

	while (1) {
		//AVPacket *packet = av_packet_alloc();
		int ret = av_read_frame(fmtCtx, packet);
		if (ret == 0 && packet->stream_index == param.VideoStreamIndex) {
			ret = avcodec_send_packet(vcodecCtx, packet);
			if (ret == 0) {
				ret = avcodec_receive_frame(vcodecCtx, frame);
				if (ret == 0) {
					av_packet_unref(packet);
					return 0;  // 代表读取成功
				}
				else if (ret == AVERROR(EAGAIN)) {
					av_frame_unref(frame);
				}
			}
		}
		av_packet_unref(packet);
	}
	return -1;
}

// 写一个转换颜色编码的函数
void GetRGBPixels(AVFrame *frame, std::vector<Color_RGB> &buffer) {
	static SwsContext *swsctx = nullptr;
	swsctx = sws_getCachedContext(swsctx,
		frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
		frame->width, frame->height, AVPixelFormat::AV_PIX_FMT_BGR24, NULL, NULL, NULL, NULL
	);  // 这里原来的类型转换是用的 (AVPixelFormat)frame->format

	// 每次循环调用这个函数，都会重新分配这个vector，debug下就很慢
	//std::vector<Color_RGB> buffer(frame->width * frame->height);

	//uint8_t* data[] = {(uint8_t*)&buffer[0]};
	uint8_t* data[] = { reinterpret_cast<uint8_t*>(&buffer[0]) };  // c++类型的指针风格转换
	int linesize[] = { frame->width * 3 };
	// sws_scale 函数可以对画面进行缩放，同时还能改变颜色编码，
	sws_scale(swsctx, frame->data, frame->linesize, 0, frame->height, data, linesize);
	// return buffer;  // 不返回了，直接用buffer
}




/* ffmpeg读流 + opencv做展示 */

int main() {
	const char* rtsp_path = "rtsp://192.168.108.133:554/user=admin&password=&channel=1&stream=0.sdp?";
	DecoderParam decoderParam;
	InitDecoder(rtsp_path, decoderParam);  // 如果file_path.c_str()是，std::string，就写成file_path.c_str()
	int width = decoderParam.width;
	int height = decoderParam.height;
	auto &fmtCtx = decoderParam.fmtCtx;   // 不知道它这都习惯定义变量时用 & 引用
	auto &vcodecCtx = decoderParam.vcodecCtx;

	cv::Mat img(cv::Size(width, height), CV_8UC3);
	std::vector<Color_RGB> buffer(width * height);
	AVFrame *frame = av_frame_alloc();
	AVPacket *packet = av_packet_alloc();

	int nums = 0;
	int time = 0;
	while (1) {
		//AVFrame *frame = RequestFrame(decoderParam);  // 不再用原来这种方式
		RequestFrame(decoderParam, frame, packet);

		// 原来的格式是AV_PIX_FMT_YUVJ420P，被丢弃，会有一个警告：deprecated pixel format used, make sure you did set range correctly  (主要是针对rtsp，本地视频好像不用)
		frame->format = ConvertDeprecatedFormat(static_cast<AVPixelFormat>(frame->format));

		//std::vector<Color_RGB> pixels = GetRGBPixels(frame, buffer);  // 解码调用  // 不再用原来这种方式
		GetRGBPixels(frame, buffer);  // 解码调用
		//uint8_t* data[] = { reinterpret_cast<uint8_t*>(&pixels[0]) };
		//uint8_t *data = reinterpret_cast<uint8_t*>(&buffer[0]);  // 上面那行也是一个意思，一般写作unsigned char* data，OpenGL都是这样的
		//img.data = data;
		img.data = reinterpret_cast<uint8_t*>(&buffer[0]);

		/*  不用下面这种循环写法，在图片很大时就会很慢 */
		//int time = 0;
		//for (int i = 0; i < img.rows; ++i) {
		//	for (int j = 0; j < img.cols; ++j) {
		//		img.at<cv::Vec3b>(i, j) = { pixels[time].r, pixels[time].g, pixels[time].b };
		//		time++;
		//	}
		//}


		//std::this_thread::sleep_for(std::chrono::milliseconds(200));   // 开始时是21M

		std::tm time_info{};
		std::time_t timestamp = std::time(nullptr);
		errno_t err = localtime_s(&time_info, &timestamp);
		if (err) {
			std::cout << "Failed to convert timestamp to time\n";
		}
		else {
			char str[100]{};
			std::strftime(str, sizeof(str), "%Y-%m-%d %H:%M:%S", &time_info);
			//std::cout << "Current time: " << str << '\n';
			cv::putText(img, std::string(str), cv::Point2i(500, 500), cv::FONT_HERSHEY_PLAIN, 2, (255, 0, 255), 2, cv::LINE_AA);
		}

		// show
		cv::imshow("ffmpeg", img);
		if ((cv::waitKey(1) & 0xff) != 255) break;

		nums += 200;
		if (nums % (1000 * 60) == 0) {
			time++;
			std::cout << "过去了: " << time << "分钟！" << std::endl;
			nums = 0;
		}

	}
	// av_frame_unref(frame);  // 这似乎不是释放
	av_frame_free(&frame);  // 来释放内存
	av_packet_free(&packet);
	avcodec_free_context(&decoderParam.vcodecCtx);
	cv::destroyAllWindows();
	return 0;
}

