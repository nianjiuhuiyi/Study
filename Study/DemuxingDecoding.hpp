#pragma once

#include <iostream>
#include <fstream>

extern "C" {
#include <libavcodec/avcodec.h>
#pragma comment(lib, "avcodec.lib")

#include <libavformat/avformat.h>
#pragma comment(lib, "avformat.lib")

#include <libavutil/imgutils.h>            //av_image_alloc 需要
#include <libavutil/timestamp.h>   // av_ts2timestr 需要
#pragma comment(lib, "avutil.lib")

}

#define AV_IN_OUT
#define AV_OUT


class DemuxingDecoding {
public:
	DemuxingDecoding(const char* src_filename, const char* video_dst_filename, const char* audio_dst_filename) : mSrc_filename(src_filename), mVideo_dst_filename(video_dst_filename), mAudio_dst_filename(audio_dst_filename) { }

	~DemuxingDecoding() {
		this->finalize();
	}

public:
	int run() {
		int ret = 0;
		AVStream *video_stream = nullptr;
		AVStream *audio_stream = nullptr;

		// 初始化一些参数
		ret = this->initialize();
		if (ret != 0) {
			std::cout << "初始化失败..." << std::endl;
			return EXIT_FAILURE;
		}

		/* open input file, and allocate format context */
		if (avformat_open_input(&mFmt_ctx, mSrc_filename, nullptr, nullptr) < 0) {
			std::cerr << "Could not open source file: " << mSrc_filename << std::endl;
			return EXIT_FAILURE;
		}

		/* retrieve stream information */
		if (avformat_find_stream_info(mFmt_ctx, nullptr) < 0) {
			std::cerr << "Could not find stream information\n";
			return EXIT_FAILURE;
		}

		// 获取视频编解码器的数据
		if (this->open_codec_context(mFmt_ctx, AVMEDIA_TYPE_VIDEO, &mVideo_dec_ctx, &mVideo_stream_idx) >= 0) {
			video_stream = mFmt_ctx->streams[mVideo_stream_idx];

			mVideo_dst_file.open(mVideo_dst_filename, std::ios::out | std::ios::binary);
			if (!mVideo_dst_file.is_open()) {
				std::cerr << "Could not open destination file " << mVideo_dst_filename << std::endl;
				return 1;
			}

			/* allocate image where the decoded image will be put */
			mWidth = mVideo_dec_ctx->width;
			mHeight = mVideo_dec_ctx->height;
			mPix_fmt = mVideo_dec_ctx->pix_fmt;
			// 这个函数就是返回的buffer size
			ret = av_image_alloc(mVideo_dst_data, mVideo_dst_linesize, mWidth, mHeight, mPix_fmt, 1);
			if (ret < 0) {
				std::cerr << "Could not allocate raw video buffer\n";
				return EXIT_FAILURE;
			}
			mVideo_dst_bufsize = ret;
		}
	

		// 获取音频编解码器的数据
		if ((this->open_codec_context(mFmt_ctx, AVMEDIA_TYPE_AUDIO, &mAudio_dec_ctx, &mAudio_stream_idx)) >= 0) {
			audio_stream = mFmt_ctx->streams[mAudio_stream_idx];

			mAudio_dst_file.open(mAudio_dst_filename, std::ios::out | std::ios::binary);
			if (!mAudio_dst_file.is_open()) {
				std::cerr << "Could not open destination file " << mAudio_dst_filename << std::endl;
				return 1;
			}
		}

		/* 打印输入文件的信息 */
		av_dump_format(mFmt_ctx, 0, mSrc_filename, 0);

		if (!video_stream && !audio_stream) {
			std::cerr << "Could not find audio or video stream in the input, aborting\n";
			return EXIT_FAILURE;
		}
		if (video_stream)
			std::cout << "Demuxing video from file " << mSrc_filename << " into " << mVideo_dst_filename;
		if (audio_stream)
			std::cout << "Demuxing audio from file " << mSrc_filename << " into " << mAudio_dst_filename;

		/* read frames from the file */
		while (av_read_frame(mFmt_ctx, mPkt) >= 0) {
			// 仅处理音频、视频流
			if (mPkt->stream_index == mVideo_stream_idx) {
				ret = this->decode_packet(mVideo_dec_ctx, mPkt);
			}
			else if (mPkt->stream_index == mAudio_stream_idx) {
				ret = this->decode_packet(mAudio_dec_ctx, mPkt);
			}
			// 跟frame一样，一帧的packet用完后，要及时unref，不要最后再去，不然可能会内存泄露
			av_packet_unref(mPkt);
			if (ret < 0) break;
		}

		/* flush the decoders */
		if (mVideo_dec_ctx) {
			this->decode_packet(mVideo_dec_ctx, nullptr);
		}
		if (mAudio_dec_ctx) {
			this->decode_packet(mAudio_dec_ctx, nullptr);
		}
		std::cout << "Demuxing successed.\n";

		if (video_stream) {
			std::cout << "Play the output video file with the command:\n"
				<< "ffplay -f rawvideo -pix_fmt " << av_get_pix_fmt_name(mPix_fmt)
				<< " -video_size " << mWidth << "x" << mHeight << " " << mVideo_dst_filename << std::endl;
		}
		if (audio_stream) {
			enum AVSampleFormat &sfmt = mAudio_dec_ctx->sample_fmt;
			int n_channels = mAudio_dec_ctx->ch_layout.nb_channels;
			const char* fmt;

			if (av_sample_fmt_is_planar(sfmt)) {
				const char* packed = av_get_sample_fmt_name(sfmt);
				std::cout << "Warning: the sample format the decoder produced is planar: " << (packed ? packed : "?") << "This example will output the first channel only.\n";
				sfmt = av_get_packed_sample_fmt(sfmt);
				n_channels = 1;
			}
			if (this->get_format_from_sample_fmt(&fmt, sfmt) < 0) {
				return 1;
			}
			std::cout << "Play the output audio file with the command:\n"
				<< "ffplay -f " << fmt << " -ac " << n_channels << " -ar " << mAudio_dec_ctx->sample_rate << " " << mAudio_dst_filename;
		}
	}

private:
	int initialize() {
		mFmt_ctx = avformat_alloc_context();
		mVideo_dec_ctx = nullptr;
		mAudio_dec_ctx = nullptr;

		mVideo_stream_idx = -1;
		mAudio_stream_idx = -1;

		mFrame = av_frame_alloc();
		if (!mFrame) {
			std::cerr << "Could not allocate frame\n";
			return AVERROR(ENOMEM);
		}

		mPkt = av_packet_alloc();
		if (!mPkt) {
			std::cerr << "Could not allocate packet\n";
			return AVERROR(ENOMEM);
		}

		mVideo_frame_count = 0;
		mAudio_frame_count = 0;

		return 0;
	}

	void finalize() {
		if (mFmt_ctx) {
			avformat_close_input(&mFmt_ctx);
			avformat_free_context(mFmt_ctx);
		}

		// 因为自定义函数中用了 avcodec_alloc_context3 ,所以要去释放编解码器的上下文
		if (mVideo_dec_ctx)
			avcodec_free_context(&mVideo_dec_ctx);
		if (mAudio_dec_ctx)
			avcodec_free_context(&mAudio_dec_ctx);

		if (mVideo_dst_file)
			mVideo_dst_file.close();

		if (mAudio_dst_file)
			mAudio_dst_file.close();

		if (mFrame)
			av_frame_free(&mFrame);

		if (mPkt)
			av_packet_free(&mPkt);

		av_free(mVideo_dst_data[0]);
	}

	// 用来获取音频、视频解码器的上下文具体信息
	// 用const来标记输入的参数是只读的，用指针或是引用不加const，表示这参数是用来输出的
	// 这里一定要用 AVCodecContext **dec_ctx 这样的二级指针，用 AVCodecContext *dec_ctx这的话，明明全部运行正确，但运行后传递进来的mAudio_dec_ctx指针依旧是空指针
	int open_codec_context(AV_IN_OUT AVFormatContext *fmt_ctx, AV_IN_OUT enum AVMediaType type, AV_OUT AVCodecContext **dec_ctx, AV_OUT int *stream_idx) {
		int ret = 0;
		int stream_index = 0;

		AVStream *stream = nullptr;
		const AVCodec *dec = nullptr;

		const char* media_type_str = av_get_media_type_string(type);

		ret = av_find_best_stream(fmt_ctx, type, -1, -1, nullptr, 0);
		if (ret < 0) {
			std::cerr << "Could not find stream of " << media_type_str << std::endl;
			return ret;
		}

		stream_index = ret;
		stream = fmt_ctx->streams[stream_index];

		/* find decoder for the stream */
		// 别忘了编码用到的类似的，因为编码一般用传入的格式指定，avcodec_find_encoder_by_name(const char*name); 这里解码，因为流里已经带了编码格式，这里直接就查找出来，用来解码
		dec = avcodec_find_decoder(stream->codecpar->codec_id);
		if (!dec) {
			std::cerr << "Failed to find codec of " << media_type_str << std::endl;
			return AVERROR(EINVAL);
		}

		/* Allocate a codec context for the decoder */
		// TODO: 后面记得要去释放
		*dec_ctx = avcodec_alloc_context3(dec);
		if (!*dec_ctx) {
			std::cerr << "Failed to allocate the codec context of " << media_type_str << std::endl;
			return AVERROR(ENOMEM);
		}

		/* Copy codec parameters from input stream to output codec context */
		if ((ret = avcodec_parameters_to_context(*dec_ctx, stream->codecpar)) < 0) {
			std::cerr << "Failed to copy " << media_type_str << " codec parameters to decoder context\n";
			return ret;
		}

		/* Init the decoders */
		if ((ret = avcodec_open2(*dec_ctx, dec, nullptr)) < 0) {
			std::cerr << "Failed to open codec of " << media_type_str << std::endl;
			return ret;
		}

		*stream_idx = stream_index;
		return 0;
	}

	// 用来进行音频、视频的packet解码
	int decode_packet(AVCodecContext *dec, const AVPacket *pkt) {
		int ret = 0;

		ret = avcodec_send_packet(dec, pkt);
		if (ret < 0) {
			std::cerr << "Error submitting a packet for decoding: " << av_err2str(ret);
			return ret;
		}

		// get all the available frames from the decoder
		while (ret >= 0) {
			ret = avcodec_receive_frame(dec, mFrame);
			if (ret < 0) {
				// 这是两个特殊值，意味着没有输出
				if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
					return 0;

				std::cerr << "Error during decoding: " << av_err2str(ret);
				return ret;
			}

			// write the frame data to output file
			if (dec->codec->type == AVMEDIA_TYPE_VIDEO) {
				ret = this->output_video_frame(mFrame);
			}
			else if (dec->codec->type == AVMEDIA_TYPE_AUDIO) {
				ret = this->output_audio_frame(mFrame);
			}

			// 一帧数据用完后，一定要unref，之前写的内存泄露可能就是因为这
			av_frame_unref(mFrame);
			if (ret < 0) return ret;
		}
		return 0;
	}

	// 内部函数 decode_packet 里要用的，把这部分代码封装了
	int output_video_frame(AVFrame *frame) {
		if (frame->width != mWidth || frame->height != mHeight || frame->format != mPix_fmt) {
			std::cerr << "Error: Width, height and pixel format have to be constant in a rawvideo file, but the width, height or pixel format of the input video changed：\n"
				<< "old: width=" << mWidth << ", height=" << mHeight << ", format=" << av_get_pix_fmt_name(mPix_fmt)
				<< "new: width=" << frame->width << ", height=" << frame->height << ", format=" << av_get_pix_fmt_name(static_cast<AVPixelFormat>(frame->format));
			return -1;
		}

		std::cout << "video_frame n: " << mVideo_frame_count++ << " coded_n: " << frame->coded_picture_number << std::endl;

		/* copy decoded frame to destination buffer:
		* this is required since rawvideo expects non aligned data */
		// 源代码是用的 (const uint8_t **)(frame->data)
		av_image_copy(mVideo_dst_data, mVideo_dst_linesize, const_cast<const uint8_t **>(frame->data), frame->linesize, mPix_fmt, mWidth, mHeight);

		/* write to rawvideo file */
		mVideo_dst_file.write(reinterpret_cast<const char *>(mVideo_dst_data[0]), mVideo_dst_bufsize);
		return 0;
	}

	// 
	int output_audio_frame(AVFrame *frame) {
		size_t unpadded_linesize = frame->nb_samples *
			av_get_bytes_per_sample(static_cast<AVSampleFormat>(frame->format));

		/*
		av_ts2timestr(frame->pts, &mAudio_dec_ctx->time_base) 要去改源码的字符串初始化方式，
		*/
		char my_error[AV_TS_MAX_STRING_SIZE] = { 0 };
		std::cout << "audio_frame n: " << mAudio_frame_count++ << " nb_samples: " << frame->nb_samples << " pts: " << av_ts_make_time_string(my_error, frame->pts, &mAudio_dec_ctx->time_base) << std::endl;;

		/* Write the raw audio data samples of the first plane. This works
		 * fine for packed formats (e.g. AV_SAMPLE_FMT_S16). However,
		 * most audio decoders output planar audio, which uses a separate
		 * plane of audio samples for each channel (e.g. AV_SAMPLE_FMT_S16P).
		 * In other words, this code will write only the first audio channel
		 * in these cases.
		 * You should use libswresample or libavfilter to convert the frame
		 * to packed data. */
		mAudio_dst_file.write(reinterpret_cast<const char *>(frame->extended_data[0]), unpadded_linesize);

		return 0;
	}

	// 纯为音频，了解吧
	int get_format_from_sample_fmt(const char* *fmt, enum AVSampleFormat sample_fmt) {

		struct sample_fmt_entry {
			enum AVSampleFormat sample_fmt;
			const char* fmt_be;
			const char* fmt_le;
		};
		struct sample_fmt_entry sample_fmt_entries[] = {
			{ AV_SAMPLE_FMT_U8,  "u8",    "u8"    },
			{ AV_SAMPLE_FMT_S16, "s16be", "s16le" },
			{ AV_SAMPLE_FMT_S32, "s32be", "s32le" },
			{ AV_SAMPLE_FMT_FLT, "f32be", "f32le" },
			{ AV_SAMPLE_FMT_DBL, "f64be", "f64le" },
		};
		*fmt = nullptr;

		for (int i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); ++i) {
			struct sample_fmt_entry *entry = &sample_fmt_entries[i];
			if (sample_fmt == entry->sample_fmt) {
				*fmt = AV_NE(entry->fmt_be, entry->fmt_le);
				return 0;
			}
		}

		std::cerr << "sample format " << av_get_sample_fmt_name(sample_fmt) << " is not supported as output format\n";
		return -1;
	}


private:

	const char* mSrc_filename;  // 输入的带音频的视频文件路径
	const char* mVideo_dst_filename;
	const char* mAudio_dst_filename;

	AVFormatContext *mFmt_ctx;
	AVCodecContext *mVideo_dec_ctx;
	AVCodecContext *mAudio_dec_ctx;

	AVFrame *mFrame;
	AVPacket *mPkt;

	int mVideo_stream_idx;
	int mAudio_stream_idx;

	std::ofstream mVideo_dst_file;
	std::ofstream mAudio_dst_file;

	int mWidth;
	int mHeight;
	enum AVPixelFormat mPix_fmt;

	// 视频里说，这你用[4]，是Y、U、V各占一个，还剩下一个是作为扩展
	uint8_t* mVideo_dst_data[4] = { nullptr };
	int mVideo_dst_linesize[4];
	int mVideo_dst_bufsize;

	int mVideo_frame_count;
	int mAudio_frame_count;
};
