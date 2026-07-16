#pragma once

#include "process.hpp"
#include <vector>
#include <string>
#include <fstream>
#include <deque>

enum class ServiceState {
	Starting,
	Running,
	Stopping,
	Exited,
	Crashed,
	Restarting,
	Failed
};

const char* toString(ServiceState state);

class Service {
public:
	explicit Service(std::string name, std::string path, std::vector<std::string> args, bool restartEnabled, int maxRestarts);

	bool start();
	bool stop();
	bool stopGracefully(int timeoutMs);
	bool wait();
	bool poll();

	const std::string& name() const;
	ServiceState state() const;
	int restartCount() const;
	long memoryRssKb() const;

	bool readLogs();

	const std::string stdoutLog() const;
	const std::string stderrLog() const;
private:
	std::string name_;
	Process process_;
	bool restartEnabled_;
	ServiceState state_ = ServiceState::Exited;
	int restartCount_ = 0;
	int maxRestarts_;

	std::deque<std::string> stdoutLines_;
	std::deque<std::string> stderrLines_;
	std::string stdoutPendingLine_;
	std::string stderrPendingLine_;
	std::ofstream stdoutFile_;
	std::ofstream stderrFile_;

	bool ensureLogFiles();
	void updateStateFromProcess();
	void appendLogChunk(std::deque<std::string>& lines, std::string& pendingLine, const char* buffer, size_t size);
	std::string buildLogString(const std::deque<std::string>& lines, const std::string& pendingLine) const;
};
