#include "service.hpp"
#include <cerrno>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <unistd.h>

namespace {
	constexpr size_t kMaxLogLines = 500;
}

const char* toString(ServiceState state) {
	switch (state) {
	case ServiceState::Starting:
		return "Starting";
	case ServiceState::Running:
		return "Running";
	case ServiceState::Stopping:
		return "Stopping";
	case ServiceState::Exited:
		return "Exited";
	case ServiceState::Crashed:
		return "Crashed";
	case ServiceState::Restarting:
		return "Restarting";
	case ServiceState::Failed:
		return "Failed";
	}

	return "Unknown";
}

Service::Service(std::string name, std::string path, std::vector<std::string> args, bool restartEnabled, int maxRestarts)
	: name_(name), process_(path, args), restartEnabled_(restartEnabled), maxRestarts_(maxRestarts) {}

bool Service::start() {
	state_ = ServiceState::Starting;

	if (!process_.start()) {
		state_ = ServiceState::Failed;
		return false;
	}

	state_ = ServiceState::Running;
	return true;
}

bool Service::stop() {
	state_ = ServiceState::Stopping;

	if (!process_.stop()) {
		updateStateFromProcess();
		return false;
	}

	return true;
}

bool Service::stopGracefully(int timeoutMs) {
	state_ = ServiceState::Stopping;

	if (!process_.gracefulStop(timeoutMs)) {
		updateStateFromProcess();
		return false;
	}

	updateStateFromProcess();
	return true;
}

bool Service::wait() {
	if (!process_.wait()) {
		state_ = ServiceState::Failed;
		return false;
	}

	updateStateFromProcess();

	bool crashed =
		process_.lastExitCode() != 0 ||
		process_.lastSignal() != -1;

	if (restartEnabled_ && crashed && restartCount_ < maxRestarts_) {
		++restartCount_;
		state_ = ServiceState::Restarting;
		return start();
	}

	return true;
}

bool Service::poll() {
	if (!process_.poll()) {
		state_ = ServiceState::Failed;
		return false;
	}

	updateStateFromProcess();

	bool crashed =
		process_.lastExitCode() != 0 ||
		process_.lastSignal() != -1;

	if (restartEnabled_ && crashed && state_ != ServiceState::Running && restartCount_ < maxRestarts_) {
		++restartCount_;
		state_ = ServiceState::Restarting;
		return start();
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
			appendLogChunk(stdoutLines_, stdoutPendingLine_, buffer, bytes);
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
			appendLogChunk(stderrLines_, stderrPendingLine_, buffer, bytes);
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
	return buildLogString(stdoutLines_, stdoutPendingLine_);
}

const std::string Service::stderrLog() const {
	return buildLogString(stderrLines_, stderrPendingLine_);
}

const std::string& Service::name() const {
	return name_;
}

ServiceState Service::state() const {
	return state_;
}

int Service::restartCount() const {
	return restartCount_;
}

long Service::memoryRssKb() const {
	return process_.memoryRssKb();
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

void Service::updateStateFromProcess() {
	switch (process_.state()) {
	case ProcessState::NotStarted:
		state_ = ServiceState::Exited;
		break;
	case ProcessState::Running:
		state_ = ServiceState::Running;
		break;
	case ProcessState::Exited:
		state_ = process_.lastExitCode() == 0 ? ServiceState::Exited : ServiceState::Crashed;
		break;
	case ProcessState::Stopped:
		state_ = ServiceState::Crashed;
		break;
	case ProcessState::Failed:
		state_ = ServiceState::Failed;
		break;
	}
}

void Service::appendLogChunk(std::deque<std::string>& lines, std::string& pendingLine, const char* buffer, size_t size) {
	for (size_t i = 0; i < size; ++i) {
		pendingLine.push_back(buffer[i]);

		if (buffer[i] == '\n') {
			lines.push_back(std::move(pendingLine));
			pendingLine.clear();

			while (lines.size() > kMaxLogLines) {
				lines.pop_front();
			}
		}
	}
}

std::string Service::buildLogString(const std::deque<std::string>& lines, const std::string& pendingLine) const {
	std::ostringstream stream;

	for (const auto& line : lines) {
		stream << line;
	}

	stream << pendingLine;

	return stream.str();
}
