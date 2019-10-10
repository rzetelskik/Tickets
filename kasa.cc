#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <variant>
#include <regex>

using Price = unsigned long;
using ValidTime = unsigned long long;
using Ticket = std::tuple<std::string, Price, ValidTime>;
using TicketSet = std::set<Ticket>;
using StopNameSet = std::unordered_set<std::string>;
using StopTime = unsigned long;
using Stop = std::pair<const std::string&, StopTime>;
using Route = std::vector<Stop>;
using LineNum = unsigned long long;
using Timetable = std::unordered_map<LineNum, Route>;
using RouteStop = std::pair<std::string, StopTime>;
using AddRoute = std::pair<LineNum, std::vector<RouteStop>>;
using QueryStop = std::pair<std::string, StopTime>;
using Query = std::vector<QueryStop>;
using Other = nullptr_t;
using Request = std::variant<AddRoute, Ticket, Query, Other>;

enum RequestType {
    ADD_ROUTE,
    ADD_TICKET,
    QUERY,
    IGNORE,
    ERROR
};

using ParseResult = std::pair<RequestType, Request>;


RequestType getRequestType(const std::string& line) {
    if (line.empty()) {
        return IGNORE;
    }
    char c = line.at(0);

    if (isdigit(c)) {
        return ADD_ROUTE;
    } else if (isalpha(c) || isspace(c)) {
        return ADD_TICKET;
    } else if (c == '?') {
        return QUERY;
    } else {
        return ERROR;
    }
}

ParseResult parseIgnore() {
    return ParseResult(IGNORE, Request(nullptr));
}

ParseResult parseError() {
    return ParseResult(ERROR, Request(nullptr));
}

bool isStopTimeCorrect(StopTime stopTime, StopTime prevStopTime) {
    return (stopTime > prevStopTime && stopTime >= 355 && stopTime <= 1281);
}

ParseResult parseAddRoute(const std::string& line) {
    std::regex rgx(R"(^(\d+)((?: (?:[5-9]|1\d|2[0-1]):[0-5]\d [a-zA-Z^_]+)+)$)");
    std::smatch match;

    //TODO exceptions
    //TODO check for repeating stops
    if (!std::regex_match(line, match, rgx)) {
        return parseError();
    }
    LineNum lineNum = stoull(match.str(1));
    std::vector<RouteStop> routeStops;
    StopTime prevStopTime = 0;
    std::string iterStr = match.str(2);
    std::regex iterRgx(R"( ([5-9]|1\d|2[0-1])\:([0-5]\d) ([a-zA-Z^_]+))");

    for (auto it = std::sregex_iterator(iterStr.begin(), iterStr.end(), iterRgx); it != std::sregex_iterator(); ++it) {
        std::smatch iterMatch = *it;
        RouteStop routeStop;
        StopTime stopTime = 60 * stoul(iterMatch.str(1)) + stoul(iterMatch.str(2));
        if (!isStopTimeCorrect(stopTime, prevStopTime)) return parseError();
        routeStop.second = stopTime;
        routeStop.first = iterMatch.str(3);
        routeStops.push_back(routeStop);

        prevStopTime = stopTime;
    }

    return ParseResult(ADD_ROUTE, AddRoute(lineNum, routeStops));
}

ParseResult parseAddTicket(const std::string& line) {
    std::regex rgx(R"(^([a-zA-Z ]+) (\d+)\.(\d{2}) ([1-9]\d+)$)");
    std::smatch match;

    if (!std::regex_match(line, match, rgx)) {
        return parseError();
    }

    std::string name = match.str(1);
    Price integerPart = std::stoul(match.str(2));
    Price decimalPart = std::stoul(match.str(3));
    Price fullPrice = 100 * integerPart + decimalPart;
    ValidTime validTime = std::stoull(match.str(4));

    Ticket addTicket = std::make_tuple(name, fullPrice, validTime);
    return ParseResult(ADD_TICKET, Request(addTicket));
}



ParseResult parseQuery(const std::string& line) {
    std::regex rgx(R"(^\?((?: [a-zA-Z^_]+ \d+)+) ([a-zA-Z^_]+)$)");
    std::smatch match;

    if (!std::regex_match(line, match, rgx)) {
        return parseError();
    }
    Query query;
    std::regex iterRgx(R"( ([a-zA-Z^_]+) (\d+))");
    std::string iterStr = match.str(1);
    for (auto it = std::sregex_iterator(iterStr.begin(), iterStr.end(), iterRgx); it != std::sregex_iterator(); ++it) {
        std::smatch iterMatch = *it;
        QueryStop queryStop;
        queryStop.first = iterMatch.str(1);
        queryStop.second = std::stoul(iterMatch.str(2));
        query.push_back(queryStop);
    }

    QueryStop queryStop = std::make_pair(match.str(match.size() - 1), 0);
    query.push_back(queryStop);

    return ParseResult(QUERY, query);
}

ParseResult parseInputLine(const std::string& line) {
    switch (getRequestType(line)) {
        case ADD_ROUTE:
            return parseAddRoute(line);
        case ADD_TICKET:
            return parseAddTicket(line);
        case QUERY:
            return parseQuery(line);
        case IGNORE:
            return parseIgnore();
        case ERROR:
            return parseError();
}

int main() {
    std::string buffer;
    std::string debug_enum_array[5] = { "ADD_ROUTE", "ADD_TICKET", "QUERY", "IGNORE", "ERROR"};

    while(std::getline(std::cin, buffer)) {
        ParseResult parseResult = parseInputLine(buffer);
        std::cout << debug_enum_array[parseResult.first] << std::endl;
    }
    return 0;
}