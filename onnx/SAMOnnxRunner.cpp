#include <chrono>

#include "SAMOnnxRunner.h"
#include "transform.h"
#include "utils.h"


cv::Mat SAMOnnxRunner::Image_PreProcess(const cv::Mat &srcImage) {
	std::cout << "[INFO] PreProcess Image ..." << std::endl;
	cv::Mat rgbImage;
	cv::cvtColor(srcImage, rgbImage, cv::COLOR_BGR2RGB);

	// my: convertTo 改变 图片的数据类型，改成了float型，可拿去变tensor作运算
	cv::Mat floatImage;
	rgbImage.convertTo(floatImage, CV_32FC3);

	cv::Mat pixel_Mean = cv::Mat::ones(cv::Size(floatImage.cols, floatImage.rows), CV_32FC3);
	cv::Mat pixel_Std = cv::Mat::ones(cv::Size(floatImage.cols, floatImage.rows), CV_32FC3);

	pixel_Mean = cv::Scalar(123.675, 116.28, 103.53);
	pixel_Std = cv::Scalar(58.395, 57.12, 59.375);   // 不理解这两行为啥可以这样写

	floatImage -= pixel_Mean;
	floatImage /= pixel_Std;

	cv::Mat resizeImage = ResizeLongestSide_apply_image(floatImage, this->EncoderInputSize);

	int pad_h = this->EncoderInputSize - resizeImage.rows;
	int pad_w = this->EncoderInputSize - resizeImage.cols;

	// my: cv::copyMakeBorder 拷贝数据，应该还可以带填充边框
	cv::Mat paddingImage;
	cv::copyMakeBorder(resizeImage, paddingImage, 0, pad_h, 0, pad_w, cv::BorderTypes::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
	std::cout << "[INFO] paddingImage width : " << paddingImage.cols << ", paddingImage height : " << paddingImage.rows << std::endl;

	return paddingImage;
}

bool SAMOnnxRunner::Encoder_BuildEmbedding(const cv::Mat &Image) {
	int64_t n = this->EncoderInputShape[0];
	int64_t c = this->EncoderInputShape[1];
	int64_t h = this->EncoderInputShape[2];
	int64_t w = this->EncoderInputShape[3];

	std::vector<uint8_t> inputTensorValues(n * c * h * w);

	if (Image.size() != cv::Size(w, h)) {
		std::cerr << "[WARRING] Image size not match" << std::endl;
		std::cout << "[INFO] Image width : " << Image.cols << " Image height : " << Image.rows << std::endl;
		return false;
	}
	if (Image.channels() != 3) {
		std::cerr << "Input image is not a 3-channel image" << std::endl;
		return false;
	}
	
	std::cout << "[INFO] Encoder BuildEmbedding Start ..." << std::endl;
	for (int i = 0; i < h; i++) {
		for (int j = 0; j < w; j++) {
			inputTensorValues[i * w + j] = static_cast<uchar>(Image.at<cv::Vec3f>(i, j)[2]);
			inputTensorValues[h * w + i * w + j] = static_cast<uchar>(Image.at<cv::Vec3f>(i, j)[1]);
			inputTensorValues[2 * h * w + i * w + j] = static_cast<uchar>(Image.at<cv::Vec3f>(i, j)[0]);
		}
	}

	Ort::Value inputTensor = Ort::Value::CreateTensor<uint8_t>(this->memory_info_handler, inputTensorValues.data(), inputTensorValues.size(), this->EncoderInputShape.data(), this->EncoderInputShape.size());

	image_embedding = std::vector<float>(this->EncoderOutputShape[0] * EncoderOutputShape[1] * EncoderOutputShape[2] * EncoderOutputShape[3]);
	Ort::Value outputTensorPre = Ort::Value::CreateTensor<float>(this->memory_info_handler, image_embedding.data(), image_embedding.size(), this->EncoderOutputShape.data(), EncoderOutputShape.size());

	assert(outputTensorPre.IsTensor() && outputTensorPre.HasValue());

	const char* inputNamesPre[] = {"input"};
	const char* outputNamesPre[] = {"output"};
	
	auto time_start = std::chrono::high_resolution_clock::now();
	Ort::RunOptions run_options;
	this->EncoderSession->Run(run_options, inputNamesPre, &inputTensor, 1, outputNamesPre, &outputTensorPre, 1);
	auto time_end = std::chrono::high_resolution_clock::now();
	/*
		它原来是这么写的：
		std::chrono::duration<double> diff = time_end - time_start;
		std::cout << "Cost time : " << diff.count() << "s" << std::endl;
	*/

	long long cost_time = std::chrono::duration_cast<std::chrono::seconds>(time_end - time_start).count();
	std::cout << "[INFO] Encoder BuildEmbedding Finish ..." << std::endl;
	std::cout << "[INFO] Encoder BuildEmbedding Cost time : " << cost_time << "s" << std::endl;

	return true;
}

std::vector<MatInfo> SAMOnnxRunner::Decoder_Inference(Configuration cfg, cv::Mat srcImage, ClickInfo clickinfo, BoxInfo boxinfo) {
	int numPoints = cfg.UseBoxInfo ? 3 : 1;
	std::cout << "[INFO] ResizeLongestSide apply coordinates" << std::endl;
	ClickInfo applyCoords = ResizeLongestSide_apply_coord(srcImage, clickinfo, this->EncoderInputSize);
	float inputPointValues[] = { (float)applyCoords.pt.x, (float)applyCoords.pt.y };
	float inputLabelValues[] = { static_cast<float>(applyCoords.positive) };

	BoxInfo applyBoxs = ResizeLongestSide_apply_box(srcImage, boxinfo, this->EncoderInputSize);

	float inputPointsValues[] = { (float)applyBoxs.left_top.x, (float)applyBoxs.left_top.y, (float)applyBoxs.right_bot.x, (float)applyBoxs.right_bot.y };
	float inputLabelsValues[] = { static_cast<float>(applyCoords.positive),  (float)2, 3.0f };

	constexpr size_t maskInputSize = 256 * 256;
	float maskInputValues[maskInputSize] = { 0 };
	float hasMaskValues[] = { 0 };
	float orig_im_size_values[] = { (float)srcImage.rows, (float)srcImage.cols };
	memset(maskInputValues, 0, sizeof(maskInputValues));

	std::vector<int64_t> inputPointShape = { 1, numPoints, 2 };
	std::vector<int64_t> pointLabelsShape = { 1, numPoints };
	std::vector<int64_t> maskInputShape = { 1, 1, 256, 256 };
	std::vector<int64_t> hasMaskInputShape = { 1 };
	std::vector<int64_t> origImSizeShape = { 2 };

	std::vector<Ort::Value> inputTensorsSam;
	inputTensorsSam.push_back(Ort::Value::CreateTensor<float>(this->memory_info_handler, (float*)this->image_embedding.data(), image_embedding.size(), this->EncoderOutputShape.data(), EncoderOutputShape.size()));

	if (cfg.UseBoxInfo) {
		inputTensorsSam.push_back(Ort::Value::CreateTensor<float>(this->memory_info_handler, inputPointsValues, 2 * numPoints, inputPointShape.data(), inputPointShape.size()));
		inputTensorsSam.push_back(Ort::Value::CreateTensor<float>(memory_info_handler, inputLabelsValues, 1 * numPoints, pointLabelsShape.data(), pointLabelsShape.size()));
	}
	else {
		inputTensorsSam.push_back(Ort::Value::CreateTensor<float>(
			memory_info_handler, inputPointValues, 2, inputPointShape.data(), inputPointShape.size()));
		inputTensorsSam.push_back(Ort::Value::CreateTensor<float>(
			memory_info_handler, inputLabelValues, 1, pointLabelsShape.data(), pointLabelsShape.size()));
	}

	if (cfg.HasMaskInput && this->tensor_buffer[1] != nullptr && tensor_buffer[2] != nullptr) {
		hasMaskValues[0] = 1;
		const float *IoU_Prediction_TensorData = tensor_buffer[1].GetTensorMutableData<float>();
		float max_iou_value = 0;
		int max_iou_idx = -1;
		// 筛选出IOU最高的mask作为mask_input
		int result_length = 1 ? cfg.UseSingleMask : 4;
		for (int i = 0; i < result_length; i++) {
			if (IoU_Prediction_TensorData[i] > max_iou_value) {
				max_iou_value = IoU_Prediction_TensorData[i];
				max_iou_idx = i;
			}
			const float *MaskTensorData = tensor_buffer[2].GetTensorMutableData<float>();
			// my: 一般std::copy自己都是用在容器上，看这种写法，对数组也是OK的
			std::copy(MaskTensorData + max_iou_idx * maskInputSize, MaskTensorData + max_iou_idx * maskInputSize + maskInputSize, maskInputValues);
			std::cout << "[INFO] HasMaskInput and tensors_buffer is not empty , Use the mask output from the previous run as mask_input" << std::endl;
		}
	}
	else {
		memset(maskInputValues, 0, sizeof(maskInputValues));
	}

	inputTensorsSam.push_back(Ort::Value::CreateTensor<float>(this->memory_info_handler, maskInputValues, maskInputSize, maskInputShape.data(), maskInputShape.size()));
	inputTensorsSam.push_back(Ort::Value::CreateTensor<float>(this->memory_info_handler, hasMaskValues, 1, hasMaskInputShape.data(), hasMaskInputShape.size()));
	inputTensorsSam.push_back(Ort::Value::CreateTensor<float>(this->memory_info_handler, orig_im_size_values, 2, origImSizeShape.data(), origImSizeShape.size()));

	Ort::RunOptions runOptionsSam;

	std::cout << "[INFO] Decoder Inference Start ..." << std::endl;
	auto time_start = std::chrono::high_resolution_clock::now();
	std::vector<Ort::Value> DecoderOutputTensors = this->DecoderSession->Run(runOptionsSam, this->DecoderInputNames, inputTensorsSam.data(), inputTensorsSam.size(), this->DecoderOutputNames, 3);
	auto time_end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> diff = time_end - time_start;
	std::cout << "[INFO] Decoder Inference Finish ..." << std::endl;
	std::cout << "[INFO] Decoder Inference Cost time : " << diff.count() << "s" << std::endl;

	float *masks = DecoderOutputTensors.at(0).GetTensorMutableData<float>();
	float *iou_predictions = DecoderOutputTensors.at(1).GetTensorMutableData<float>();
	float *low_res_masks = DecoderOutputTensors.at(2).GetTensorMutableData<float>();

	Ort::Value &mask_ = DecoderOutputTensors[0];
	Ort::Value &iou_predictions_ = DecoderOutputTensors.at(1);
	Ort::Value &low_res_masks_ = DecoderOutputTensors[2];
	this->tensor_buffer = std::move(DecoderOutputTensors);
	// 我用下面容器的写法来替代会报错，看来还是不一样
	//std::move(DecoderOutputTensors.begin(), DecoderOutputTensors.end(), this->tensor_buffer.begin());

	std::vector<int64_t> mask_dims = mask_.GetTypeInfo().GetTensorTypeAndShapeInfo().GetShape();
	auto iou_pred_dims = iou_predictions_.GetTypeInfo().GetTensorTypeAndShapeInfo().GetShape();
	auto low_res_dims = low_res_masks_.GetTypeInfo().GetTensorTypeAndShapeInfo().GetShape();

	// my: 感觉这有问题，形状应该是 (n,c,h,w) 啊，下面的我w,h是不是反了
	const int Resizemasks_batch = static_cast<int>(mask_dims.at(0));
	const int Resizemasks_nums = static_cast<int>(mask_dims.at(1));
	const int Resizemasks_width = static_cast<int>(mask_dims.at(2));
	const int Resizemasks_height = static_cast<int>(mask_dims.at(3));
	std::cout << "[INFO] Resizemasks_batch : " << Resizemasks_batch << " Resizemasks_nums : " << Resizemasks_nums << " Resizemasks_width : " << Resizemasks_width << " Resizemasks_height : " << Resizemasks_height << std::endl;

	std::cout << "[INFO] Gemmiou_predictions_dim_0 : " << iou_pred_dims.at(0) << " Generate mask num : " << iou_pred_dims.at(1) << std::endl;

	std::cout << "[INFO] Reshapelow_res_masks_dim_0 : " << low_res_dims.at(0) << " Reshapelow_res_masks_dim_1 : " \
		<< low_res_dims.at(1) << std::endl;
	std::cout << "[INFO] Reshapelow_res_masks_dim_2 : " << low_res_dims.at(2) << " Reshapelow_res_masks_dim_3 : " \
		<< low_res_dims.at(3) << std::endl;

	std::vector<MatInfo> masks_list;
	for (int index = 0; index < Resizemasks_nums; index++) {
		cv::Mat mask(srcImage.rows, srcImage.cols, CV_8UC1);
		for (int i = 0; i < mask.rows; i++) {
			for (int j = 0; j < mask.cols; j++) {
				mask.at<uchar>(i, j) = masks[i * mask.cols + j + index * mask.rows * mask.cols] > 0 ? 255 : 0;
			}
		}
		MatInfo mat_info = { mask, *(iou_predictions++)};
		masks_list.emplace_back(mat_info);
	}
	return masks_list;
}

SAMOnnxRunner::SAMOnnxRunner(unsigned int num_threads) : num_threads(num_threads) { }

void SAMOnnxRunner::InitOrtEnv(Configuration cfg) {
	// 初始化OnnxRuntime运行环境
	this->env = Ort::Env(ORT_LOGGING_LEVEL_WARNING, "SegmentAnythingModel");
	this->session_options = Ort::SessionOptions();
	session_options.SetInterOpNumThreads(this->num_threads);
	// 设置图像优化级别
	session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

	this->SegThreshold = cfg.SegThreshold;
	//this->device = cfg.Device;  // cfg里还没初始化device的值，然而类定义时已经赋值了
	/* 如果用GPU的话
	if (device == "cuda") {
		OrtSessionOptionsAppendExecutionProvider_CUDA(session_options , 0);
	}
	*/

#if _WIN32
	std::cout << "[ENV] Env _WIN32 change modelpath from multi byte to wide char ..." << std::endl;

	/*   ---------- OnnxRuntime 需要的模型路径必须是 宽字符  
	使用下面这种方法，还是无法将 string 转成 wchar， 编译即便加上
	pragma #pragma warning(disable : 4996)     使得编译通过了，但是运行还是会报错。
		
	// 还要这两头文件才行
	#include <codecvt>
	#include <locale>
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t> > converter;
	std::wstring temp = converter.from_bytes(cfg.EncoderModelPath);
	const wchar_t* encoder_modelpath = temp.c_str();
	temp = converter.from_bytes(cfg.DecoderModelPath);
	const wchar_t* decoder_modelpath = temp.c_str();
	

	或是直接加 L ，参考：https://cloud.tencent.com/developer/article/1177239
	const wchar_t* model_path = L"I:\\SAM-ToolTable\\sam_onnx.onnx";
	Ort::Session session(env, model_path, Ort::SessionOptions{nullptr});  // cpu

	*/

	const wchar_t* encoder_modelpath = multi_Byte_To_Wide_Char(cfg.EncoderModelPath);
	const wchar_t* decoder_modelpath = multi_Byte_To_Wide_Char(cfg.DecoderModelPath);
#else
	const char* encoder_modelpath = cfg.EncoderModelPath;
	const char* decoder_modelpath = cfg.DecoderModelPath;
#endif  // _WIN32

	// 创建Session并加载模型进内存
	std::cout << "[INFO] Building SegmentAnything Model Encoder ... " << std::endl;
	this->EncoderSession = std::make_unique<Ort::Session>(env, encoder_modelpath, session_options);
	if (EncoderSession->GetInputCount() != 1 || EncoderSession->GetOutputCount() != 1) {
		std::cerr << "[INFO] Preprocessing model not loaded (invalid input/output count)" << std::endl;
		return;
	}
	std::cout << "[INFO] Building SegmentAnything Model Decoder ... " << std::endl;
	this->DecoderSession = std::make_unique<Ort::Session>(env, decoder_modelpath, session_options);
	if (DecoderSession->GetInputCount() != 6 || DecoderSession->GetOutputCount() != 3) {
		std::cerr << "[INFO] Model not loaded (invalid input/output count)" << std::endl;
		return;
	}

	this->EncoderInputShape = EncoderSession->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
	this->EncoderOutputShape = EncoderSession->GetOutputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
	if (EncoderInputShape.size() != 4 || EncoderOutputShape.size() !=4) {
		std::cerr << "[ERROR] Preprocessing model not loaded (invalid shape)" << std::endl;
		return;
	}

	std::cout << "[INFO] Build EncoderSession and DecoderSession successfully." << std::endl;
	this->InitModelSession = true;

	delete encoder_modelpath;
	delete decoder_modelpath;
}

std::vector<MatInfo> SAMOnnxRunner::InferenceSingleImage(const Configuration &cfg, const cv::Mat &srcImage, ClickInfo clickinfo, BoxInfo boxinfo) {
	if (srcImage.empty()) {
		throw "[ERROR] srcImage empty !";       // 居然可以直接这样抛出
	}
	std::cout << "[INFO] srcImage width : " << srcImage.cols << " srcImage height :  " << srcImage.rows << std::endl;

	cv::Mat rgbImage = this->Image_PreProcess(srcImage);
	if (!this->InitEncoderEmbedding) {
		std::cout << "[INFO] Preprocess before encoder image embedding ... " << std::endl;
		this->InitEncoderEmbedding = this->Encoder_BuildEmbedding(rgbImage);
	}

	std::vector<MatInfo> result = this->Decoder_Inference(cfg, srcImage, clickinfo, boxinfo);
	if (result.empty()) {
		throw "[ERROR] No result !";
	}
	else {
		std::sort(result.begin(), result.end(), [](const MatInfo &a, const MatInfo &b) { return a.iou_pred > b.iou_pred; });
	}
	std::cout << "[INFO] Generate result mask size is " << result.size() << std::endl;

	unsigned int index = 0;
	for (unsigned int i = 0; i < result.size(); i++)
	{	
		if (result[i].iou_pred < cfg.SegThreshold)
		{
			std::cout << "[INFO] Result IoU prediction lower than segthreshold , only " << result[i].iou_pred << std::endl;
			continue;
		}
		std::string save_path = cfg.SaveDir + "/result_" + std::to_string(i) +".png";
		cv::imwrite(save_path, result[i].mask);
		std::cout << "[INFO] Result save as " << save_path << " Iou prediction is " << result[i].iou_pred << std::endl;
	}
	return result;
}

void SAMOnnxRunner::setSegThreshold(float threshold) {
	this->SegThreshold = threshold;
}

void SAMOnnxRunner::ResetInitEncoder() {
	this->InitEncoderEmbedding = false;
}
