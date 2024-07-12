/*
1、运行的命令行是：--encoder_model_path  "F:\onnx_models\vit_b\encoder\sam_vit_b_01ec64_encoder.onnx"   --decoder_model_path  "F:\onnx_models\vit_b\decoder\sam_vit_b_01ec64_decoder.onnx"  --image_path "C:\Users\Administrator\Downloads\16026.jpg"   --use_demo true --use_boxinfo true

2、一定要用onnxruntime-win-x64-1.14.1， 使用 onnxruntime-win-x64-1.18.0 会编译不通过，编译时得到类似于这样的错误：
	D:\lib\onnxruntime-win-x64-1.18.0\include\onnxruntime_cxx_api.h(315): error C3615: constexpr 函数 "Ort::BFloat16_t::BFloa
		t16_t" 不会生成常数表达式 [C:\Users\Administrator\Desktop\onnx\SegmentAnything-OnnxRunner\build\main.vcxproj]

3、onnxrunime 还无法像 opencv那样添加环境变量，即 即便运行前记得添加动态库的路径，like this  set PATH=D:\lib\onnxruntime-win-x64-1.14.1\lib;%PATH% 
	这是没用的，一定要把 onnxruntime.dll  （onnxruntime_providers_shared.dll 这个不一定要）这动态库放到编译好的可执行文件的路径中

4、这个项目的名字是：SegmentAnything-OnnxRunner（也用cmake编译过了）
	- 一些UI交互去看它的readme吧，

5、这个项目对使用opencv的UI交互也有着积极的意义(interactive.h)，也要懂得去用结构体来装数据


*/


#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <fstream>
#include <filesystem>

#include "SAMConfig.h"
#include "SAMOnnxRunner.h"
#include "interactive.h"

static bool file_exists(const std::string &filename) {
	std::ifstream file(filename);
	return file.good();   // 表示流是否是有效状态，用 file.is_open()是一个效果
}


void process_arguments(int argc, char* argv[], Configuration &cfg) {
	std::cout << "[INFO] Argc num :" << argc << std::endl;

	std::map<std::string, std::string> arguments;
	for (int i = 0; i < argc; i++) {
		std::string arg = argv[i];
		if (arg.substr(0, 2) == "--") {
			std::string param_name = arg.substr(2);
			std::string param_value = (i + 1 < argc) ? argv[i + 1] : "";
			//arguments[param_name] = param_value;
			arguments.insert({ param_name, param_value });
			i++;
		}
	}
	for (const auto& arg : arguments) {
		std::cout << "[INFO] Parameter Name : " << arg.first << " , Parameter Value : " << arg.second << std::endl;
	}

	std::string encoder_model_path, decoder_model_path;
	std::string image_path, save_dir;
	bool USE_BOXINFO = false, USE_DEMO = true, USE_SINGLEMASK = false;
	bool KEEP_BOXINFO = true;
	double threshold = 0.9;

	for (const auto &arg : arguments) {
		if (arg.first == "encoder_model_path") { encoder_model_path = arg.second; }
		else if (arg.first == "decoder_model_path") { decoder_model_path = arg.second; }
		else if (arg.first == "image_path") { image_path = arg.second; }
		else if (arg.first == "save_dir") { save_dir = arg.second; }
		else if (arg.first == "use_demo") {
			std::istringstream(arg.second) >> std::boolalpha >> USE_DEMO;
			if (USE_DEMO) { std::cout << "[INFO] Prepare to run SAM with graphical interface demo" << std::endl; }
		}
		else if (arg.first == "use_boxinfo") { std::istringstream(arg.second) >> std::boolalpha >> USE_BOXINFO; }
		else if (arg.first == "use_singlemask") { std::istringstream(arg.second) >> std::boolalpha >> USE_SINGLEMASK; }
		else if (arg.first == "threshold") { threshold = std::stod(arg.second); }
	}

	if (encoder_model_path.empty() || decoder_model_path.empty() || image_path.empty()) {
		std::cout << "[ERROR] Model path (--encoder_model_path/--decoder_model_path) or Image path (--image_path) not provided." << std::endl;
		throw std::runtime_error("[ERROR] Model path (--encoder_model_path/--decoder_model_path) or Image path (--image_path) not provided.");
	}
	// 再判断下给定的文件路是否有东西
	if (!file_exists(encoder_model_path) || !file_exists(decoder_model_path) || !file_exists(image_path)) {
		throw std::runtime_error("Maybe one of the path is wrong, please check!");
	}

	if (save_dir.empty()) {
		save_dir = "./output";
		try {
			// 下面这要c++17的支持
			if (!std::filesystem::exists(save_dir)) {
				std::filesystem::create_directory(save_dir);
				std::cout << "[INFO] No save folder provided, create default folder at " << save_dir << std::endl;
			}
		}
		catch (const std::filesystem::filesystem_error &e) {
			std::cout << "[ERROR] Error creating or checking folder: " << e.what() << std::endl;
		}
	}

	cfg.EncoderModelPath = encoder_model_path;
	cfg.DecoderModelPath = decoder_model_path;
	cfg.SaveDir = save_dir;
	cfg.ImagePath = image_path;
	cfg.SegThreshold = threshold;
	cfg.UseSingleMask = USE_SINGLEMASK;
	cfg.UseBoxInfo = USE_BOXINFO;
	cfg.KeepBoxInfo = KEEP_BOXINFO;
	cfg.USE_DEMO = USE_DEMO;
}



int main(int argc, char* argv[]) {
	
	Configuration cfg;
	process_arguments(argc, argv, cfg);

	// Init Onnxruntime Env
	SAMOnnxRunner Segmentator(std::thread::hardware_concurrency());
	Segmentator.InitOrtEnv(cfg);

	cv::Mat srcImage = cv::imread(cfg.ImagePath, -1);  // -1 代表会加载alpha通道
	cv::Mat visualImage = srcImage.clone();   // 结果图先没有，用clone得到返回值，图在前面先定义了，用copyTo
	cv::Mat maskImage;

	if (cfg.USE_DEMO) {
		std::cout << "[WELCOME] Segment Anything Onnx Runner Demo" << std::endl;
		const char* windowName = "Segment Anything Onnx Runner Demo";
		cv::namedWindow(windowName, cv::WINDOW_NORMAL);
		cv::resizeWindow(windowName, srcImage.cols / 2, srcImage.rows / 2);

		MouseParams mouseparams;
		mouseparams.image = visualImage;

		cv::setMouseCallback(windowName, GetClick_handler, reinterpret_cast<void*>(&mouseparams));
		
		bool RunnerWork = true;
		while (RunnerWork) {
			std::vector<MatInfo> maskinfo;
			// 当产生有效点击时才进行推理
			if ((mouseparams.clickinfo.pt.x > 0) && (mouseparams.clickinfo.pt.y > 0)) {
				maskinfo = Segmentator.InferenceSingleImage(cfg, srcImage, mouseparams.clickinfo, mouseparams.boxinfo);
				unsigned int index = 0;
				// Apply mask to image
				visualImage = cv::Mat::zeros(srcImage.size(), CV_8UC3);
				for (int i = 0; i < srcImage.rows; i++) {
					for (int j = 0; j < srcImage.cols; j++) {
						double factor = maskinfo[index].mask.at<uchar>(i, j) > 0 ? 1.0 : 0.4;
						visualImage.at<cv::Vec3b>(i, j) = srcImage.at<cv::Vec3b>(i, j) * factor;
					}
				}
				maskImage = maskinfo[0].mask;
			}
			mouseparams.clickinfo.pt.x = 0;
			mouseparams.clickinfo.pt.y = 0;
			if (!cfg.KeepBoxInfo) {
				mouseparams.boxinfo.left_top = cv::Point(0, 0);
				mouseparams.boxinfo.right_bot = cv::Point(srcImage.cols, srcImage.rows);
			}

			cv::imshow(windowName, visualImage);

			int key = cv::waitKeyEx(100);
			switch (key) {
			case 27:
			case 'q': { // Quit Segment Anything Onnx Runner Demo
				RunnerWork = false;
			} break;
			case 'c': {
				// Continue , Use the mask output from the previous run. 
				if (!(maskImage.empty())) {
					std::cout << "[INFO] The maskImage is not empty, and the mask with the highest confidence is used as the mask_input of the decoder." << std::endl;
					cfg.HasMaskInput = true;
				}
				else {
					std::cout << "[WARNINGS] The maskImage is empty, and there is no available mask as the mask_input of the decoder." << std::endl;
				}
			} break;
			}
		}
		
		cv::destroyWindow(windowName);
	}
	else {
		bool RunnerWork = true;
		ClickInfo clickinfo;
		BoxInfo boxinfo;

		while (RunnerWork) {
			std::cout << "[PROMPT] Please enter the prompt of the click point (0/1 , x , y) : ";
			std::cin >> clickinfo.positive >> clickinfo.pt.x >> clickinfo.pt.y;

			if (cfg.UseBoxInfo) {
				std::cout << "[PROMPT] Please enter the prompt of the box info (x1 , y1 , x2 , y2) : ";
				std::cin >> boxinfo.left_top.x >> boxinfo.left_top.y >> boxinfo.right_bot.x >> boxinfo.right_bot.y;
			}

			auto time_start = std::chrono::high_resolution_clock::now();
			Segmentator.InferenceSingleImage(cfg, srcImage, clickinfo, boxinfo);
			auto time_end = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double> diff = time_end - time_start;
			std::cout << "Segmentator InferenceSingleImage Cost time : " << diff.count() << "s" << std::endl;

			std::cout << "[INFO] Whether to proceed to the next round of segmentation (0/1) : ";
			std::cin >> std::boolalpha >> RunnerWork;
		}

		Segmentator.ResetInitEncoder();
	}

	return EXIT_SUCCESS;
}