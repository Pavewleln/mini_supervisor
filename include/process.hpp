#pragma once

#include "file_descriptor.hpp"

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
	bool forceKill();
	bool gracefulStop(int timeoutMs);
	bool wait();
	bool poll();

	pid_t pid() const;

	bool isAlive() const;
	bool isRunning() const;
	ProcessState state() const;

	int lastSignal() const;
	int lastExitCode() const;
	long memoryRssKb() const;

	int stdoutFd() const;
	int stderrFd() const;

	ssize_t readStdout(char* buffer, size_t size);
	ssize_t readStderr(char* buffer, size_t size);
private:
	std::string path_;
	std::vector<std::string> args_;
	ProcessState state_ = ProcessState::NotStarted;
	
	int lastStatus_ = 0;
	int lastExitCode_ = -1;
	int lastSignal_ = -1;

	FileDescriptor stdoutRead_;
	FileDescriptor stdoutWrite_;
	FileDescriptor stderrRead_;
	FileDescriptor stderrWrite_;

	pid_t pid_ = -1;
};
