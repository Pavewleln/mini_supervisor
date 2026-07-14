#include "service.hpp"
#include <iostream>
#include <unistd.h>

int main() {
	Service service("bad-worker", "/bin/sh", {"-c", "exit 42"}, true, 3);

	if (!service.start()) return 1;

	for (int i = 0; i < 4; ++i) {
		if (!service.wait()) return 1;

		std::cout << "restart count: "
				<< service.restartCount()
				<< "\n";
	}

	return 0;
}