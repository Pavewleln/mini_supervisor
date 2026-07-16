#pragma once

#include "process.hpp"
#include <vector>
#include <string>
#include <fstream>

class Service {
public:
	explicit Service(std::string name, std::string path, std::vector<std::string> args, bool restartEnabled, int maxRestarts);

	bool start();
	bool stop();
	bool stopGracefully(int timeoutMs);
	bool wait();
	bool poll();

	const std::string& name() const;
	ProcessState state() const;
	int restartCount() const;

	bool readLogs();

	const std::string stdoutLog() const;
	const std::string stderrLog() const;
private:
	std::string name_;
	Process process_;
	bool restartEnabled_;
	int restartCount_ = 0;
	int maxRestarts_;

	std::string stdoutLog_;
	std::string stderrLog_;
	std::ofstream stdoutFile_;
	std::ofstream stderrFile_;

	bool ensureLogFiles();
};
