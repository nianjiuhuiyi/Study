#include <iostream>
#include "StreamPusher.h"
#include "Log.h"
#include <thread>
extern "C" {
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
}

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swscale.lib")


StreamPusher::StreamPusher(const char* srcUrl, const char* dstUrl, int dstVideoWidth, int dstVideoHeight, int dstVideoFps) :
	mSrcUrl(srcUrl), mDstUrl(dstUrl), mDstVideoWidth(dstVideoWidth), mDstVideoHeight(dstVideoHeight), mDstVideoFps(dstVideoFps) {
	LOGI("StreamPusher::StreamPusher");
}
StreamPusher::~StreamPusher() {
	LOGI("StreamPusher::~StreamPusher");
}

void StreamPusher::start() {
	LOGI("StreamPusher::start begin");

	bool conn = this->connectSrc();

	// 下面这里用 do{...} while(0) 来代替了原来的多层if else，代码看起来好一点，原来的嵌套太多层了，（但这代码还没测过，先放这里）
	
	do 
	{	
		if (!conn) {
			std::cerr << "源流媒体链接失败..." << std::endl;
			break;
		}
		conn = this->connectDst();
		if (!conn) {
			std::cerr << "推流服务器链接失败..." << std::endl;
			break;
		}

		// 初始化参数
		AVFrame* srcFrame = av_frame_alloc(); // pkt->解码->frame
		AVFrame* dstFrame = av_frame_alloc();

		dstFrame->width = mDstVideoWidth;
		dstFrame->height = mDstVideoHeight;
		dstFrame->format = mDstVideoCodecCtx->pix_fmt;  // AV_PIX_FMT_YUV420P;
		int dstFrame_buff_size = av_image_get_buffer_size(mDstVideoCodecCtx->pix_fmt, mDstVideoWidth, mDstVideoHeight, 1);
		uint8_t* dstFrame_buff = (uint8_t*)av_malloc(dstFrame_buff_size);
		av_image_fill_arrays(dstFrame->data, dstFrame->linesize, dstFrame_buff,
			mDstVideoCodecCtx->pix_fmt, mDstVideoWidth, mDstVideoHeight, 1);

		SwsContext* sws_ctx_src2dst = sws_getContext(mSrcVideoWidth, mSrcVideoHeight,
			mSrcVideoCodecCtx->pix_fmt,
			mDstVideoWidth, mDstVideoHeight,
			mDstVideoCodecCtx->pix_fmt,
			SWS_BICUBIC, nullptr, nullptr, nullptr);

		AVPacket srcPkt; // 拉流时获取的未解码帧
		AVPacket* dstPkt = av_packet_alloc(); // 推流时编码后的帧
		int continuity_read_error_count = 0; // 连续读错误数量
		int continuity_write_error_count = 0; // 连续写错误数量
		int ret = -1;
		int64_t frameCount = 0;

		while (true) {
			if (av_read_frame(mSrcFmtCtx, &srcPkt) >= 0) {
				continuity_read_error_count = 0;

				if (srcPkt.stream_index != mSrcVideoIndex) {
					//av_free_packet(&pkt);//过时
					av_packet_unref(&srcPkt);
					continue;
				}
				// 读取pkt->解码->编码->推流
				ret = avcodec_send_packet(mSrcVideoCodecCtx, &srcPkt);
				if (ret != 0) { 
					LOGI("avcodec_send_packet error: ret=%d", ret); 
					continue;
				}
				ret = avcodec_receive_frame(mSrcVideoCodecCtx, srcFrame);
				if (ret != 0) {
					LOGI("avcodec_receive_frame error: ret=%d", ret);
					continue;
				}

				frameCount++;
				// 解码成功->修改分辨率->修改编码

				// frame（yuv420p） 转 frame_bgr
				sws_scale(sws_ctx_src2dst,
					srcFrame->data, srcFrame->linesize, 0, mSrcVideoHeight,
					dstFrame->data, dstFrame->linesize);

				//开始编码 start
				dstFrame->pts = dstFrame->pkt_dts = av_rescale_q_rnd(frameCount, mDstVideoCodecCtx->time_base, mDstVideoStream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

			
				// 原来代码中使用的是 dstFrame->pkt_duration =
				// 报错：'AVFrame::pkt_duration': 被声明已否决。 chatgpt解答：表示使用了已经被弃用的 AVFrame::pkt_duration 字段。在新的 FFmpeg 版本中，推荐使用 AVFrame::pkt_duration2 或者 AVFrame::best_effort_timestamp 字段
				dstFrame->best_effort_timestamp = av_rescale_q_rnd(1, mDstVideoCodecCtx->time_base, mDstVideoStream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

				dstFrame->pkt_pos = frameCount;
				ret = avcodec_send_frame(mDstVideoCodecCtx, dstFrame);
				if (ret < 0) {
					LOGI("avcodec_send_frame error: ret=%d", ret);
					continue;
				}

				ret = avcodec_receive_packet(mDstVideoCodecCtx, dstPkt);
				if (ret < 0) {
					LOGI("avcodec_receive_packet error: ret=%d", ret);
					continue;
				}

				// 推流 start
				dstPkt->stream_index = mDstVideoIndex;
				ret = av_interleaved_write_frame(mDstFmtCtx, dstPkt);
				if (ret >= 0) {   // 推流成功
					continuity_write_error_count = 0;
					continue;
				}
				// 下面就是推流失败
				LOGI("av_interleaved_write_frame error: ret=%d", ret);
				++continuity_write_error_count;
				if (continuity_write_error_count > 5) {// 连续5次推流失败，则断开连接
					LOGI("av_interleaved_write_frame error: continuity_write_error_count=%d,dstUrl=%s", continuity_write_error_count, mDstUrl.data());
					break;
				}

			}
			else {
				// av_free_packet(&pkt);//过时
				av_packet_unref(&srcPkt);
				++continuity_read_error_count;
				if (continuity_read_error_count > 5) {// 连续5次拉流失败，则断开连接
					LOGI("av_read_frame error: continuity_read_error_count=%d,srcUrl=%s", continuity_read_error_count, mSrcUrl.data());
					break;
				}
				else {
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
			}
		}

		// 销毁
		av_frame_free(&srcFrame);
		//av_frame_unref(srcFrame);
		srcFrame = NULL;

		av_frame_free(&dstFrame);
		//av_frame_unref(dstFrame);
		dstFrame = NULL;

		av_free(dstFrame_buff);
		dstFrame_buff = NULL;

		sws_freeContext(sws_ctx_src2dst);
		sws_ctx_src2dst = NULL;
	} while (0);
	


	//if (conn) {
	//	conn = this->connectDst();
	//	if (conn) {
	//		// 初始化参数
	//		AVFrame* srcFrame = av_frame_alloc(); // pkt->解码->frame
	//		AVFrame* dstFrame = av_frame_alloc();

	//		dstFrame->width = mDstVideoWidth;
	//		dstFrame->height = mDstVideoHeight;
	//		dstFrame->format = mDstVideoCodecCtx->pix_fmt;  // AV_PIX_FMT_YUV420P;
	//		int dstFrame_buff_size = av_image_get_buffer_size(mDstVideoCodecCtx->pix_fmt, mDstVideoWidth, mDstVideoHeight, 1);
	//		uint8_t* dstFrame_buff = (uint8_t*)av_malloc(dstFrame_buff_size);
	//		av_image_fill_arrays(dstFrame->data, dstFrame->linesize, dstFrame_buff,
	//			mDstVideoCodecCtx->pix_fmt, mDstVideoWidth, mDstVideoHeight, 1);

	//		SwsContext* sws_ctx_src2dst = sws_getContext(mSrcVideoWidth, mSrcVideoHeight,
	//			mSrcVideoCodecCtx->pix_fmt,
	//			mDstVideoWidth, mDstVideoHeight,
	//			mDstVideoCodecCtx->pix_fmt,
	//			SWS_BICUBIC, nullptr, nullptr, nullptr);

	//		AVPacket srcPkt; // 拉流时获取的未解码帧
	//		AVPacket* dstPkt = av_packet_alloc(); // 推流时编码后的帧
	//		int continuity_read_error_count = 0; // 连续读错误数量
	//		int continuity_write_error_count = 0; // 连续写错误数量
	//		int ret = -1;
	//		int64_t frameCount = 0;
	//		while (true) {// 不中断会继续执行
	//			if (av_read_frame(mSrcFmtCtx, &srcPkt) >= 0) {
	//				continuity_read_error_count = 0;
	//				if (srcPkt.stream_index == mSrcVideoIndex) {
	//					// 读取pkt->解码->编码->推流
	//					ret = avcodec_send_packet(mSrcVideoCodecCtx, &srcPkt);
	//					if (ret == 0) {
	//						ret = avcodec_receive_frame(mSrcVideoCodecCtx, srcFrame);
	//						if (ret == 0) {
	//							frameCount++;
	//							// 解码成功->修改分辨率->修改编码

	//							// frame（yuv420p） 转 frame_bgr
	//							sws_scale(sws_ctx_src2dst,
	//								srcFrame->data, srcFrame->linesize, 0, mSrcVideoHeight,
	//								dstFrame->data, dstFrame->linesize);

	//							//开始编码 start
	//							dstFrame->pts = dstFrame->pkt_dts = av_rescale_q_rnd(frameCount, mDstVideoCodecCtx->time_base, mDstVideoStream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

	//							/*
	//							原来代码中使用的是 dstFrame->pkt_duration =
	//							报错：'AVFrame::pkt_duration': 被声明已否决。 chatgpt解答：表示使用了已经被弃用的 AVFrame::pkt_duration 字段。在新的 FFmpeg 版本中，推荐使用 AVFrame::pkt_duration2 或者 AVFrame::best_effort_timestamp 字段
	//							*/
	//							dstFrame->best_effort_timestamp = av_rescale_q_rnd(1, mDstVideoCodecCtx->time_base, mDstVideoStream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

	//							dstFrame->pkt_pos = frameCount;

	//							ret = avcodec_send_frame(mDstVideoCodecCtx, dstFrame);
	//							if (ret >= 0) {
	//								ret = avcodec_receive_packet(mDstVideoCodecCtx, dstPkt);
	//								if (ret >= 0) {
	//									// 推流 start
	//									dstPkt->stream_index = mDstVideoIndex;
	//									ret = av_interleaved_write_frame(mDstFmtCtx, dstPkt);
	//									if (ret < 0) {// 推流失败
	//										LOGI("av_interleaved_write_frame error: ret=%d", ret);
	//										++continuity_write_error_count;
	//										if (continuity_write_error_count > 5) {// 连续5次推流失败，则断开连接
	//											LOGI("av_interleaved_write_frame error: continuity_write_error_count=%d,dstUrl=%s", continuity_write_error_count, mDstUrl.data());
	//											break;
	//										}
	//									}
	//									else {
	//										continuity_write_error_count = 0;
	//									}
	//									// 推流 end
	//								}
	//								else {
	//									LOGI("avcodec_receive_packet error: ret=%d", ret);
	//								}
	//							}
	//							else {
	//								LOGI("avcodec_send_frame error: ret=%d", ret);
	//							}
	//							// 开始编码 end
	//						}
	//						else {
	//							LOGI("avcodec_receive_frame error: ret=%d", ret);
	//						}
	//					}
	//					else {
	//						LOGI("avcodec_send_packet error: ret=%d", ret);
	//					}

	//					// std::this_thread::sleep_for(std::chrono::milliseconds(1));
	//				}
	//				else {
	//					//av_free_packet(&pkt);//过时
	//					av_packet_unref(&srcPkt);
	//				}
	//			}
	//			else {
	//				// av_free_packet(&pkt);//过时
	//				av_packet_unref(&srcPkt);
	//				++continuity_read_error_count;
	//				if (continuity_read_error_count > 5) {// 连续5次拉流失败，则断开连接
	//					LOGI("av_read_frame error: continuity_read_error_count=%d,srcUrl=%s", continuity_read_error_count, mSrcUrl.data());
	//					break;
	//				}
	//				else {
	//					std::this_thread::sleep_for(std::chrono::milliseconds(100));
	//				}
	//			}
	//		}

	//		// 销毁
	//		av_frame_free(&srcFrame);
	//		//av_frame_unref(srcFrame);
	//		srcFrame = NULL;

	//		av_frame_free(&dstFrame);
	//		//av_frame_unref(dstFrame);
	//		dstFrame = NULL;

	//		av_free(dstFrame_buff);
	//		dstFrame_buff = NULL;

	//		sws_freeContext(sws_ctx_src2dst);
	//		sws_ctx_src2dst = NULL;
	//	}
	//}

	this->closeConnectDst();
	this->closeConnectSrc();

	LOGI("StreamPusher::start end");
}

bool StreamPusher::connectSrc() {

	mSrcFmtCtx = avformat_alloc_context();

	AVDictionary* fmt_options = NULL;
	av_dict_set(&fmt_options, "rtsp_transport", "tcp", 0); //设置rtsp底层网络协议 tcp or udp
	av_dict_set(&fmt_options, "stimeout", "3000000", 0);   //设置rtsp连接超时（单位 us）
	av_dict_set(&fmt_options, "rw_timeout", "3000000", 0); //设置rtmp/http-flv连接超时（单位 us）
	av_dict_set(&fmt_options, "fflags", "discardcorrupt", 0);
	//av_dict_set(&fmt_options, "timeout", "3000000", 0);//设置udp/http超时（单位 us）

	int ret = avformat_open_input(&mSrcFmtCtx, mSrcUrl.data(), NULL, &fmt_options);

	if (ret != 0) {
		LOGI("avformat_open_input error: srcUrl=%s", mSrcUrl.data());
		return false;
	}

	if (avformat_find_stream_info(mSrcFmtCtx, NULL) < 0) {
		LOGI("avformat_find_stream_info error: srcUrl=%s", mSrcUrl.data());
		return false;
	}

	// video start
	for (int i = 0; i < mSrcFmtCtx->nb_streams; i++) {
		if (mSrcFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			mSrcVideoIndex = i;
			break;
		}
	}
	//mSrcVideoIndex = av_find_best_stream(mFmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

	if (mSrcVideoIndex > -1) {
		AVCodecParameters* videoCodecPar = mSrcFmtCtx->streams[mSrcVideoIndex]->codecpar;
		const AVCodec* videoCodec = avcodec_find_decoder(videoCodecPar->codec_id);
		if (!videoCodec) {
			LOGI("avcodec_find_decoder error: srcUrl=%s", mSrcUrl.data());
			return false;
		}

		mSrcVideoCodecCtx = avcodec_alloc_context3(videoCodec);
		if (avcodec_parameters_to_context(mSrcVideoCodecCtx, videoCodecPar) != 0) {
			LOGI("avcodec_parameters_to_context error: srcUrl=%s", mSrcUrl.data());
			return false;
		}
		if (avcodec_open2(mSrcVideoCodecCtx, videoCodec, nullptr) < 0) {
			LOGI("avcodec_open2 error: srcUrl=%s", mSrcUrl.data());
			return false;
		}

		mSrcVideoCodecCtx->thread_count = 1;
		mSrcVideoStream = mSrcFmtCtx->streams[mSrcVideoIndex];

		if (0 == mSrcVideoStream->avg_frame_rate.den) {
			LOGI("avg_frame_rate.den = 0 error: srcUrl=%s", mSrcUrl.data());
			mSrcVideoFps = 25;
		}
		else {
			mSrcVideoFps = mSrcVideoStream->avg_frame_rate.num / mSrcVideoStream->avg_frame_rate.den;
		}

		mSrcVideoWidth = mSrcVideoCodecCtx->width;
		mSrcVideoHeight = mSrcVideoCodecCtx->height;

	}
	else {
		LOGI("There is no video stream in the video: srcUrl=%s", mSrcUrl.data());
		return false;
	}
	// video end;
	return true;
}
void StreamPusher::closeConnectSrc() {
	std::this_thread::sleep_for(std::chrono::milliseconds(1));

	if (mSrcVideoCodecCtx) {
		avcodec_close(mSrcVideoCodecCtx);
		avcodec_free_context(&mSrcVideoCodecCtx);
		mSrcVideoCodecCtx = nullptr;
	}

	if (mSrcFmtCtx) {
		// 拉流不需要释放start
		//if (mFmtCtx && !(mFmtCtx->oformat->flags & AVFMT_NOFILE)) {
		//    avio_close(mFmtCtx->pb);
		//}
		// 拉流不需要释放end
		avformat_close_input(&mSrcFmtCtx);
		avformat_free_context(mSrcFmtCtx);
		mSrcFmtCtx = nullptr;
	}
}
bool StreamPusher::connectDst() {
	// 这里直接给 rtsp 是可以的，跟命令行推流时给 -f rtsp 应该是一个意思，rtmp放这里会报错，然后推流到rtmp服务器时是用的 -f flv
	if (avformat_alloc_output_context2(&mDstFmtCtx, NULL, "flv", mDstUrl.data()) < 0) {
		LOGI("avformat_alloc_output_context2 error: dstUrl=%s", mDstUrl.data());
		return false;
	}

	// init video start
	// const AVCodec *videoCodec = avcodec_find_encoder(this->mSrcFmtCtx->streams[mDstVideoIndex]->codecpar->codec_id);
	// 上面的是应该用的输入流的编码格式
	const AVCodec* videoCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!videoCodec) {
		LOGI("avcodec_find_encoder error: dstUrl=%s", mDstUrl.data());
		return false;
	}
	mDstVideoCodecCtx = avcodec_alloc_context3(videoCodec);
	if (!mDstVideoCodecCtx) {
		LOGI("avcodec_alloc_context3 error: dstUrl=%s", mDstUrl.data());
		return false;
	}
	//int bit_rate = 300 * 1024 * 8;  //压缩后每秒视频的bit位大小 300kB
	int bit_rate = 4096000;
	// CBR：Constant BitRate - 固定比特率
	mDstVideoCodecCtx->flags |= AV_CODEC_FLAG_QSCALE;
	mDstVideoCodecCtx->bit_rate = bit_rate;
	mDstVideoCodecCtx->rc_min_rate = bit_rate;
	mDstVideoCodecCtx->rc_max_rate = bit_rate;
	mDstVideoCodecCtx->bit_rate_tolerance = bit_rate;

	//VBR
	// mDstVideoCodecCtx->flags |= AV_CODEC_FLAG_QSCALE;
	// mDstVideoCodecCtx->rc_min_rate = bit_rate / 2;
	// mDstVideoCodecCtx->rc_max_rate = bit_rate / 2 + bit_rate;
	// mDstVideoCodecCtx->bit_rate = bit_rate;

	//ABR：Average Bitrate - 平均码率
	// mDstVideoCodecCtx->bit_rate = bit_rate;

	mDstVideoCodecCtx->codec_id = videoCodec->id;
	//mDstVideoCodecCtx->pix_fmt = AV_PIX_FMT_YUVJ420P;  // 不支持AV_PIX_FMT_BGR24直接进行编码
	mDstVideoCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;  // yuvj×××这个格式被丢弃了，然后转化为yuv格式，不然会有警告
	mDstVideoCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
	mDstVideoCodecCtx->width = mDstVideoWidth;
	mDstVideoCodecCtx->height = mDstVideoHeight;
	mDstVideoCodecCtx->time_base = { 1,mDstVideoFps };
	// mDstVideoCodecCtx->framerate = { mDstVideoFps, 1 };
	mDstVideoCodecCtx->gop_size = 5;
	mDstVideoCodecCtx->max_b_frames = 0;
	mDstVideoCodecCtx->thread_count = 1;
	mDstVideoCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;   //添加PPS、SPS


	AVDictionary* video_codec_options = NULL;  // 一些类似于命令行的附加的参数
	//H.264
	if (mDstVideoCodecCtx->codec_id == AV_CODEC_ID_H264) {
		//            av_dict_set(&video_codec_options, "profile", "main", 0);
		av_dict_set(&video_codec_options, "preset", "superfast", 0);
		av_dict_set(&video_codec_options, "tune", "zerolatency", 0);
	}
	//H.265
	if (mDstVideoCodecCtx->codec_id == AV_CODEC_ID_H265) {
		av_dict_set(&video_codec_options, "preset", "ultrafast", 0);
		av_dict_set(&video_codec_options, "tune", "zero-latency", 0);
	}
	if (avcodec_open2(mDstVideoCodecCtx, videoCodec, &video_codec_options) < 0) {
		LOGI("avcodec_open2 error: dstUrl=%s", mDstUrl.data());
		return false;
	}
	mDstVideoStream = avformat_new_stream(mDstFmtCtx, videoCodec);
	if (!mDstVideoStream) {
		LOGI("avformat_new_stream error: dstUrl=%s", mDstUrl.data());
		return false;
	}
	mDstVideoStream->id = mDstFmtCtx->nb_streams - 1;
	// stream的time_base参数非常重要，它表示将现实中的一秒钟分为多少个时间基, 在下面调用avformat_write_header时自动完成
	avcodec_parameters_from_context(mDstVideoStream->codecpar, mDstVideoCodecCtx);
	mDstVideoIndex = mDstVideoStream->id;
	// init video end


	// open output url
	if (!(mDstFmtCtx->oformat->flags & AVFMT_NOFILE)) {
		if (avio_open(&mDstFmtCtx->pb, mDstUrl.data(), AVIO_FLAG_WRITE) < 0) {
			LOGI("avio_open error: dstUrl=%s", mDstUrl.data());
			return false;
		}
	}

	AVDictionary* fmt_options = NULL;
	// av_dict_set(&fmt_options, "bufsize", "1024", 0);
	av_dict_set(&fmt_options, "rw_timeout", "30000000", 0); // 设置rtmp/http-flv连接超时（单位 us）
	av_dict_set(&fmt_options, "stimeout", "30000000", 0);   // 设置rtsp连接超时（单位 us）
	av_dict_set(&fmt_options, "rtsp_transport", "tcp", 0);  // 也可用 udp
	// av_dict_set(&fmt_options, "fflags", "discardcorrupt", 0);
	// av_dict_set(&fmt_options, "muxdelay", "0.1", 0);
	// av_dict_set(&fmt_options, "tune", "zerolatency", 0);

	mDstFmtCtx->video_codec_id = mDstFmtCtx->oformat->video_codec;

	if (avformat_write_header(mDstFmtCtx, &fmt_options) < 0) { // 调用该函数会将所有stream的time_base，自动设置一个值，通常是1/90000或1/1000，这表示一秒钟表示的时间基长度
		LOGI("avformat_write_header error: dstUrl=%s", mDstUrl.data());
		return false;
	}


	// mDstFmtCtx->audio_codec_id = AV_CODEC_ID_AAC;  // 如果以后是声音的话，可能要这样指定下
	std::cout << "\nmy_video_codec: " << mDstFmtCtx->video_codec_id << std::endl;
	std::cout << "\nmy_audio_codec: " << mDstFmtCtx->audio_codec_id << std::endl;
	av_dump_format(mDstFmtCtx, 0, mDstUrl.c_str(), 1);

	return true;
}
void StreamPusher::closeConnectDst() {
	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	if (mDstFmtCtx) {
		// 推流需要释放start
		if (mDstFmtCtx && !(mDstFmtCtx->oformat->flags & AVFMT_NOFILE)) {
			avio_close(mDstFmtCtx->pb);
		}
		// 推流需要释放end
		avformat_free_context(mDstFmtCtx);
		mDstFmtCtx = nullptr;
	}

	if (mDstVideoCodecCtx) {
		if (mDstVideoCodecCtx->extradata) {
			av_free(mDstVideoCodecCtx->extradata);
			mDstVideoCodecCtx->extradata = NULL;
		}

		avcodec_close(mDstVideoCodecCtx);
		avcodec_free_context(&mDstVideoCodecCtx);
		mDstVideoCodecCtx = nullptr;
	}
}