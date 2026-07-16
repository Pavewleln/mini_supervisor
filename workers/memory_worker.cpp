#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

int main(int argc, char* argv[]) {
	int targetMb = 32;

	if (argc >= 2) {
		targetMb = std::atoi(argv[1]);
	}

	if (targetMb < 1) {
		targetMb = 1;
	}

	std::vector<std::vector<char>> blocks;
	blocks.reserve(static_cast<size_t>(targetMb));

	for (int i = 1; i <= targetMb; ++i) {
		blocks.emplace_back(1024 * 1024, static_cast<char>(i));
		std::cout << "memory_worker allocated_mb=" << i << "\n";
		std::cout.flush();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	while (true) {
		std::cout << "memory_worker holding_mb=" << targetMb << "\n";
		std::cout.flush();
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}
