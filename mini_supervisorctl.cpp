#include <nlohmann/json.hpp>

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace {
	using json = nlohmann::json;

	struct HttpResponse {
		int statusCode = 0;
		std::string body;
	};

	void printUsage(const char* program) {
		std::cerr << "usage: " << program << " [--port <port>] <command> [service]\n";
		std::cerr << "commands:\n";
		std::cerr << "  status\n";
		std::cerr << "  logs <service>\n";
		std::cerr << "  start <service>\n";
		std::cerr << "  stop <service>\n";
		std::cerr << "  restart <service>\n";
	}

	bool sendAll(int fd, const std::string& data) {
		size_t sent = 0;

		while (sent < data.size()) {
			ssize_t bytes = send(fd, data.data() + sent, data.size() - sent, 0);
			if (bytes == -1) {
				return false;
			}

			sent += static_cast<size_t>(bytes);
		}

		return true;
	}

	HttpResponse request(const std::string& method, const std::string& path, int port) {
		int fd = socket(AF_INET, SOCK_STREAM, 0);
		if (fd == -1) {
			throw std::runtime_error(std::string("socket error: ") + std::strerror(errno));
		}

		sockaddr_in address{};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		address.sin_port = htons(static_cast<uint16_t>(port));

		if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == -1) {
			std::string message = std::string("connect error: ") + std::strerror(errno);
			close(fd);
			throw std::runtime_error(message);
		}

		std::ostringstream requestStream;
		requestStream << method << " " << path << " HTTP/1.1\r\n";
		requestStream << "Host: 127.0.0.1:" << port << "\r\n";
		requestStream << "Content-Length: 0\r\n";
		requestStream << "Connection: close\r\n";
		requestStream << "\r\n";

		std::string requestText = requestStream.str();
		if (!sendAll(fd, requestText)) {
			std::string message = std::string("send error: ") + std::strerror(errno);
			close(fd);
			throw std::runtime_error(message);
		}

		std::string responseText;
		char buffer[4096];

		while (true) {
			ssize_t bytes = recv(fd, buffer, sizeof(buffer), 0);
			if (bytes == -1) {
				std::string message = std::string("recv error: ") + std::strerror(errno);
				close(fd);
				throw std::runtime_error(message);
			}

			if (bytes == 0) {
				break;
			}

			responseText.append(buffer, static_cast<size_t>(bytes));
		}

		close(fd);

		std::istringstream responseStream(responseText);
		std::string httpVersion;
		HttpResponse response;
		responseStream >> httpVersion >> response.statusCode;

		size_t bodyPos = responseText.find("\r\n\r\n");
		if (bodyPos != std::string::npos) {
			response.body = responseText.substr(bodyPos + 4);
		}

		return response;
	}

	bool printErrorResponse(const HttpResponse& response) {
		try {
			json body = json::parse(response.body);
			if (body.contains("error")) {
				std::cerr << "error: " << body["error"].get<std::string>() << "\n";
				return true;
			}
		} catch (const json::exception&) {
		}

		std::cerr << "http error: " << response.statusCode << "\n";
		return true;
	}

	void printStatus(const HttpResponse& response) {
		json body = json::parse(response.body);

		for (const auto& service : body.at("services")) {
			std::cout << service.at("name").get<std::string>() << "\t";
			std::cout << service.at("state").get<std::string>() << "\t";
			std::cout << "restarts=" << service.at("restarts").get<int>() << "\n";
		}
	}

	void printLogs(const HttpResponse& response) {
		json body = json::parse(response.body);
		std::cout << "stdout:\n" << body.value("stdout", "");
		std::cout << "stderr:\n" << body.value("stderr", "");
	}

	void printActionResult(const HttpResponse& response) {
		json body = json::parse(response.body);
		std::cout << body.at("name").get<std::string>() << "\t";
		std::cout << body.at("state").get<std::string>() << "\t";
		std::cout << "restarts=" << body.at("restarts").get<int>() << "\n";
	}
}

int main(int argc, char* argv[]) {
	int port = 8080;
	int argIndex = 1;

	if (argc >= 3 && std::string(argv[1]) == "--port") {
		port = std::stoi(argv[2]);
		argIndex = 3;
	}

	if (argIndex >= argc) {
		printUsage(argv[0]);
		return 1;
	}

	std::string command = argv[argIndex++];
	std::string method;
	std::string path;

	if (command == "status") {
		if (argIndex != argc) {
			printUsage(argv[0]);
			return 1;
		}

		method = "GET";
		path = "/services";
	} else if (command == "logs") {
		if (argIndex + 1 != argc) {
			printUsage(argv[0]);
			return 1;
		}

		method = "GET";
		path = "/services/" + std::string(argv[argIndex]) + "/logs";
	} else if (command == "start" || command == "stop" || command == "restart") {
		if (argIndex + 1 != argc) {
			printUsage(argv[0]);
			return 1;
		}

		method = "POST";
		path = "/services/" + std::string(argv[argIndex]) + "/" + command;
	} else {
		printUsage(argv[0]);
		return 1;
	}

	try {
		HttpResponse response = request(method, path, port);

		if (response.statusCode < 200 || response.statusCode >= 300) {
			printErrorResponse(response);
			return 1;
		}

		if (command == "status") {
			printStatus(response);
		} else if (command == "logs") {
			printLogs(response);
		} else {
			printActionResult(response);
		}
	} catch (const std::exception& error) {
		std::cerr << "error: " << error.what() << "\n";
		return 1;
	}

	return 0;
}
