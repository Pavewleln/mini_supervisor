#include "supervisor.hpp"
#include <utility>

void Supervisor::addService(Service service) {
	services_.push_back(std::move(service));
}

bool Supervisor::startAll() {
	bool ok = true;
	for (auto& service : services_) {
		if (!service.start()) {
			ok = false;
		}
	}

	return ok;
}

bool Supervisor::stopAll() {
	bool ok = true;
	for (auto& service : services_) {
		if (service.state() == ProcessState::Running && !service.stop()) {
			ok = false;
		}
	}

	return ok;
}

bool Supervisor::waitAll() {
	bool ok = true;
	for (auto& service : services_) {
		if (!service.wait()) {
			ok = false;
		}
	}

	return ok;
}

Service* Supervisor::findService(const std::string& name) {
	for (auto& service : services_) {
		if (service.name() == name) {
			return &service;
		}
	}

	return nullptr;
}

const std::vector<Service>& Supervisor::services() const {
	return services_;
}

bool Supervisor::runOnce() {
	bool ok = true;

	for (auto& service : services_) {
		if (!service.readLogs()) {
			ok = false;
		}

		if (service.state() == ProcessState::Running) {
			if (!service.poll()) {
				ok = false;
			}
		}

		if (!service.readLogs()) {
			ok = false;
		}
	}

	return ok;
}