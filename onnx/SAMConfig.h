#pragma once
#include <string>

struct Configuration {
	double SegThreshold = 0.9;
	bool UseSingleMask;
	bool UseBoxInfo;
	bool USE_DEMO;
	bool KeepBoxInfo = true;  // Reuse the same box information
	bool HasMaskInput = false;  // Enter the existing mask into the decoder

	std::string EncoderModelPath;
	std::string DecoderModelPath;
	std::string Device;

	std::string SaveDir;
	std::string ImagePath;
};
