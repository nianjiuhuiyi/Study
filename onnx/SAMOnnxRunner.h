#pragma once

#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>

#include "SAMConfig.h"


typedef struct BoxInfo {
	cv::Point left_top;
	cv::Point right_bot;

	BoxInfo() {};
	BoxInfo(int left_x, int left_y, int right_x, int right_y) {
		this->left_top.x = left_x;
		left_top.y = left_y;
		this->right_bot.x = right_x;
		right_bot.y = right_y;
	};
}BoxInfo;

struct ClickInfo {
	cv::Point pt;
	bool positive;
};

struct MatInfo {
	cv::Mat mask;
	float iou_pred;
};

class SAMOnnxRunner {
private:
	// Encoder Settings Params
	bool InitModelSession = false;
	bool InitEncoderEmbedding = false;
	int EncoderInputSize = 1024;

	// Decoder Settings Params
	double SegThreshold;

	// Env Settings Params
	std::string device{"cpu"};
	Ort::Env env;
	Ort::SessionOptions session_options;
	std::unique_ptr<Ort::Session> EncoderSession;
	std::unique_ptr<Ort::Session> DecoderSession;  // 不用指针理论上也是行的，暂不知道指针的优势
	std::vector<int64_t> EncoderInputShape;
	std::vector<int64_t> EncoderOutputShape;

	// CPU MemoryInfo and memory allocation
	Ort::AllocatorWithDefaultOptions allocator;
	Ort::MemoryInfo memory_info_handler = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

	const char* DecoderInputNames[6]{ "image_embeddings", "point_coords", "point_labels", "mask_input", "has_mask_input", "orig_im_size" };
	const char* DecoderOutputNames[3]{ "masks", "iou_predictions", "low_res_masks" };

	// Image Embedding
	std::vector<float> image_embedding;
	// low_res_masks for mask_input
	std::vector<Ort::Value> tensor_buffer;

protected:
	const unsigned int num_threads;
	cv::Mat Image_PreProcess(const cv::Mat &srcImage);   // 预处理加填充图片
	bool Encoder_BuildEmbedding(const cv::Mat &Image);
	std::vector<MatInfo> Decoder_Inference(Configuration cfg, cv::Mat srcImage, ClickInfo clickinfo, BoxInfo boxinfo);

public:
	explicit SAMOnnxRunner(unsigned int num_threads = 1);
	~SAMOnnxRunner() = default;    // 跟这应该是一个意思： ~SAMOnnxRunner() {}; 代码它原来就是这么写的

	void InitOrtEnv(Configuration cfg);

	std::vector<MatInfo> InferenceSingleImage(const Configuration &cfg, const cv::Mat &srcImage, ClickInfo clickinfo, BoxInfo boxinfo);

	void setSegThreshold(float threshold);

	void ResetInitEncoder();
};

