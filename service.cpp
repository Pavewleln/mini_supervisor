#include "service.hpp"
#include <cerrno>
#include <unistd.h>

Service::Service(std::string name, std::string path, std::vector<std::string> args, bool restartEnabled, int maxRestarts)
	: name_(name), process_(path, args), restartEnabled_(restartEnabled), maxRestarts_(maxRestarts) {}

bool Service::start() {
	return process_.start();
}

bool Service::stop() {
	return process_.stop();
}

bool Service::stopGracefully(int timeoutMs) {
	return process_.gracefulStop(timeoutMs);
}

bool Service::wait() {
	if (!process_.wait()) {
		return false;
	}

	bool crashed =
		process_.lastExitCode() != 0 ||
		process_.lastSignal() != -1;

	if (restartEnabled_ && crashed && restartCount_ < maxRestarts_) {
		++restartCount_;
		return process_.start();
	}

	return true;
}

bool Service::poll() {
	if (!process_.poll()) {
		return false;
	}

	bool crashed =
		process_.lastExitCode() != 0 ||
		process_.lastSignal() != -1;

	if (restartEnabled_ && crashed && state() != ProcessState::Running && restartCount_ < maxRestarts_) {
		++restartCount_;
		return process_.start();
	}

	return true;
}

bool Service::readLogs() {
	char buffer[4096];

	while(true) {
		ssize_t bytes = process_.readStdout(buffer, sizeof(buffer));
		if (bytes > 0) {
			stdoutLog_.append(buffer, bytes);
			continue;
		}

		if (bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			break;
		}

		if (bytes == 0) {
			break;
		}

		return false;
	}

	while(true) {
		ssize_t bytes = process_.readStderr(buffer, sizeof(buffer));
		if (bytes > 0) {
			stderrLog_.append(buffer, bytes);
			continue;
		}

		if (bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			break;
		}

		if (bytes == 0) {
			break;
		}

		return false;
	}

	return true;
}

const std::string Service::stdoutLog() const {
	return stdoutLog_;
}

const std::string Service::stderrLog() const {
	return stderrLog_;
}

const std::string& Service::name() const {
	return name_;
}

ProcessState Service::state() const {
	return process_.state();
}

int Service::restartCount() const {
	return restartCount_;
}
