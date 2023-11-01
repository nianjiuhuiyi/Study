#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <iostream>
#include <vector>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include <opencv2/opencv.hpp>

#include "my_ffmpeg.hpp"
#include "yolov5.hpp"



// ffmpeg还不会读取win上的本地摄像头，暂时都试过了，没能成功
const char* videoStreamAddress = "rtsp://192.168.108.11:554/user=admin&password=&channel=1&stream=1.sdp?";
//const char* videoStreamAddress = "C:\\Users\\Administrator\\Videos\\keypoint_result.mp4";



#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif



static void glfw_error_callback(int error, const char* description) {
	fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}


/*
	用ffmpeg读数视频流，然后将其处理一下，转成opencv的mat，然后在使用opengl渲染，在imgui里展示出来
*/
void ffmpeg_opengl(AVFrame *frame, std::vector<Color_RGB> &buffer, cv::Mat &img) {
	static SwsContext *swsctx = nullptr;
	swsctx = sws_getCachedContext(swsctx,
		frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
		frame->width, frame->height, AVPixelFormat::AV_PIX_FMT_BGR24, NULL, NULL, NULL, NULL
	); /*AVPixelFormat::AV_PIX_FMT_RGB24*/
	uint8_t* data[] = { reinterpret_cast<uint8_t*>(&buffer[0]) };
	int linesize[] = { frame->width * 3 };
	sws_scale(swsctx, frame->data, frame->linesize, 0, frame->height, data, linesize);

	int time = 0;
	for (int i = 0; i < img.rows; ++i) {
		for (int j = 0; j < img.cols; ++j) {
			img.at<cv::Vec3b>(i, j) = { buffer[time].b, buffer[time].g, buffer[time].r };
			time++;
		}
	}
}


int main(int argc, char** argv) {
	// Setup window
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit())
		return 1;

	// Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
	// GL ES 2.0 + GLSL 100
	const char* glsl_version = "#version 100";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
	// GL 3.2 + GLSL 150
	const char* glsl_version = "#version 150";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
	// GL 3.0 + GLSL 130
	const char* glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
	//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

	// Create window with graphics context
	GLFWwindow* window = glfwCreateWindow(1280, 720, "Dear ImGui GLFW+OpenGL3 example", NULL, NULL);
	if (window == NULL)
		return 1;
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // Enable vsync

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	//ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();
	ImGui::StyleColorsClassic();

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	// Load Fonts  // 加载其它字体，其demo它文件里也有
	// 注意执行路径时，和这相对路径对不对应，可以添加几种字体，进到style中可以修改
	io.Fonts->AddFontFromFileTTF("JetBrainsMono-Bold.ttf", 16.0f);  
	// 字体要支持汉字才行
	io.Fonts->AddFontFromFileTTF("c:/windows/fonts/simhei.ttf", 13.0f, NULL, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());  

	// Our state
	bool show_demo_window = false;
	bool show_videoCapture_window = true;  // my
	
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);


	// ffmpeg视频读取的初始化
	DecoderParam decoderParam;
	InitDecoder(videoStreamAddress, decoderParam);
	int width = decoderParam.width;
	int height = decoderParam.height;
	auto &fmtCtx = decoderParam.fmtCtx;   // 不知道它这都习惯定义变量时用 & 引用
	auto &vcodecCtx = decoderParam.vcodecCtx;

	// 存放数据，循环外初始化（static也是一样效果）
	static std::vector<Color_RGB> buffer(width * height);  
	static cv::Mat img(cv::Size(width, height), CV_8UC3);   // uint8_t *data
	static std::vector<std::string> class_list;  
	
	static cv::dnn::Net net;
	loadModle("yolov5s.onnx", "coco.names", &net, &class_list);

	// Main loop  // 
	while (!glfwWindowShouldClose(window)) {
		// 我把它的一些注释删了，，去它的demo里看就好
		glfwPollEvents();

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
		if (show_demo_window)
			ImGui::ShowDemoWindow(&show_demo_window);

		// 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
		{
			static float f = 0.0f;
			static int counter = 0;

			ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

			ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
			ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
			
			// my function of videoCapture
			ImGui::Checkbox("VideoCapture Window", &show_videoCapture_window);

			ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
			ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

			if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
				counter++;
			ImGui::SameLine();
			ImGui::Text("counter = %d", counter);

			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			ImGui::End();
		}

		// my video and detect
		{
			if (show_videoCapture_window) {
				static bool if_detect = false;
				static bool show_style_editor = false;
				static GLuint video_texture = 0;

				if (show_style_editor) {
					ImGui::Begin("Dear ImGui Style Editor", &show_style_editor);
					ImGui::ShowStyleEditor();
					ImGui::End();
				}
				ImGui::Begin(u8"OpenGL Texture video，汉字可以嘛");  // 主要前面要有u8才能支持汉字
				ImGui::Text("size = %d x %d", width, height);

				AVFrame *frame = RequestFrame(decoderParam);
				// 原来的格式是AV_PIX_FMT_YUVJ420P，被丢弃，会有一个警告：deprecated pixel format used, make sure you did set range correctly
				frame->format = ConvertDeprecatedFormat(static_cast<AVPixelFormat>(frame->format));

				// 转换的核心
				ffmpeg_opengl(frame, buffer,img);
				
				if (if_detect) { detect(net, class_list, img); }
				

				////generate texture using GL commands
				glBindTexture(GL_TEXTURE_2D, video_texture);
				// Setup filtering parameters for display
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frame->width, frame->height, 0, GL_RGB, GL_UNSIGNED_BYTE, img.data);

		
				ImGui::Image((void*)(intptr_t)video_texture, ImVec2(frame->width, frame->height));

				ImGui::Checkbox("Detect", &if_detect);
				ImGui::SameLine();  // 这个可以把两个button放在一行里
				ImGui::Checkbox("Style Editor", &show_style_editor);
				ImGui::End();

				av_frame_free(&frame);
			}
		}

		// Rendering
		ImGui::Render();
		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
	}

	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}

/*
	这个主要是用ffmpeg读取流来展示，已经归档在笔记中了

*/