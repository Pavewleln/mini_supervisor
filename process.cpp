#include "process.hpp"

#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <algorithm>
#include <csignal>

namespace {
	bool setNonBlocking(int fd) {
		int flags = fcntl(fd, F_GETFL, 0);
		if (flags == -1) {
			return false;
		}

		return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
	}
}

Process::Process(std::string path, std::vector<std::string> args)
	: path_(path), args_(args) {}

bool Process::start() {
	if (isRunning()) {
		std::cerr << "process already running\n";
		return false;
	}

	int stdoutPipe[2];
	if (pipe(stdoutPipe) == -1) {
		std::cerr << "stdout pipe error: " << std::strerror(errno) << "\n";
		state_ = ProcessState::Failed;
		return false;
	}

	stdoutRead_.reset(stdoutPipe[0]);
	stdoutWrite_.reset(stdoutPipe[1]);

	int stderrPipe[2];
	if (pipe(stderrPipe) == -1) {
		std::cerr << "stderr pipe error: " << std::strerror(errno) << "\n";
		stdoutRead_.reset();
		stdoutWrite_.reset();
		state_ = ProcessState::Failed;
		return false;
	}

	stderrRead_.reset(stderrPipe[0]);
	stderrWrite_.reset(stderrPipe[1]);

	pid_ = fork();

	/* error */
	if (pid_ == -1) {
		std::cerr << "fork error: " << std::strerror(errno) << "\n";
		stdoutRead_.reset();
		stdoutWrite_.reset();
		stderrRead_.reset();
		stderrWrite_.reset();
		state_ = ProcessState::Failed;
		return false;
	}

	/* child */
	if (pid_ == 0) {

		if (setpgid(0, 0) == -1) {
			std::cerr << "setpgid error: " << std::strerror(errno) << "\n";
			_exit(1);
		}

		/* stdout */
		stdoutRead_.reset();
		if (dup2(stdoutWrite_.get(), STDOUT_FILENO) == -1) {
			std::cerr << "dup2 error: " << std::strerror(errno) << "\n";
			_exit(1);
		}

		stdoutWrite_.reset();

		/* stderr */
		stderrRead_.reset();
		if (dup2(stderrWrite_.get(), STDERR_FILENO) == -1) {
			std::cerr << "dup2 error: " << std::strerror(errno) << "\n";
			_exit(1);
		}

		stderrWrite_.reset();

		/* argv */
		std::vector<char*> argv;
		argv.push_back(const_cast<char*>(path_.c_str()));

		for(auto& arg : args_)
			argv.push_back(const_cast<char*>(arg.c_str()));

		argv.push_back(nullptr);

		execvp(argv[0], argv.data());
		std::cerr << "execvp error: " << std::strerror(errno) << "\n";
		_exit(1);
	}

	/* parent */

	/* stdout */
	stdoutWrite_.reset();

	if (!setNonBlocking(stdoutRead_.get())) {
		std::cerr << "fcntl stdout error: " << std::strerror(errno) << "\n";
		state_ = ProcessState::Failed;
		return false;
	}

	/* stderr */
	stderrWrite_.reset();

	if (!setNonBlocking(stderrRead_.get())) {
		std::cerr << "fcntl stderr error: " << std::strerror(errno) << "\n";
		state_ = ProcessState::Failed;
		return false;
	}

	state_ = ProcessState::Running;

	return true;
}

bool Process::stop() {
	if (!isRunning()) {
		std::cerr << "process is not running\n";
		return false;
	}

	if (kill(-pid_, SIGTERM) == -1) {
		std::cerr << "kill error: " << std::strerror(errno) << "\n";
		return false;
	}

	return true;
}

bool Process::forceKill() {
	if (!isAlive()) {
		std::cerr << "process is not alive\n";
		return false;
	}

	if (kill(-pid_, SIGKILL) == -1) {
		if (errno == ESRCH) {
			return poll() && !isAlive();
		}

		std::cerr << "kill error: " << std::strerror(errno) << "\n";
		return false;
	}

	return true;
}

bool Process::gracefulStop(int timeoutMs) {
	if (!isRunning()) {
		std::cerr << "process is not running\n";
		return false;
	}

	if (!stop()) {
		return false;
	}

	const int stepUs = 100 * 1000;
	int elapsedUs = 0;
	const int timeoutUs = timeoutMs * 1000;

	while (elapsedUs < timeoutUs) {
		if (!poll()) {
			return false;
		}

		if (!isAlive()) {
			return true;
		}

		int sleepUs = std::min(stepUs, timeoutUs - elapsedUs);
		usleep(sleepUs);
		elapsedUs += sleepUs;
	}

	if (!poll()) {
		return false;
	}

	if (!isAlive()) {
		return true;
	}

	if (!forceKill()) {
		return false;
	}

	if (isRunning()) {
		return wait();
	}

	const int killWaitUs = 1000 * 1000;
	int killElapsedUs = 0;

	while (isAlive() && killElapsedUs < killWaitUs) {
		usleep(stepUs);
		killElapsedUs += stepUs;
	}

	return !isAlive();
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

bool Process::poll() {
	if (!isRunning()) {
		return true;
	}

	int status = 0;

	pid_t pid = waitpid(pid_, &status, WNOHANG);

	/* error */
	if (pid == -1) {
		std::cerr << "waitpid error: " << std::strerror(errno) << "\n";
		state_ = ProcessState::Failed;
		return false;
	}

	/* child */
	if (pid == 0) {
		return true;
	}

	/* parent */

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

int Process::stdoutFd() const {
	return stdoutRead_.get();
}

int Process::stderrFd() const {
	return stderrRead_.get();
}

pid_t Process::pid() const {
	return pid_;
}

bool Process::isAlive() const {
	if (pid_ <= 0) {
		return false;
	}

	if (kill(-pid_, 0) == 0) {
		return true;
	}

	return errno == EPERM;
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

ssize_t Process::readStdout(char* buffer, size_t size) {
	return read(stdoutRead_.get(), buffer, size);
}
ssize_t Process::readStderr(char* buffer, size_t size) {
	return read(stderrRead_.get(), buffer, size);
}
