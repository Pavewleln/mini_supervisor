#include "service.hpp"

Service::Service(std::string name, std::string path, std::vector<std::string> args, bool restartEnabled, int maxRestarts)
	: name_(name), process_(path, args), restartEnabled_(restartEnabled), maxRestarts_(maxRestarts) {}

bool Service::start() {
	return process_.start();
}

bool Service::stop() {
	return process_.stop();
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

const std::string& Service::name() const {
	return name_;
}
ProcessState Service::state() const {
	return process_.state();
}
int Service::restartCount() const {
	return restartCount_;
}