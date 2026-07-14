#include "process.hpp"

#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <cerrno>

Process::Process(std::string path, std::vector<std::string> args)
	: path_(path), args_(args) {}

bool Process::start() {
	if (isRunning()) {
		std::cerr << "process already running\n";
		return false;
	}

	pid_ = fork();

	if (pid_ == -1) {
		std::cerr << "fork error: " << std::strerror(errno) << "\n";
		state_ = ProcessState::Failed;
		return false;
	}

	if (pid_ == 0) {
		std::vector<char*> argv;

		argv.push_back(const_cast<char*>(path_.c_str()));

		for(auto& arg : args_)
			argv.push_back(const_cast<char*>(arg.c_str()));

		argv.push_back(nullptr);

		execvp(argv[0], argv.data());
		std::cerr << "execvp error: " << std::strerror(errno) << "\n";
		_exit(1);
	}

	state_ = ProcessState::Running;

	return true;
}

bool Process::stop() {
	if (!isRunning()) {
		std::cerr << "process is not running\n";
		return false;
	}

	if (kill(pid_, SIGTERM) == -1) {
		std::cerr << "kill error: " << std::strerror(errno) << "\n";
		return false;
	}

	return true;
}

bool Process::wait() {
	int status = 0;
	if (waitpid(pid_, &status, 0) == -1) {
		std::cerr << "waitpid error: " << std::strerror(errno) << "\n";
		state_ = ProcessState::Failed;
		return false;
	}

	lastStatus_ = status;

	if (WIFEXITED(status)) {
		lastExitCode_ = WEXITSTATUS(status);
		lastSignal_ = -1;
		state_ = ProcessState::Exited;
	}

	if (WIFSIGNALED(status)) {
		lastSignal_ = WTERMSIG(status);
		lastExitCode_ = -1;
		state_ = ProcessState::Stopped;
	}

	return true;
}

pid_t Process::pid() const {
	return pid_;
}

bool Process::isRunning() const {
	return state_ == ProcessState::Running;
}

ProcessState Process::state() const {
	return state_;
}

int Process::lastSignal() const {
	return lastSignal_;
}

int Process::lastExitCode() const {
	return lastExitCode_;
}