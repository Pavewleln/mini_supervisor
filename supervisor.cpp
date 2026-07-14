#include "supervisor.hpp"

void Supervisor::addService(Service service) {
	services_.push_back(std::move(service));
}

bool Supervisor::startAll() {
	for (auto& service : services_) {
		if (!service.start()) {
			return false;
		}
	}

	return true;
}

bool Supervisor::stopAll() {
	for (auto& service : services_) {
		if (service.state() == ProcessState::Running && !service.stop()) {
			return false;
		}
	}

	return true;
}

bool Supervisor::waitAll() {
	for (auto& service : services_) {
		if (!service.wait()) {
			return false;
		}
	}

	return true;
}

Service* Supervisor::findService(const std::string& name) {
	for (auto& service : services_) {
		if (service.name() == name) {
			return &service;
		}
	}

	return nullptr;
}