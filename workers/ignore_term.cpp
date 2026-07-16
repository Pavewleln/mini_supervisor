#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

namespace {
	std::atomic<int> termSignals{0};

	void handleTerm(int) {
		++termSignals;
	}
}

int main() {
	std::signal(SIGTERM, handleTerm);

	while (true) {
		std::cout << "ignore_term alive, term_signals=" << termSignals.load() << "\n";
		std::cout.flush();
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}
