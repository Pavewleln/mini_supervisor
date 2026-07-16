#include <chrono>
#include <iostream>
#include <thread>

int main() {
	for (int i = 1; i <= 5; ++i) {
		std::cout << "logger stdout line " << i << "\n";
		std::cerr << "logger stderr line " << i << "\n";
		std::cout.flush();
		std::cerr.flush();
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}

	return 0;
}
