#include "config.hpp"
#include <iostream>
#include <sstream>
#include <fstream>

std::vector<ServiceConfig> loadConfig(const std::string& path) {
	std::ifstream file(path);
	std::vector<ServiceConfig> configs;

	if (!file.is_open()) {
		std::cerr << "Error opening file!" << "\n";
		return configs;
	}

	std::string line;

	while (std::getline(file, line)) {
		if (line.empty()) continue;

		ServiceConfig config;
		std::stringstream ss(line);
		std::string token;

		/* name */
		if (std::getline(ss, token, '|')) {
			config.name = token;
		}

		/* path */
		if (std::getline(ss, token, '|')) {
			config.path = token;
		}

		/* args */
		if (std::getline(ss, token, '|')) {
			std::stringstream token_stream(token);
			std::string arg;

			while (std::getline(token_stream, arg, ',')) {
				config.args.push_back(arg);
			}
		}

		/* restartEnabled */
		if (std::getline(ss, token, '|')) {
			config.restartEnabled = (bool)(token == "true");
		}

		/* maxRestarts */
		if (std::getline(ss, token, '|')) {
			config.maxRestarts = std::stoi(token);
		}

		configs.push_back(config);
	}

	return configs;
}