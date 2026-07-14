#pragma once

#include "process.hpp"
#include <vector>
#include <string>

class Service {
public:
	explicit Service(std::string name, std::string path, std::vector<std::string> args, bool restartEnabled, int maxRestarts);

	bool start();
	bool stop();
	bool wait();

	const std::string& name() const;
	ProcessState state() const;
	int restartCount() const;
private:
	std::string name_;
	Process process_;
	bool restartEnabled_;
	int restartCount_ = 0;
	int maxRestarts_;
};