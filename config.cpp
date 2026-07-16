#include "config.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <cctype>
#include <utility>

namespace {
	using json = nlohmann::json;

	bool looksLikeJson(const std::string& text) {
		for (char ch : text) {
			if (!std::isspace(static_cast<unsigned char>(ch))) {
				return ch == '{';
			}
		}

		return false;
	}

	std::vector<ServiceConfig> loadJsonConfig(const std::string& text) {
		std::vector<ServiceConfig> configs;

		try {
			json root = json::parse(text);

			if (!root.contains("services") || !root["services"].is_array()) {
				std::cerr << "config error: services must be an array\n";
				return configs;
			}

			for (const auto& item : root["services"]) {
				ServiceConfig config;
				config.name = item.at("name").get<std::string>();
				config.path = item.at("command").get<std::string>();
				config.args = item.value("args", std::vector<std::string>{});
				config.restartEnabled = item.value("restart", false);
				config.maxRestarts = item.value("max_restarts", 0);

				if (config.name.empty() || config.path.empty()) {
					std::cerr << "config error: service name and command must not be empty\n";
					return {};
				}

				configs.push_back(std::move(config));
			}
		} catch (const json::exception& error) {
			std::cerr << "json config error: " << error.what() << "\n";
			return {};
		}

		return configs;
	}

	std::vector<ServiceConfig> loadLegacyConfig(const std::string& text) {
		std::vector<ServiceConfig> configs;
		std::stringstream lines(text);
		std::string line;

		while (std::getline(lines, line)) {
			if (line.empty()) {
				continue;
			}

			ServiceConfig config;
			std::stringstream ss(line);
			std::string token;

			if (std::getline(ss, token, '|')) {
				config.name = token;
			}

			if (std::getline(ss, token, '|')) {
				config.path = token;
			}

			if (std::getline(ss, token, '|')) {
				std::stringstream tokenStream(token);
				std::string arg;

				while (std::getline(tokenStream, arg, ',')) {
					config.args.push_back(arg);
				}
			}

			if (std::getline(ss, token, '|')) {
				config.restartEnabled = token == "true";
			}

			if (std::getline(ss, token, '|')) {
				config.maxRestarts = std::stoi(token);
			}

			configs.push_back(config);
		}

		return configs;
	}
}

std::vector<ServiceConfig> loadConfig(const std::string& path) {
	std::ifstream file(path);

	if (!file.is_open()) {
		std::cerr << "config open error: " << path << "\n";
		return {};
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string text = buffer.str();

	if (looksLikeJson(text)) {
		return loadJsonConfig(text);
	}

	return loadLegacyConfig(text);
}
