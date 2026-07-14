#include "supervisor.hpp"
#include "service.hpp"
#include <iostream>
#include <unistd.h>

int main() {
	Supervisor supervisor;

	supervisor.addService(Service("worker-1", "/bin/sleep", {"10"}, false, 0));
	supervisor.addService(Service("bad-worker", "/bin/sh", {"-c", "exit 42"}, true, 3));

	if (!supervisor.startAll()) return 1;

	sleep(1);

	if (!supervisor.stopAll()) return 1;
	if (!supervisor.waitAll()) return 1;

	auto* service = supervisor.findService("bad-worker");
	if (service) {
		std::cout << service->name() << " restarts: " << service->restartCount() << "\n";
	}

	return 0;
}