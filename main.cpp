#include "config.hpp"
#include "supervisor.hpp"

#include <iostream>
#include <string>
#include <unistd.h>
#include <csignal>

volatile std::sig_atomic_t g_shouldStop = 0;

void handleSignal(int) {
	g_shouldStop = 1;
}

int main(int argc, char* argv[]) {
	std::signal(SIGINT, handleSignal);
	std::signal(SIGTERM, handleSignal);

	if (argc != 3) {
		std::cerr << "usage: " << argv[0] << " <config> <duration_seconds>\n";
		return 1;
	}

	std::string configPath = argv[1];
	int durationSeconds = std::stoi(argv[2]);

	auto configs = loadConfig(configPath);

	if (configs.empty()) {
		std::cerr << "config is empty or failed to load\n";
		return 1;
	}

	Supervisor supervisor;

	for (const auto& config : configs) {
		supervisor.addService(Service(
			config.name,
			config.path,
			config.args,
			config.restartEnabled,
			config.maxRestarts
		));
	}

	if (!supervisor.startAll()) {
		return 1;
	}

	const int ticksPerSecond = 5;
	const int sleepUs = 200 * 1000;
	const int iterations = durationSeconds * ticksPerSecond;

	for (int i = 0; i < iterations && !g_shouldStop; ++i) {
		if (!supervisor.runOnce()) {
			return 1;
		}

		usleep(sleepUs);
	}

	supervisor.stopAll();

	for (int i = 0; i < ticksPerSecond; ++i) {
		supervisor.runOnce();
		usleep(sleepUs);
	}

	for (const auto& service : supervisor.services()) {
		std::cout << "service: " << service.name() << "\n";
		std::cout << "restarts: " << service.restartCount() << "\n";
		std::cout << "stdout:\n" << service.stdoutLog();
		std::cout << "stderr:\n" << service.stderrLog();
	}

	return 0;
}