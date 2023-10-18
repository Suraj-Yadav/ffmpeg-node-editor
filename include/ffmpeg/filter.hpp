#include <string>
#include <vector>

enum SocketType { Video, Audio };

struct Socket {
	std::string name;
	SocketType type;
};

struct AllowedValues {
	std::string desc;
	std::string value;
};

struct Option {
	std::string name;
	std::string desc;
	std::string type;
	std::string defaultValue;
	std::string min;
	std::string max;
	std::vector<AllowedValues> allowed;
};

struct Filter {
	std::string name;
	std::string desc;
	std::vector<Socket> input;
	std::vector<Socket> output;
	std::vector<Option> options;
};
