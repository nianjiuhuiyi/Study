#include <iostream>
#include <fstream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <string>

#include <filesystem>

#include <opencv2/opencv.hpp>

#include "httplib.h"
#include "safequeue.hpp"

#include <windows.h>  // 这一定要放最后，放前面会影响其它头文件的定义，仅针对win

extern "C" {

#include <libavformat/avformat.h>
#pragma comment(lib, "avformat.lib")

#include <libavcodec/avcodec.h>
#pragma comment(lib, "avcodec.lib")

#include <libswresample/swresample.h>  // swr_alloc() 函数，音频重采样器需要
#pragma comment(lib, "swresample.lib")

#include <libswscale/swscale.h>
#pragma comment(lib, "swscale.lib")

#include <libavutil/opt.h>   // av_opt_set_int 函数需要
}

// 全局变量
std::queue<std::vector<uint8_t>> audio_queue; // 音频数据队列
std::mutex audio_mutex;                      // 音频队列互斥锁
std::condition_variable audio_cv;            // 音频队列条件变量
std::atomic<bool> stop_audio_thread{ false };  // 停止音频线程标志


// 存音频文件的路径
SafeQueue<std::string> wave_queue;



// 音频参数
struct audioParam {
    unsigned int sample_rate;  // 采样率
    unsigned int channels;     // 声道数
    unsigned int bits_per_sample;  // 每个采样点的位数
    unsigned int duration;  // 录音秒数（这个不一定要进进来这里）
};

// 指定音频采样格式
audioParam audio_param = {
    32000,   // 44100 也是ok
    2,
    16,
    5  // 5秒一个文件，来这里改时间
};



static double r2d(AVRational r) {
    return r.den == 0 ? 0 : (double)r.num / (double)r.den;
}

// 保存音频数据为 WAV 文件
void save_audio_to_wav(const std::vector<uint8_t> &audio_data, const std::string &filename) {
    std::ofstream wav_file(filename, std::ios::binary);
    if (!wav_file) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    // WAV 文件头
    struct WavHeader {
        char chunk_id[4] = { 'R', 'I', 'F', 'F' };
        uint32_t chunk_size;
        char format[4] = { 'W', 'A', 'V', 'E' };
        char subchunk1_id[4] = { 'f', 'm', 't', ' ' };
        uint32_t subchunk1_size = 16;
        uint16_t audio_format = 1;
        uint16_t num_channels = audio_param.channels;
        uint32_t sample_rate = audio_param.sample_rate;
        uint32_t byte_rate = audio_param.sample_rate * audio_param.channels * (audio_param.bits_per_sample / 8);
        uint16_t block_align = audio_param.channels * (audio_param.bits_per_sample / 8);
        uint16_t bits_per_sample = audio_param.bits_per_sample;
        char subchunk2_id[4] = { 'd', 'a', 't', 'a' };
        uint32_t subchunk2_size;
    };

    WavHeader header;
    header.chunk_size = 36 + audio_data.size();
    header.subchunk2_size = audio_data.size();

    wav_file.write(reinterpret_cast<char *>(&header), sizeof(header));
    wav_file.write(reinterpret_cast<const char *>(audio_data.data()), audio_data.size());
    wav_file.close();

    wave_queue.push(filename);
}

// 音频数据处理函数
void process_audio_buffer(std::vector<uint8_t> &audio_buffer, unsigned int &audio_chunk_size) {

    while (!stop_audio_thread) {
        std::unique_lock<std::mutex> lock(audio_mutex);
        audio_cv.wait(lock, [] { return !audio_queue.empty() || stop_audio_thread; });

        if (stop_audio_thread) break;

        while (!audio_queue.empty()) {
            auto data = audio_queue.front();
            audio_queue.pop();
            audio_buffer.insert(audio_buffer.end(), data.begin(), data.end());

            if (audio_buffer.size() >= audio_chunk_size) {
                // 保存音频数据为 WAV 文件
                auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
                std::string filename = "audio_" + std::to_string(timestamp) + ".wav";
                save_audio_to_wav(audio_buffer, filename);
                audio_buffer.clear();
            }
        }
    }
}


std::string UTF8ToGB2312(const std::string &utf8Str) {
    // Step 1: Convert UTF-8 to UTF-16 (wide char)
    int wideCharLen = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, nullptr, 0);
    if (wideCharLen == 0) {
        throw std::runtime_error("Failed to convert UTF-8 to UTF-16");
    }
    std::wstring wideStr(wideCharLen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, &wideStr[0], wideCharLen);

    // Step 2: Convert UTF-16 to GB2312
    int gb2312Len = WideCharToMultiByte(CP_ACP, 0, wideStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (gb2312Len == 0) {
        throw std::runtime_error("Failed to convert UTF-16 to GB2312");
    }
    std::string gb2312Str(gb2312Len, '\0');
    WideCharToMultiByte(CP_ACP, 0, wideStr.c_str(), -1, &gb2312Str[0], gb2312Len, nullptr, nullptr);

    return gb2312Str;
}

// 发送数据
void send_audio() {
    std::unique_ptr<httplib::Client> client = std::make_unique<httplib::Client>("192.168.108.149", 6789);
    
    while (!stop_audio_thread) {
        std::string file_name = wave_queue.get();
        std::ifstream file(file_name, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to open file" << std::endl;
            continue;
        }
        // 读取文件内容
        std::string file_content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        // 创建MultipartFormDataItems对象
        httplib::MultipartFormDataItems items = {
            {"audio", file_content, file_name, "audio/wav"}
        };
        auto res = client->Post("/upload", items);
        file.close();

        // 检查请求是否成功
        if (res && res->status == 200) {
            std::string utf8_string = res->body;
            std::cout << "File uploaded successfully: " << UTF8ToGB2312(utf8_string) << std::endl;

            std::filesystem::remove(file_name);

        }
        else {
            std::cerr << "Failed to upload file" << std::endl;
            if (res) {
                std::cerr << "Status code: " << res->status << std::endl;
                std::cerr << "Response body: " << res->body << std::endl;
            }
        }
        
    }
   
}


struct Color_RGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

// 写一个转换颜色编码的函数
static void GetRGBPixels(AVFrame *frame, std::vector<Color_RGB> &buffer) {
    static SwsContext *swsctx = nullptr;
    swsctx = sws_getCachedContext(swsctx,
        frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
        frame->width, frame->height, AVPixelFormat::AV_PIX_FMT_BGR24, NULL, NULL, NULL, NULL
    );  // 这里原来的类型转换是用的 (AVPixelFormat)frame->format

    // 每次循环调用这个函数，都会重新分配这个vector，debug下就很慢
    //std::vector<Color_RGB> buffer(frame->width * frame->height);

    //uint8_t* data[] = {(uint8_t*)&buffer[0]};
    uint8_t *data[] = { reinterpret_cast<uint8_t *>(&buffer[0]) };  // c++类型的指针风格转换
    int linesize[] = { frame->width * 3 };
    // sws_scale 函数可以对画面进行缩放，同时还能改变颜色编码，
    sws_scale(swsctx, frame->data, frame->linesize, 0, frame->height, data, linesize);
    // return buffer;  // 不返回了，直接用buffer
}

/* 
1、这里是读取了本地一个带有音频的视频文件，
    做画面展示的同时，还将音频文件每5秒存成一个wav文件，
    再将这个文件传至rk3588上的ASR语音识别服务，返回对应字符串并做展示。
2、以后这大概率跑不起来，因为语音室识别服务大概率没开，
    所以把“std::thread process_thread(send_audio);”这个线程注释掉就好了，
    但要注意，注释后，就不会自动删除存在本地的音频文件了，要去手动删除，不然就越来越多。
3、把这里的main函数改成其它名字，把“ffmpeg_opencvShow.cpp”的main123改回main就是运行那个仅画面展示的demo
*/
int main() {
    // 初始化 FFmpeg
    avformat_network_init();

    //const char *input = "rtmp://192.168.108.218/live/123";
    const char *input = "C:\\Users\\Administrator\\Videos\\audio.mp4";
    //const char *input = "C:\\Users\\Administrator\\Pictures\\001.mp4";

    // 打开 RTMP 流
    AVFormatContext *format_ctx = avformat_alloc_context();

    int ret = 0;
    do {
        ret = avformat_open_input(&format_ctx, input, nullptr, nullptr);
        if (ret != 0) {
            char buf[1024] = { 0 };
            av_strerror(ret, buf, sizeof(buf) - 1);
            std::cerr << "Failed to open RTMP stream" << std::endl;
            break;
        }

        if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
            std::cerr << "Failed to find stream info" << std::endl;
            break;
        }

        int videoIdx = av_find_best_stream(format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if (videoIdx < 0) {
            std::cout << "av_find_best_stream error: " << av_get_media_type_string(AVMEDIA_TYPE_VIDEO);
            break;
        }
        int audioIdx = av_find_best_stream(format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (audioIdx < 0) {
            std::cout << "av_find_best_stream error: " << av_get_media_type_string(AVMEDIA_TYPE_AUDIO);
            break;
        }

        AVStream *video_stream = format_ctx->streams[videoIdx];  // video_stream->index == videoIdx;
        AVStream *audio_stream = format_ctx->streams[audioIdx];
        
        const int &src_w = video_stream->codecpar->width;
        const int &src_h = video_stream->codecpar->height;

        // video_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO;
        // audio_stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO;
        // 打印一些基本信息,视频
        double fps = r2d(video_stream->avg_frame_rate);
        std::cout << "视频帧率: " << fps << "fps.\n";
        if (AV_CODEC_ID_MPEG4 == video_stream->codecpar->codec_id) {
            std::cout << "视频压缩编码格式: MPEG4" << std::endl;
        }
        else if (AV_CODEC_ID_HEVC == video_stream->codecpar->codec_id) {
            std::cout << "视频压缩编码格式: h265" << std::endl;
        }
        std::cout << "视频(w, h)： (" << src_w << ", " << src_h << ")\n";

        // 音频
        std::cout << "音频采样率: " << audio_stream->codecpar->sample_rate << "Hz.\n";
        // 下面这已经被5.1丢弃了，编译会报错(点进去看头文件，会有说明的deprecated，然后建议你怎么使用)
        //std::cout << "音频通道数: " << as->codecpar->channels << std::endl;
        std::cout << "音频通道数: " << audio_stream->codecpar->ch_layout.nb_channels << std::endl;
        // 这应该就是上面 “每个采样点的位数” 一般好像都是16
        std::cout << "音频的 bits_per_coded_sample: " << audio_stream->codecpar->bits_per_coded_sample << std::endl;
        // 音频采样格式
        if (AV_SAMPLE_FMT_FLTP == audio_stream->codecpar->format) {
            std::cout << "音频采样格式：AV_SAMPLE_FMT_FLTP\n";
        }
        else if (AV_SAMPLE_FMT_S16P == audio_stream->codecpar->format) {
            std::cout << "音频采样格式：AV_SAMPLE_FMT_S16P\n";
        }
        // 音频压缩编码格式
        if (AV_CODEC_ID_AAC == audio_stream->codecpar->codec_id) {
            std::cout << "音频压缩编码格式：AV_CODEC_ID_AAC\n";
        }
        else if (AV_CODEC_ID_MP3 == audio_stream->codecpar->codec_id) {
            std::cout << "音频压缩编码格式：AV_CODEC_ID_MP3\n";
        }

        // 创建一个图
        cv::Mat image(cv::Size(src_w, src_h), CV_8UC3);
        std::vector<Color_RGB> buffer(src_w * src_h);

        
        // 计算5秒的大小
        unsigned int audio_chunk_size = audio_param.sample_rate * audio_param.channels * (audio_param.bits_per_sample / 8) * audio_param.duration;
        std::vector<uint8_t> audio_buffer;
        audio_buffer.reserve(audio_chunk_size);


        // 初始化视频解码器
        AVCodecParameters *video_codecpar = video_stream->codecpar;
        AVCodec *video_codec = const_cast<AVCodec*>(avcodec_find_decoder(video_codecpar->codec_id));
        AVCodecContext *video_codec_ctx = avcodec_alloc_context3(video_codec);
        avcodec_parameters_to_context(video_codec_ctx, video_codecpar);
        avcodec_open2(video_codec_ctx, video_codec, nullptr);

        // 初始化音频解码器
        AVCodecParameters *audio_codecpar = audio_stream->codecpar;
        AVCodec *audio_codec = const_cast<AVCodec*>(avcodec_find_decoder(audio_codecpar->codec_id));
        AVCodecContext *audio_codec_ctx = avcodec_alloc_context3(audio_codec);
       
        avcodec_parameters_to_context(audio_codec_ctx, audio_codecpar);
        avcodec_open2(audio_codec_ctx, audio_codec, nullptr);


        // 初始化音频重采样器
        SwrContext *swr_ctx = swr_alloc();
        if (!swr_ctx) {
            std::cerr << "Failed to allocate SwrContext" << std::endl;
            break;
        }
        // 设置输入声道布局
        AVChannelLayout in_chlayout = audio_codecpar->ch_layout;
        if (in_chlayout.order == AV_CHANNEL_ORDER_UNSPEC) {
            // 如果声道布局未指定，使用默认布局
            av_channel_layout_default(&in_chlayout, audio_codecpar->ch_layout.nb_channels);
        }
        // 设置输入参数
        av_opt_set_chlayout(swr_ctx, "in_chlayout", &in_chlayout, 0);
        av_opt_set_int(swr_ctx, "in_sample_rate", audio_codecpar->sample_rate, 0);
        av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", audio_codec_ctx->sample_fmt, 0);
        // 设置输出参数
        av_opt_set_chlayout(swr_ctx, "out_chlayout", &audio_codecpar->ch_layout, 0);
        av_opt_set_int(swr_ctx, "out_sample_rate", audio_param.sample_rate, 0);
        av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);  // 暂时只能是 AV_SAMPLE_FMT_S16;
        

        // 初始化 SwrContext
        if (swr_init(swr_ctx) < 0) {
            std::cerr << "Failed to initialize SwrContext" << std::endl;
            break;
        }

        // 音频处理线程
        std::thread audio_thread(process_audio_buffer, std::ref(audio_buffer), std::ref(audio_chunk_size));

        // 把本地音频文件发送到语音识别服务
        std::thread process_thread(send_audio);

        int pkt_count = 0;
        int print_max_count = 5000;
        std::cout << "\n---------av_read_frames start\n";
        // 读取帧
        AVPacket *packet = av_packet_alloc();
        AVFrame *frame = av_frame_alloc();
        while (av_read_frame(format_ctx, packet) >= 0) {

            //if (pkt_count++ > print_max_count) break;

            if (packet->stream_index == videoIdx) {
                // 解码视频帧
                if (avcodec_send_packet(video_codec_ctx, packet) == 0) {
                    while (avcodec_receive_frame(video_codec_ctx, frame) == 0) {
                        // 将帧转换为 OpenCV 格式并显示
                        //cv::Mat image(frame->height, frame->width, CV_8UC3, frame->data[0], frame->linesize[0]);
                        GetRGBPixels(frame, buffer);  // 解码调用
                        image.data = reinterpret_cast<uint8_t *>(&buffer[0]);
                        cv::imshow("Video", image);
                        //cv::waitKey(33);
                        cv::waitKey(static_cast<int>(1000.0 / fps));
                    }
                }
            }
            else if (packet->stream_index == audioIdx) {
                // 解码音频帧
                if (avcodec_send_packet(audio_codec_ctx, packet) == 0) {
                    while (avcodec_receive_frame(audio_codec_ctx, frame) == 0) {
                        // 检查音频帧数据是否有效
                        if (frame->data[0] == nullptr || frame->nb_samples <= 0) {
                            std::cerr << "Invalid audio frame data" << std::endl;
                            continue;
                        }

                        // 分配输出缓冲区
                        int out_samples = swr_get_out_samples(swr_ctx, frame->nb_samples);
                        std::vector<uint8_t> output_buffer(out_samples * audio_param.channels * (audio_param.bits_per_sample / 8));

                        // 获取输出缓冲区的指针
                        uint8_t *output_ptr = output_buffer.data(); 

                        // 重采样音频数据
                        int ret = swr_convert(swr_ctx, &output_ptr, out_samples, (const uint8_t **)frame->data, frame->nb_samples);
                        if (ret < 0) {
                            std::cerr << "Failed to resample audio" << std::endl;
                            continue;
                        }

                        // 计算实际输出的音频数据大小
                        int out_size = ret * audio_param.channels * (audio_param.bits_per_sample / 8);

                        // 将音频数据放入队列
                        std::unique_lock<std::mutex> lock(audio_mutex);
                        audio_queue.push(std::vector<uint8_t>(output_buffer.begin(), output_buffer.begin() + out_size));
                        audio_cv.notify_one();
                    }
                }
            }
            av_packet_unref(packet);
        }

        // 清理资源
        stop_audio_thread.exchange(true);
        audio_cv.notify_all();
        audio_thread.join();
        process_thread.join();


        av_packet_free(&packet);
        av_frame_free(&frame);

        avcodec_free_context(&video_codec_ctx);
        avcodec_free_context(&audio_codec_ctx);
        swr_free(&swr_ctx);

    } while (0);

    
    // 要对应去释放
    if (format_ctx) {
        avformat_close_input(&format_ctx);
        avformat_free_context(format_ctx);
    }


    avformat_network_deinit();

    std::cout << "ok" << std::endl;
    system("pause");

    return 0;
}
