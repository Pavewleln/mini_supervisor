#include "http_server.hpp"

#include <nlohmann/json.hpp>

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace {
	using json = nlohmann::json;

	std::string reasonPhrase(int statusCode) {
		switch (statusCode) {
		case 200:
			return "OK";
		case 400:
			return "Bad Request";
		case 404:
			return "Not Found";
		case 405:
			return "Method Not Allowed";
		case 409:
			return "Conflict";
		case 500:
			return "Internal Server Error";
		default:
			return "Error";
		}
	}

	std::string makeResponse(int statusCode, const std::string& contentType, const std::string& body) {
		std::ostringstream response;
		response << "HTTP/1.1 " << statusCode << " " << reasonPhrase(statusCode) << "\r\n";
		response << "Content-Type: " << contentType << "\r\n";
		response << "Content-Length: " << body.size() << "\r\n";
		response << "Connection: close\r\n";
		response << "\r\n";
		response << body;
		return response.str();
	}

	std::string jsonError(const std::string& message) {
		return json{{"error", message}}.dump();
	}

	std::vector<std::string> splitPath(const std::string& path) {
		std::vector<std::string> parts;
		std::stringstream stream(path);
		std::string part;

		while (std::getline(stream, part, '/')) {
			if (!part.empty()) {
				parts.push_back(part);
			}
		}

		return parts;
	}
}

HttpServer::HttpServer(Supervisor& supervisor, std::mutex& supervisorMutex, int port)
	: supervisor_(supervisor), supervisorMutex_(supervisorMutex), port_(port) {}

bool HttpServer::start() {
	serverFd_ = socket(AF_INET, SOCK_STREAM, 0);
	if (serverFd_ == -1) {
		std::cerr << "http socket error: " << std::strerror(errno) << "\n";
		return false;
	}

	int reuse = 1;
	if (setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
		std::cerr << "http setsockopt error: " << std::strerror(errno) << "\n";
		close(serverFd_);
		serverFd_ = -1;
		return false;
	}

	sockaddr_in address{};
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	address.sin_port = htons(static_cast<uint16_t>(port_));

	if (bind(serverFd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == -1) {
		std::cerr << "http bind error: " << std::strerror(errno) << "\n";
		close(serverFd_);
		serverFd_ = -1;
		return false;
	}

	if (listen(serverFd_, 16) == -1) {
		std::cerr << "http listen error: " << std::strerror(errno) << "\n";
		close(serverFd_);
		serverFd_ = -1;
		return false;
	}

	running_ = true;
	return true;
}

void HttpServer::stop() {
	running_ = false;
}

void HttpServer::run() {
	while (running_) {
		fd_set readSet;
		FD_ZERO(&readSet);
		FD_SET(serverFd_, &readSet);

		timeval timeout{};
		timeout.tv_sec = 0;
		timeout.tv_usec = 200 * 1000;

		int ready = select(serverFd_ + 1, &readSet, nullptr, nullptr, &timeout);
		if (ready == -1) {
			if (errno == EINTR) {
				continue;
			}

			std::cerr << "http select error: " << std::strerror(errno) << "\n";
			break;
		}

		if (ready == 0) {
			continue;
		}

		int clientFd = accept(serverFd_, nullptr, nullptr);
		if (clientFd == -1) {
			if (errno == EINTR) {
				continue;
			}

			std::cerr << "http accept error: " << std::strerror(errno) << "\n";
			continue;
		}

		handleClient(clientFd);
		close(clientFd);
	}

	close(serverFd_);
	serverFd_ = -1;
}

void HttpServer::handleClient(int clientFd) {
	char buffer[8192];
	ssize_t bytes = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
	if (bytes <= 0) {
		return;
	}

	buffer[bytes] = '\0';
	std::istringstream request(buffer);
	std::string method;
	std::string path;
	std::string version;
	request >> method >> path >> version;

	int statusCode = 200;
	std::string contentType = "application/json";
	std::string body = handleRequest(method, path, statusCode, contentType);
	std::string response = makeResponse(statusCode, contentType, body);
	send(clientFd, response.data(), response.size(), 0);
}

std::string HttpServer::handleRequest(const std::string& method, const std::string& path, int& statusCode, std::string& contentType) {
	(void)contentType;

	if (method == "GET" && path == "/services") {
		statusCode = 200;
		return servicesJson();
	}

	std::vector<std::string> parts = splitPath(path);
	if (parts.size() == 3 && parts[0] == "services" && method == "GET" && parts[2] == "logs") {
		return logsJson(parts[1], statusCode);
	}

	if (parts.size() == 3 && parts[0] == "services" && method == "POST") {
		if (parts[2] == "start" || parts[2] == "stop" || parts[2] == "restart") {
			return serviceAction(parts[1], parts[2], statusCode);
		}
	}

	if (method != "GET" && method != "POST") {
		statusCode = 405;
		return jsonError("method not allowed");
	}

	statusCode = 404;
	return jsonError("not found");
}

std::string HttpServer::servicesJson() {
	std::lock_guard<std::mutex> lock(supervisorMutex_);
	json services = json::array();

	for (const auto& service : supervisor_.services()) {
		services.push_back({
			{"name", service.name()},
			{"state", toString(service.state())},
			{"restarts", service.restartCount()},
			{"memory_rss_kb", service.memoryRssKb()}
		});
	}

	return json{{"services", services}}.dump();
}

std::string HttpServer::logsJson(const std::string& serviceName, int& statusCode) {
	std::lock_guard<std::mutex> lock(supervisorMutex_);
	Service* service = supervisor_.findService(serviceName);

	if (service == nullptr) {
		statusCode = 404;
		return jsonError("service not found");
	}

	statusCode = 200;
	return json{
		{"name", service->name()},
		{"stdout", service->stdoutLog()},
		{"stderr", service->stderrLog()}
	}.dump();
}

std::string HttpServer::serviceAction(const std::string& serviceName, const std::string& action, int& statusCode) {
	std::lock_guard<std::mutex> lock(supervisorMutex_);
	Service* service = supervisor_.findService(serviceName);

	if (service == nullptr) {
		statusCode = 404;
		return jsonError("service not found");
	}

	bool ok = false;

	if (action == "start") {
		ok = service->start();
	} else if (action == "stop") {
		ok = service->stopGracefully(3000);
	} else if (action == "restart") {
		if (service->state() == ServiceState::Running) {
			ok = service->stopGracefully(3000);
			if (!ok) {
				statusCode = 500;
				return jsonError("failed to stop service");
			}
		}

		ok = service->start();
	}

	if (!ok) {
		statusCode = 409;
		return jsonError("service action failed");
	}

	statusCode = 200;
	return json{
		{"name", service->name()},
		{"state", toString(service->state())},
		{"restarts", service->restartCount()},
		{"memory_rss_kb", service->memoryRssKb()}
	}.dump();
}
