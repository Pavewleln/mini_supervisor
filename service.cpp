#include "service.hpp"
#include <cerrno>
#include <filesystem>
#include <iostream>
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
	if (!ensureLogFiles()) {
		return false;
	}

	char buffer[4096];

	while(true) {
		ssize_t bytes = process_.readStdout(buffer, sizeof(buffer));
		if (bytes > 0) {
			stdoutLog_.append(buffer, bytes);
			stdoutFile_.write(buffer, bytes);
			stdoutFile_.flush();

			if (!stdoutFile_) {
				std::cerr << "stdout log write error for service " << name_ << "\n";
				return false;
			}

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
			stderrFile_.write(buffer, bytes);
			stderrFile_.flush();

			if (!stderrFile_) {
				std::cerr << "stderr log write error for service " << name_ << "\n";
				return false;
			}

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

bool Service::ensureLogFiles() {
	if (stdoutFile_.is_open() && stderrFile_.is_open()) {
		return true;
	}

	std::error_code error;
	std::filesystem::create_directories("logs", error);

	if (error) {
		std::cerr << "create logs directory error: " << error.message() << "\n";
		return false;
	}

	if (!stdoutFile_.is_open()) {
		stdoutFile_.open("logs/" + name_ + ".out.log", std::ios::app | std::ios::binary);
	}

	if (!stderrFile_.is_open()) {
		stderrFile_.open("logs/" + name_ + ".err.log", std::ios::app | std::ios::binary);
	}

	if (!stdoutFile_.is_open() || !stderrFile_.is_open()) {
		std::cerr << "open log files error for service " << name_ << "\n";
		return false;
	}

	return true;
}
