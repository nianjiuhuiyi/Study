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
	std::cout << "status:" << res->status << std::endl;
	std::cout << "body:" << res->body << std::endl;



	//httplib::Client cli("192.168.108.52", 7714);
	//auto res = cli.Get("/iot/http/push");
	//std::cout << "status:" << res->status << std::endl;
	//std::cout << "body:" << res->body << std::endl;

	system("pause");
	return 0;
}
