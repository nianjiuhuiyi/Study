#include <iostream>
#include "httplib.h"
#include "json.hpp"


int main(int argc, char** argv) {
	//std::cout << "hello world!" << std::endl;

	//httplib::Server svr;
	//svr.Get("/hi", [](const httplib::Request &, httplib::Response &res) {
	//	res.set_content("new hello world!", "text/plain");
	//});
	//svr.listen("0.0.0.0", 8080);

	
	using json = nlohmann::json;

	json content = {
		{"deviceID", 0},
		{"timeStamp", 20230818162957384},
		{"cameraCode", 5},
		{"monitorPointCode", 10},
		{"toolsCode", "1_2_5_13"},
		{"signCode", "5_6_7"}
	};

	json req_json = {
		{"accessPointCode", "aiserver"},
		{"eventType", 32000},
		{"token", ""},
		{"content", content.dump()}
	};

	std::cout << req_json.dump() << std::endl;
	

	httplib::Headers headers = {
		{"content-type", "application/json"}
	};

	httplib::Client cli("192.168.108.52", 7714);
	auto res = cli.Post("/iot/http/push", headers, req_json.dump(), "application/json");
	// 返回的结果是指针的，一定要判断是不是空指针,不然网络不可达，直接使用res会崩溃,使用try包裹都不行
	if (res) {
		std::cout << "status:" << res->status << std::endl;
		std::cout << "body:" << res->body << std::endl;
	}
	else {
		std::cerr << "消息发送失败，可能是目标网络不不可达，10秒后再次尝试..." << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(10));
	}
	



	//httplib::Client cli("192.168.108.52", 7714);
	//auto res = cli.Get("/iot/http/push");
	//std::cout << "status:" << res->status << std::endl;
	//std::cout << "body:" << res->body << std::endl;

	system("pause");
	return 0;
}
