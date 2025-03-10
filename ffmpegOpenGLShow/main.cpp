#include <iostream>
#include <glad/glad.h>  // 特别注意：在包含GLFW的头文件之前包含了GLAD的头文件。GLAD的头文件包含了正确的OpenGL头文件（例如GL/gl.h），所以需要在其它依赖于OpenGL的头文件之前包含GLAD。
#include <GLFW/glfw3.h>
#include <opencv2/opencv.hpp>

#include "shader_s.h"

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

/*  下面是ffmpeg的读取  */
/*
	yuvj×××这个格式被丢弃了，然后转化为yuv格式，
	不然有一个警告 deprecated pixel format used, make sure you did set range correctly，
	这个问题在前面和win32写api时可用，但是不知道其它地方会不会报错，就改过了
*/
AVPixelFormat ConvertDeprecatedFormat(enum AVPixelFormat format) {
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
			break;
		}
	}
	param.fmtCtx = fmtCtx;
	param.vcodecCtx = vcodecCtx;
	param.width = vcodecCtx->width;
	param.height = vcodecCtx->height;
}
int RequestFrame(DecoderParam &param, AVFrame *frame, AVPacket *packet) {
	auto &fmtCtx = param.fmtCtx;
	auto &vcodecCtx = param.vcodecCtx;
	auto &VideoStreamIndex = param.VideoStreamIndex;

	while (1) {
		// AVPacket *packet = av_packet_alloc();
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


void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

/* 

	ffmpeg或opencv 读取流，然后用 opengl做贴图，glfw做展示 

*/

// settings
#define ffmpeg   // 这行注释掉就用opencv读取视频流
unsigned int SCR_WIDTH = 800;
unsigned int SCR_HEIGHT = 600;
const char* rtsp_path = "rtsp://192.168.108.146:554/user=admin&password=&channel=1&stream=0.sdp?";

int main() {

#ifdef ffmpeg
	DecoderParam decoderParam;
	InitDecoder(rtsp_path, decoderParam);  // 如果file_path.c_str()是，std::string，就写成file_path.c_str()
	SCR_WIDTH = decoderParam.width;
	SCR_HEIGHT = decoderParam.height;
	auto &fmtCtx = decoderParam.fmtCtx;   // 不知道它这都习惯定义变量时用 & 引用
	auto &vcodecCtx = decoderParam.vcodecCtx;

	cv::Mat img(cv::Size(SCR_WIDTH, SCR_HEIGHT), CV_8UC3);
	std::vector<Color_RGB> buffer(SCR_WIDTH * SCR_HEIGHT);
	AVFrame *frame = av_frame_alloc();  // 提前分配
	AVPacket *packet = av_packet_alloc();

#else
	cv::VideoCapture cap;
	cap.open(rtsp_path);
	cv::Mat img;
	if (cap.read(img)) {
		SCR_WIDTH = img.cols;
		SCR_HEIGHT = img.rows;
	}

#endif // ffmpeg

	// 1、实例化GLFW窗口
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);  // 说明opengl版本，方便glfw做调整
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  //告诉GLFW我们使用的是核心模式(Core-profile)；明确告诉GLFW我们需要使用核心模式意味着我们只能使用OpenGL功能的一个子集（没有我们已不再需要的向后兼容特性）
#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);  // 针对苹果，上一行core-profile才生效
#endif

	// 2、创建一个窗口对象
	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "OpenGLWindow", NULL, NULL);
	if (window == NULL) {
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	// 通知GLFW将我们窗口的上下文设置为当前线程的主上下文了
	glfwMakeContextCurrent(window);

	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

	// 3、初始化glad，GLAD是用来管理OpenGL的函数指针的，所以在调用任何OpenGL的函数之前我们需要初始化GLAD
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}  // 给GLAD传入了用来加载系统相关的OpenGL函数指针地址的函数

	// 创建编译shader pragram
	Shader ourShader("E:\\VS_project\\Study\\OpenGL_demo\\shader.vs", "E:\\VS_project\\Study\\OpenGL_demo\\shader.fs");

	//float vertices[] = {   // 3个顶点属性  // 带一些颜色
	//	// ---- 位置 ----       ---- 颜色 ----     - 纹理坐标 -
	//	0.5f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 1.0f,   // 右上
	//	0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f,   // 右下
	//	-0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f,   // 左下
	//	-0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 0.0f,   0.0f, 1.0f    // 左上
	//};

	//float vertices[] = {   // 3个顶点属性    //纯白，就是原图案
	//	// ---- 位置 ----       ---- 颜色 ----     - 纹理坐标 -
	//	0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 1.0f,   1.0f, 1.0f,   // 右上
	//	0.5f, -0.5f, 0.0f,   1.0f, 1.0f, 1.0f,   1.0f, 0.0f,   // 右下
	//	-0.5f, -0.5f, 0.0f,   1.0f, 1.0f, 1.0f,   0.0f, 0.0f,   // 左下
	//	-0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 1.0f,   0.0f, 1.0f    // 左上
	//  0.5相当于就占了窗口的一半
	//};

	float vertices[] = {   // 3个顶点属性   // 改了位置坐标，让它把图形填满
		// ---- 位置 ----       ---- 颜色(纯白)--  -- 纹理坐标 -
		1.0f,  1.0f, 0.0f,   1.0f, 1.0f, 1.0f,   1.0f, 1.0f,   // 右上
		1.0f, -1.0f, 0.0f,   1.0f, 1.0f, 1.0f,   1.0f, 0.0f,   // 右下
		-1.0f, -1.0f, 0.0f,   1.0f, 1.0f, 1.0f,   0.0f, 0.0f,   // 左下
		-1.0f,  1.0f, 0.0f,   1.0f, 1.0f, 1.0f,   0.0f, 1.0f    // 左上
	};


	unsigned int indices[] = {
		0, 1, 3,   // first triangle
		1, 2, 3    // second triangle
	};


	unsigned int VBO, VAO, EBO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);

	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	// position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	// texture coord attribute
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	//// texture coord attribute
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);


	// 1、load and create a texture
	unsigned int texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	// 2、为当前绑定的纹理对象设置环绕、过滤方式
	// set the texture wrapping parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	// set texture filtering parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	//// 3、加载图片并生成纹理
	//int width, height, nrChannels;
	//cv::Mat img = cv::imread(R"(C:\Users\Administrator\Downloads\container.jpg)");
	//cv::cvtColor(img, img, cv::COLOR_BGR2RGB);
	//unsigned char* data = img.data;
	//if (data) {
	//	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img.cols, img.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
	//	glGenerateMipmap(GL_TEXTURE_2D);
	//}
	//else {
	//	std::cout << "Failed to load texture!" << std::endl;
	//}

	// 哪怕就一张纹理，也记得设置（下一小节，纹理单位有详说）
	ourShader.use();  // 在设置uniform变量之一定激活着色器程序
	glUniform1i(glGetUniformLocation(ourShader.ID, "texture1"), 0);

	// while 中绘制就是 glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	// 但是在这之前要记得激活绑定(下一小节有讲)
	glActiveTexture(GL_TEXTURE0); // 在绑定纹理之前先激活纹理单元
	glBindTexture(GL_TEXTURE_2D, texture);


	// render loop
	while (!glfwWindowShouldClose(window)) {
		// input
		processInput(window);

		// render
		glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

#ifdef ffmpeg
		RequestFrame(decoderParam, frame, packet);
		// 原来的格式是AV_PIX_FMT_YUVJ420P，被丢弃，会有一个警告：deprecated pixel format used, make sure you did set range correctly  (主要是针对rtsp，本地视频好像不用)
		frame->format = ConvertDeprecatedFormat(static_cast<AVPixelFormat>(frame->format));
		GetRGBPixels(frame, buffer);  // 解码调用
		//uint8_t* data[] = { reinterpret_cast<uint8_t*>(&pixels[0]) };
		// uint8_t *data = reinterpret_cast<uint8_t*>(&buffer[0]);  // 上面那行也是一个意思，一般写作unsigned char* data，OpenGL都是这样的
		img.data = reinterpret_cast<uint8_t*>(&buffer[0]);
#else
		cap >> img;
		
#endif // ffmpeg
		cv::cvtColor(img, img, cv::COLOR_BGR2RGB);
		cv::flip(img, img, 0);  // opengl学习时也说了，它默认是上下颠倒的，所以这里要翻转一下
		// 必须要转成RGB才行，前面给GL_BGR,虽然有，但是显示是有问题的
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img.cols, img.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, img.data);
		glGenerateMipmap(GL_TEXTURE_2D);

		// render the triangle
		ourShader.use();
		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	// optional: de-allocate all resources once they've outlived their purpose:
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);

#ifdef ffmpeg
	// av_frame_unref(frame);  // 这似乎不是释放
	av_frame_free(&frame);  // 来释放内存
	av_packet_free(&packet);
	avcodec_free_context(&decoderParam.vcodecCtx);
#else
	cap.release();
#endif // !ffmpeg

	// glfw: terminate, clearing all previously allocated GLFW resources.
	glfwTerminate();
	return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
	// 4、glViewport函数前两个参数控制窗口左下角的位置。第三个和第四个参数控制渲染窗口的宽度和高度（像素）（有更深含义的技术，看这些代码的链接解释）
	glViewport(0, 0, width, height);
}
void processInput(GLFWwindow* window) {
	// 按esc关闭窗口
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
}

