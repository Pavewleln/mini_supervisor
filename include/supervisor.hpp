#pragma once

#include "service.hpp"

#include <string>
#include <vector>

class Supervisor {
public:
	void addService(Service service);

	bool startAll();
	bool stopAll();
	bool waitAll();

	Service* findService(const std::string& name);
	const std::vector<Service>& services() const;

	bool runOnce();

private:
	std::vector<Service> services_;
};