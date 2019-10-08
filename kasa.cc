#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>

using Stop = std::pair<std::string, uint>;
using Route = std::vector<Stop>;
using Timetable = std::unordered_map<uint, Route>;

void parseInputLine(const std::string& line) {
    std::vector<std::string> tokens;

    std::istringstream iss(line);
    for (std::string s; iss >> s;) {
        tokens.push_back(s);
    }
}

void handleInput() {
    std::string buffer;

    while(std::getline(std::cin, buffer)) {
        parseInputLine(buffer);
    }
}

int main() {
    handleInput();
    return 0;
}