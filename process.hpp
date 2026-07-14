#pragma once

#include <string>
#include <vector>
#include <sys/types.h>

enum class ProcessState {
	NotStarted,
	Running,
	Stopped,
	Exited,
	Failed
};

class Process {
public:
	explicit Process(std::string path, std::vector<std::string> args);

	bool start();
	bool stop();
	bool wait();

	pid_t pid() const;

	bool isRunning() const;
	ProcessState state() const;

	int lastSignal() const;
	int lastExitCode() const;

private:
	std::string path_;
	std::vector<std::string> args_;
	ProcessState state_ = ProcessState::NotStarted;
	
	int lastStatus_ = 0;
	int lastExitCode_ = -1;
	int lastSignal_ = -1;

	pid_t pid_ = -1;
};