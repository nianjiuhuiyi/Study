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

	// ���������� do{...} while(0) ��������ԭ���Ķ��if else�����뿴������һ�㣬ԭ����Ƕ��̫����ˣ���������뻹û������ȷ����
	
	do 
	{	
		if (!conn) {
			std::cerr << "Դ��ý������ʧ��..." << std::endl;
			break;
		}
		conn = this->connectDst();
		if (!conn) {
			std::cerr << "��������������ʧ��..." << std::endl;
			break;
		}

		// ��ʼ������
		AVFrame* srcFrame = av_frame_alloc(); // pkt->����->frame
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

		AVPacket srcPkt; // ����ʱ��ȡ��δ����֡
		AVPacket* dstPkt = av_packet_alloc(); // ����ʱ������֡
		int continuity_read_error_count = 0; // ��������������
		int continuity_write_error_count = 0; // ����д��������
		int ret = -1;
		int64_t frameCount = 0;

		while (true) {
			if (av_read_frame(mSrcFmtCtx, &srcPkt) >= 0) {
				continuity_read_error_count = 0;

				if (srcPkt.stream_index != mSrcVideoIndex) {
					//av_free_packet(&pkt);//��ʱ
					av_packet_unref(&srcPkt);
					continue;
				}
				// ��ȡpkt->����->����->����
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
				// ����ɹ�->�޸ķֱ���->�޸ı���

				// frame��yuv420p�� ת frame_bgr
				sws_scale(sws_ctx_src2dst,
					srcFrame->data, srcFrame->linesize, 0, mSrcVideoHeight,
					dstFrame->data, dstFrame->linesize);

				//��ʼ���� start
				dstFrame->pts = dstFrame->pkt_dts = av_rescale_q_rnd(frameCount, mDstVideoCodecCtx->time_base, mDstVideoStream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

			
				// ԭ��������ʹ�õ��� dstFrame->pkt_duration =
				// ����'AVFrame::pkt_duration': �������ѷ���� chatgpt��𣺱�ʾʹ�����Ѿ������õ� AVFrame::pkt_duration �ֶΡ����µ� FFmpeg �汾�У��Ƽ�ʹ�� AVFrame::pkt_duration2 ���� AVFrame::best_effort_timestamp �ֶ�
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

				// ���� start
				dstPkt->stream_index = mDstVideoIndex;
				ret = av_interleaved_write_frame(mDstFmtCtx, dstPkt);
				if (ret >= 0) {   // �����ɹ�
					continuity_write_error_count = 0;
					continue;
				}
				// �����������ʧ��
				LOGI("av_interleaved_write_frame error: ret=%d", ret);
				++continuity_write_error_count;
				if (continuity_write_error_count > 5) {// ����5������ʧ�ܣ���Ͽ�����
					LOGI("av_interleaved_write_frame error: continuity_write_error_count=%d,dstUrl=%s", continuity_write_error_count, mDstUrl.data());
					break;
				}

			}
			else {
				// av_free_packet(&pkt);//��ʱ
				av_packet_unref(&srcPkt);
				++continuity_read_error_count;
				if (continuity_read_error_count > 5) {// ����5������ʧ�ܣ���Ͽ�����
					LOGI("av_read_frame error: continuity_read_error_count=%d,srcUrl=%s", continuity_read_error_count, mSrcUrl.data());
					break;
				}
				else {
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
			}
		}

		// ����
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
	//		// ��ʼ������
	//		AVFrame* srcFrame = av_frame_alloc(); // pkt->����->frame
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

	//		AVPacket srcPkt; // ����ʱ��ȡ��δ����֡
	//		AVPacket* dstPkt = av_packet_alloc(); // ����ʱ������֡
	//		int continuity_read_error_count = 0; // ��������������
	//		int continuity_write_error_count = 0; // ����д��������
	//		int ret = -1;
	//		int64_t frameCount = 0;
	//		while (true) {// ���жϻ����ִ��
	//			if (av_read_frame(mSrcFmtCtx, &srcPkt) >= 0) {
	//				continuity_read_error_count = 0;
	//				if (srcPkt.stream_index == mSrcVideoIndex) {
	//					// ��ȡpkt->����->����->����
	//					ret = avcodec_send_packet(mSrcVideoCodecCtx, &srcPkt);
	//					if (ret == 0) {
	//						ret = avcodec_receive_frame(mSrcVideoCodecCtx, srcFrame);
	//						if (ret == 0) {
	//							frameCount++;
	//							// ����ɹ�->�޸ķֱ���->�޸ı���

	//							// frame��yuv420p�� ת frame_bgr
	//							sws_scale(sws_ctx_src2dst,
	//								srcFrame->data, srcFrame->linesize, 0, mSrcVideoHeight,
	//								dstFrame->data, dstFrame->linesize);

	//							//��ʼ���� start
	//							dstFrame->pts = dstFrame->pkt_dts = av_rescale_q_rnd(frameCount, mDstVideoCodecCtx->time_base, mDstVideoStream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

	//							/*
	//							ԭ��������ʹ�õ��� dstFrame->pkt_duration =
	//							����'AVFrame::pkt_duration': �������ѷ���� chatgpt��𣺱�ʾʹ�����Ѿ������õ� AVFrame::pkt_duration �ֶΡ����µ� FFmpeg �汾�У��Ƽ�ʹ�� AVFrame::pkt_duration2 ���� AVFrame::best_effort_timestamp �ֶ�
	//							*/
	//							dstFrame->best_effort_timestamp = av_rescale_q_rnd(1, mDstVideoCodecCtx->time_base, mDstVideoStream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

	//							dstFrame->pkt_pos = frameCount;

	//							ret = avcodec_send_frame(mDstVideoCodecCtx, dstFrame);
	//							if (ret >= 0) {
	//								ret = avcodec_receive_packet(mDstVideoCodecCtx, dstPkt);
	//								if (ret >= 0) {
	//									// ���� start
	//									dstPkt->stream_index = mDstVideoIndex;
	//									ret = av_interleaved_write_frame(mDstFmtCtx, dstPkt);
	//									if (ret < 0) {// ����ʧ��
	//										LOGI("av_interleaved_write_frame error: ret=%d", ret);
	//										++continuity_write_error_count;
	//										if (continuity_write_error_count > 5) {// ����5������ʧ�ܣ���Ͽ�����
	//											LOGI("av_interleaved_write_frame error: continuity_write_error_count=%d,dstUrl=%s", continuity_write_error_count, mDstUrl.data());
	//											break;
	//										}
	//									}
	//									else {
	//										continuity_write_error_count = 0;
	//									}
	//									// ���� end
	//								}
	//								else {
	//									LOGI("avcodec_receive_packet error: ret=%d", ret);
	//								}
	//							}
	//							else {
	//								LOGI("avcodec_send_frame error: ret=%d", ret);
	//							}
	//							// ��ʼ���� end
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
	//					//av_free_packet(&pkt);//��ʱ
	//					av_packet_unref(&srcPkt);
	//				}
	//			}
	//			else {
	//				// av_free_packet(&pkt);//��ʱ
	//				av_packet_unref(&srcPkt);
	//				++continuity_read_error_count;
	//				if (continuity_read_error_count > 5) {// ����5������ʧ�ܣ���Ͽ�����
	//					LOGI("av_read_frame error: continuity_read_error_count=%d,srcUrl=%s", continuity_read_error_count, mSrcUrl.data());
	//					break;
	//				}
	//				else {
	//					std::this_thread::sleep_for(std::chrono::milliseconds(100));
	//				}
	//			}
	//		}

	//		// ����
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
	av_dict_set(&fmt_options, "rtsp_transport", "tcp", 0); //����rtsp�ײ�����Э�� tcp or udp
	av_dict_set(&fmt_options, "stimeout", "3000000", 0);   //����rtsp���ӳ�ʱ����λ us��
	av_dict_set(&fmt_options, "rw_timeout", "3000000", 0); //����rtmp/http-flv���ӳ�ʱ����λ us��
	av_dict_set(&fmt_options, "fflags", "discardcorrupt", 0);
	//av_dict_set(&fmt_options, "timeout", "3000000", 0);//����udp/http��ʱ����λ us��

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
		// ��������Ҫ�ͷ�start
		//if (mFmtCtx && !(mFmtCtx->oformat->flags & AVFMT_NOFILE)) {
		//    avio_close(mFmtCtx->pb);
		//}
		// ��������Ҫ�ͷ�end
		avformat_close_input(&mSrcFmtCtx);
		avformat_free_context(mSrcFmtCtx);
		mSrcFmtCtx = nullptr;
	}
}
bool StreamPusher::connectDst() {
	// ����ֱ�Ӹ� rtsp �ǿ��Եģ�������������ʱ�� -f rtsp Ӧ����һ����˼��rtmp������ᱨ��Ȼ��������rtmp������ʱ���õ� -f flv
	if (avformat_alloc_output_context2(&mDstFmtCtx, NULL, "flv", mDstUrl.data()) < 0) {
		LOGI("avformat_alloc_output_context2 error: dstUrl=%s", mDstUrl.data());
		return false;
	}

	// init video start
	// const AVCodec *videoCodec = avcodec_find_encoder(this->mSrcFmtCtx->streams[mDstVideoIndex]->codecpar->codec_id);
	// �������Ӧ���õ��������ı����ʽ
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
	//int bit_rate = 300 * 1024 * 8;  //ѹ����ÿ����Ƶ��bitλ��С 300kB
	int bit_rate = 4096000;
	// CBR��Constant BitRate - �̶�������
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

	//ABR��Average Bitrate - ƽ������
	// mDstVideoCodecCtx->bit_rate = bit_rate;

	mDstVideoCodecCtx->codec_id = videoCodec->id;
	//mDstVideoCodecCtx->pix_fmt = AV_PIX_FMT_YUVJ420P;  // ��֧��AV_PIX_FMT_BGR24ֱ�ӽ��б���
	mDstVideoCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;  // yuvj�����������ʽ�������ˣ�Ȼ��ת��Ϊyuv��ʽ����Ȼ���о���
	mDstVideoCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
	mDstVideoCodecCtx->width = mDstVideoWidth;
	mDstVideoCodecCtx->height = mDstVideoHeight;
	mDstVideoCodecCtx->time_base = { 1,mDstVideoFps };
	// mDstVideoCodecCtx->framerate = { mDstVideoFps, 1 };
	mDstVideoCodecCtx->gop_size = 5;
	mDstVideoCodecCtx->max_b_frames = 0;
	mDstVideoCodecCtx->thread_count = 1;
	mDstVideoCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;   //���PPS��SPS


	AVDictionary* video_codec_options = NULL;  // һЩ�����������еĸ��ӵĲ���
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
	// stream��time_base�����ǳ���Ҫ������ʾ����ʵ�е�һ���ӷ�Ϊ���ٸ�ʱ���, ���������avformat_write_headerʱ�Զ����
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
	av_dict_set(&fmt_options, "rw_timeout", "30000000", 0); // ����rtmp/http-flv���ӳ�ʱ����λ us��
	av_dict_set(&fmt_options, "stimeout", "30000000", 0);   // ����rtsp���ӳ�ʱ����λ us��
	av_dict_set(&fmt_options, "rtsp_transport", "tcp", 0);  // Ҳ���� udp
	// av_dict_set(&fmt_options, "fflags", "discardcorrupt", 0);
	// av_dict_set(&fmt_options, "muxdelay", "0.1", 0);
	// av_dict_set(&fmt_options, "tune", "zerolatency", 0);

	mDstFmtCtx->video_codec_id = mDstFmtCtx->oformat->video_codec;

	if (avformat_write_header(mDstFmtCtx, &fmt_options) < 0) { // ���øú����Ὣ����stream��time_base���Զ�����һ��ֵ��ͨ����1/90000��1/1000�����ʾһ���ӱ�ʾ��ʱ�������
		LOGI("avformat_write_header error: dstUrl=%s", mDstUrl.data());
		return false;
	}


	// mDstFmtCtx->audio_codec_id = AV_CODEC_ID_AAC;  // ����Ժ��������Ļ�������Ҫ����ָ����
	std::cout << "\nmy_video_codec: " << mDstFmtCtx->video_codec_id << std::endl;
	std::cout << "\nmy_audio_codec: " << mDstFmtCtx->audio_codec_id << std::endl;
	av_dump_format(mDstFmtCtx, 0, mDstUrl.c_str(), 1);

	return true;
}
void StreamPusher::closeConnectDst() {
	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	if (mDstFmtCtx) {
		// ������Ҫ�ͷ�start
		if (mDstFmtCtx && !(mDstFmtCtx->oformat->flags & AVFMT_NOFILE)) {
			avio_close(mDstFmtCtx->pb);
		}
		// ������Ҫ�ͷ�end
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