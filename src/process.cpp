#include <iostream>
#include <process.hpp>

int main() {
	std::cout << "Example 1 - the mandatory Hello World" << std::endl;
	TinyProcessLib::Process process2(
		std::vector<std::string>{"mpvnet", "-"}, "", nullptr, nullptr, true);
	TinyProcessLib::Process process1(
		{"ffmpeg", "-i", "C:\\Users\\suraj\\DOWNLO~1\\test.mp4", "-c", "copy",
		 "-map", "0", "-f", "matroska", "-"},
		"",
		[&process2](const char* bytes, size_t n) { process2.write(bytes, n); },
		nullptr, true);
	auto exit_status = process2.get_exit_status();
	std::cout << "Example 2 process returned: " << exit_status << " ("
			  << (exit_status == 0 ? "success" : "failure") << ")" << std::endl;
	process1.write("q");
	std::cout << "Example 1 process returned: " << process1.get_exit_status()
			  << " (" << (exit_status == 0 ? "success" : "failure") << ")"
			  << std::endl;
	return 0;
}
