#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>

using Ticket = std::tuple<std::string, unsigned long long, unsigned long long>;
using TicketSet = std::set<Ticket>;
using StopNameSet = std::unordered_set<std::string>;
using Stop = std::pair<const std::string&, uint>;
using Route = std::vector<Stop>;
using Timetable = std::unordered_map<uint, Route>;

void parseInputLine(const std::string& line) {
    std::vector<std::string> tokens;

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