#pragma once

#include "supervisor.hpp"

#include <atomic>
#include <mutex>
#include <string>

class HttpServer {
public:
	HttpServer(Supervisor& supervisor, std::mutex& supervisorMutex, int port);

	bool start();
	void run();
	void stop();

private:
	Supervisor& supervisor_;
	std::mutex& supervisorMutex_;
	int port_;
	int serverFd_ = -1;
	std::atomic<bool> running_{false};

	void handleClient(int clientFd);
	std::string handleRequest(const std::string& method, const std::string& path, int& statusCode, std::string& contentType);
	std::string servicesJson();
	std::string logsJson(const std::string& serviceName, int& statusCode);
	std::string serviceAction(const std::string& serviceName, const std::string& action, int& statusCode);
};
