#include "config.hpp"
#include "http_server.hpp"
#include "supervisor.hpp"

#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>
#include <csignal>

volatile std::sig_atomic_t g_shouldStop = 0;

void handleSignal(int) {
	g_shouldStop = 1;
}

int main(int argc, char* argv[]) {
	std::signal(SIGINT, handleSignal);
	std::signal(SIGTERM, handleSignal);

	if (argc < 3 || argc > 4) {
		std::cerr << "usage: " << argv[0] << " <config> <duration_seconds> [http_port]\n";
		return 1;
	}

	std::string configPath = argv[1];
	int durationSeconds = std::stoi(argv[2]);
	int httpPort = argc == 4 ? std::stoi(argv[3]) : 8080;

	auto configs = loadConfig(configPath);

	if (configs.empty()) {
		std::cerr << "config is empty or failed to load\n";
		return 1;
	}

	Supervisor supervisor;
	std::mutex supervisorMutex;

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

	HttpServer httpServer(supervisor, supervisorMutex, httpPort);
	if (!httpServer.start()) {
		std::lock_guard<std::mutex> lock(supervisorMutex);
		supervisor.stopAll();
		return 1;
	}

	std::cout << "HTTP API listening on http://127.0.0.1:" << httpPort << "\n";

	std::thread httpThread([&httpServer]() {
		httpServer.run();
	});

	const int ticksPerSecond = 5;
	const int sleepUs = 200 * 1000;
	const int iterations = durationSeconds * ticksPerSecond;

	for (int i = 0; i < iterations && !g_shouldStop; ++i) {
		bool ok = false;
		{
			std::lock_guard<std::mutex> lock(supervisorMutex);
			ok = supervisor.runOnce();
		}

		if (!ok) {
			httpServer.stop();
			httpThread.join();
			return 1;
		}

		usleep(sleepUs);
	}

	{
		std::lock_guard<std::mutex> lock(supervisorMutex);
		supervisor.stopAll();
	}

	for (int i = 0; i < ticksPerSecond; ++i) {
		std::lock_guard<std::mutex> lock(supervisorMutex);
		supervisor.runOnce();
		usleep(sleepUs);
	}

	httpServer.stop();
	httpThread.join();

	std::lock_guard<std::mutex> lock(supervisorMutex);
	for (const auto& service : supervisor.services()) {
		std::cout << "service: " << service.name() << "\n";
		std::cout << "state: " << toString(service.state()) << "\n";
		std::cout << "restarts: " << service.restartCount() << "\n";
		std::cout << "stdout:\n" << service.stdoutLog();
		std::cout << "stderr:\n" << service.stderrLog();
	}

	return 0;
}
