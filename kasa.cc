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
using TicketMap = std::unordered_map<std::string, Price>;
using TicketSortedMap = std::map<std::pair<Price, std::string>, std::pair<Price, ValidTime>>;
using StopNameSet = std::unordered_set<std::string>;
using StopTime = unsigned long; //TODO: change type?
using Route = std::map<std::string, StopTime>;
using LineNum = unsigned long long;
using Timetable = std::unordered_map<LineNum, Route>;
using AddRoute = std::pair<LineNum, Route>;
using AddTicket = std::pair<std::string, std::pair<Price, ValidTime>>;
using QueryStop = std::pair<std::string, StopTime>;
using Query = std::vector<QueryStop>;
using Request = std::variant<AddRoute, AddTicket, Query>;

enum RequestType {
    ADD_ROUTE,
    ADD_TICKET,
    QUERY,
    IGNORE,
    ERROR_REQ
};

using ParseResult = std::pair<RequestType, std::optional<Request>>;

enum ResponseType {
    FOUND,
    WAIT,
    NOT_FOUND,
    NO_RESPONSE,
    ERROR_RESP
};

using Response = std::variant<StopTime, std::string, std::vector<std::string>>;
using ProcessResult = std::pair<ResponseType, std::optional<Response>>;

//region Parsing
RequestType getRequestType(const std::string& line) {
    if(line.empty()) {
        return IGNORE;
    }
    char c = line.at(0);

    if(isdigit(c)) {
        return ADD_ROUTE;
    } else if(isalpha(c) || isspace(c)) {
        return ADD_TICKET;
    } else if(c == '?') {
        return QUERY;
    } else {
        return ERROR_REQ;
    }
}

ParseResult parseIgnore() {
    return ParseResult(IGNORE, std::nullopt);
}

ParseResult parseError() {
    return ParseResult(ERROR_REQ, std::nullopt);
}

bool isStopTimeCorrect(StopTime stopTime, StopTime prevStopTime) {
    return (stopTime > prevStopTime && stopTime >= 355 && stopTime <= 1281);
}

bool isStopRepeated(const std::string& stopName, const Route& route) {
    return (route.find(stopName) != route.end());
}

ParseResult parseAddRoute(const std::string& line) {
    std::regex rgx(R"(^(\d+)((?: (?:[5-9]|1\d|2[0-1]):[0-5]\d [a-zA-Z^_]+)+)$)");
    std::smatch match;

    //TODO exceptions
    if(!std::regex_match(line, match, rgx)) {
        return parseError();
    }
    LineNum lineNum = stoull(match.str(1));
    Route route;
    StopTime prevStopTime = 0;
    std::string iterStr = match.str(2);
    std::regex iterRgx(R"( ([5-9]|1\d|2[0-1])\:([0-5]\d) ([a-zA-Z^_]+))");

    for(auto it = std::sregex_iterator(iterStr.begin(), iterStr.end(), iterRgx); it != std::sregex_iterator(); ++it) {
        std::smatch iterMatch = *it;

        const std::string& stopName = iterMatch.str(3);
        if (isStopRepeated(stopName, route)) {
            return parseError();
        }

        StopTime stopTime = 60 * stoul(iterMatch.str(1)) + stoul(iterMatch.str(2));
        if(!isStopTimeCorrect(stopTime, prevStopTime)) {
            return parseError();
        }

        route.insert({stopName, stopTime});

        prevStopTime = stopTime;
    }

    return ParseResult(ADD_ROUTE, AddRoute(lineNum, route));
}

ParseResult parseAddTicket(const std::string& line) {
    std::regex rgx(R"(^([a-zA-Z ]+) (\d+)\.(\d{2}) ([1-9]\d+)$)");
    std::smatch match;

    if(!std::regex_match(line, match, rgx)) {
        return parseError();
    }

    //TODO exceptions
    std::string name = match.str(1);

    Price integerPart = std::stoul(match.str(2));
    Price decimalPart = std::stoul(match.str(3));
    Price fullPrice = 100 * integerPart + decimalPart;
    ValidTime validTime = std::stoull(match.str(4));

    AddTicket addTicket = {name, {fullPrice, validTime}};
    return ParseResult(ADD_TICKET, Request(addTicket));
}

ParseResult parseQuery(const std::string& line) {
    std::regex rgx(R"(^\?((?: [a-zA-Z^_]+ \d+)+) ([a-zA-Z^_]+)$)");
    std::smatch match;

    //TODO exceptions
    if(!std::regex_match(line, match, rgx)) {
        return parseError();
    }
    Query query;
    std::regex iterRgx(R"( ([a-zA-Z^_]+) (\d+))");
    std::string iterStr = match.str(1);
    for(auto it = std::sregex_iterator(iterStr.begin(), iterStr.end(), iterRgx); it != std::sregex_iterator(); ++it) {
        std::smatch iterMatch = *it;

        QueryStop queryStop = {iterMatch.str(1), std::stoul(iterMatch.str(2))};
        query.push_back(queryStop);
    }

    QueryStop queryStop = {match.str(match.size() - 1), 0};
    query.push_back(queryStop);

    return ParseResult(QUERY, query);
}

ParseResult parseInputLine(const std::string& line) {
    switch(getRequestType(line)) {
        case ADD_ROUTE:
            return parseAddRoute(line);
        case ADD_TICKET:
            return parseAddTicket(line);
        case QUERY:
            return parseQuery(line);
        case IGNORE:
            return parseIgnore();
        default:
            break;
    }

    return parseError();
}
//endregion

//region Processing

std::pair<Price, ValidTime> getTicketDataByName(const std::string& name, const TicketMap& ticketMap,
        const TicketSortedMap& ticketSortedMap) {
    //TODO tu sie moze wyjebac cos w sumie
    Price price = ticketMap.find(name)->second;

    return ticketSortedMap.find({price, name})->second;
}

//TODO to nie bedzie zwracalo processResult prawdopodobnie
ProcessResult countTime(const Query& tour, const Timetable& timeTable) {
    StopTime arrivalTime = 0;
    StopTime startTime = 0;
    StopTime endTime = 0;

    for (std::size_t i = 0; i < tour.size() - 1; i++) {
        auto& current = tour[i];
        auto& next = tour[i + 1];

        const auto& currentName = current.first;
        const auto& line = timeTable.at(current.second);
        const auto& nextName = next.first;

        bool started = false;
        bool ended = false;

        for (auto& stop: line) {
            if (!started) {
                if (currentName == stop.first) {
                    if (i == 0) {
                        startTime = stop.second;
                    } else if (arrivalTime != stop.second) {
                        if (arrivalTime > stop.second) {
                            return ProcessResult(NOT_FOUND, std::nullopt);
                        } else {
                            return ProcessResult(WAIT, Response(stop.first));
                        }
                    }

                    arrivalTime = stop.second;
                    started = true;
                }
            } else if (nextName == stop.first) {
                ended = true;

                if (i == tour.size() - 2) {
                    endTime = stop.second;
                }

                arrivalTime = stop.second;
                break;
            }
        }

        if(!started || !ended) {
            return ProcessResult(NOT_FOUND, std::nullopt);
        }
    }

    return ProcessResult(FOUND, Response(endTime - startTime));
}

ProcessResult processError() {
    return ProcessResult(ERROR_RESP, std::nullopt);
}

ProcessResult processNoResponse() {
    return ProcessResult(NO_RESPONSE, std::nullopt);
}

bool isLineRepeated(const LineNum& lineNum, const Timetable& timetable) {
    return (timetable.find(lineNum) != timetable.end());
}

ProcessResult processAddRoute(const AddRoute& addRoute, Timetable& timetable) {
    if (isLineRepeated(addRoute.first, timetable)) {
        return processError();
    }

    timetable.insert(addRoute);

    return ProcessResult(NO_RESPONSE, std::nullopt);
}

bool isTicketNameRepeated(const std::string& ticketName, const TicketMap& ticketMap) {
    return (ticketMap.find(ticketName) != ticketMap.end());
}

void insertTicket(const AddTicket& addTicket, TicketMap& ticketMap, TicketSortedMap& ticketSortedMap) {
    const std::string& name = addTicket.first;
    Price price = addTicket.second.first;
    ValidTime validTime = addTicket.second.second;

    ticketMap.insert({name, price});
    ticketSortedMap.insert({{price, name}, {price, validTime}});
}

ProcessResult processAddTicket(const AddTicket& addTicket, TicketMap& ticketMap, TicketSortedMap& ticketSortedMap) {
    const std::string& ticketName = std::get<0>(addTicket);
    if (isTicketNameRepeated(ticketName, ticketMap)) {
        return processError();
    }

    insertTicket(addTicket, ticketMap, ticketSortedMap);

    return processNoResponse();
}

ProcessResult processQuery() {
    std::cout << "processQuery" << std::endl;
    return processNoResponse();
}

ProcessResult processRequest(const ParseResult& parseResult, TicketMap& ticketMap, TicketSortedMap& ticketSortedMap,
        Timetable& timetable) {
    switch (parseResult.first) {
        case ADD_ROUTE:
            return processAddRoute(std::get<AddRoute>(parseResult.second.value()), timetable);
        case ADD_TICKET:
            return processAddTicket(std::get<AddTicket>(parseResult.second.value()), ticketMap, ticketSortedMap);
        case QUERY:
            return processQuery();
        case IGNORE:
            return processNoResponse();
        default:
            break;
    }
    return processError();
}

//endregion

void printFound(const std::vector<std::string>& tickets) {
    std::cout << '!';
    for (auto& ticket: tickets) {
        std::cout << ticket;
    }
    std::cout << std::endl;
}

void printWait(const std::string& stop) {
    std::cout << ":-(" << stop << std::endl;
}

void printNotFound() {
    std::cout << ":-|" << std::endl;
}

void printError(const std::string& inputLine, unsigned int lineCounter) {
    std::cerr << "Error in line " << lineCounter << ": " << inputLine << std::endl;
}

void printOutput(const ProcessResult& processResult, const std::string& inputLine, unsigned int lineCounter) {
    switch(processResult.first) {
        case FOUND:
            return printFound(std::get<std::vector<std::string>>(processResult.second.value()));
        case WAIT:
            return printWait(std::get<std::string>(processResult.second.value()));
        case NOT_FOUND:
            return printNotFound();
        case NO_RESPONSE:
            break;
        case ERROR_RESP:
            return printError(inputLine, lineCounter);
    }
}

int main() {
    TicketMap ticketMap;
    TicketSortedMap ticketSortedMap;
    Timetable timetable;

    unsigned int lineCounter = 1;
    std::string buffer;

    while(std::getline(std::cin, buffer)) {
        ParseResult parseResult = parseInputLine(buffer);
        ProcessResult processResult = processRequest(parseResult, ticketMap, ticketSortedMap, timetable);
        printOutput(processResult, buffer, lineCounter);
        lineCounter++;
    }


//    // COUNTING TEST
//
//    StopNameSet names;
//    Route r;
//
//    std::string n1("name1");
//    names.insert(n1);
//    Stop s1 = Stop(n1, 10);
//
//    std::string n2("name2");
//    names.insert(n2);
//    Stop s2 = Stop(n2, 20);
//
//    std::string n3("name3");
//    names.insert(n3);
//    Stop s3 = Stop(n3, 30);
//
//    std::string n4("name4");
//    names.insert(n4);
//    Stop s4 = Stop(n4, 40);
//
//    std::string n5("name5");
//    names.insert(n5);
//    Stop s5 = Stop(n5, 50);
//
//    std::string n6("name6");
//    names.insert(n6);
//    Stop s6 = Stop(n6, 60);
//
//    r.push_back(s1);
//    r.push_back(s2);
//    r.push_back(s3);
//    r.push_back(s4);
//    r.push_back(s5);
//    r.push_back(s6);
//
//
//    std::string na1("line2_1");
//    names.insert(na1);
//    Stop ns1(na1, 15);
//
//    Stop ns2(n4, 40);
//
//    std::string na3("line2_3");
//    names.insert(na3);
//    Stop ns3(na3, 44);
//
//    std::string na4("line2_4");
//    names.insert(na4);
//    Stop ns4(na4, 58);
//
//    Stop ns5(n6, 70);
//
//    Route r2;
//    r2.push_back(ns1);
//    r2.push_back(ns2);
//    r2.push_back(ns3);
//    r2.push_back(ns4);
//    r2.push_back(ns5);
//
//    Timetable tt;
//    tt.insert({3, r});
//    tt.insert({5, r2});
//
//
//    QueryStop ts1("name2", 3);
//    QueryStop ts2("name6", 0);
//    Query tour;
//
//    tour.push_back(ts1);
//    tour.push_back(ts2);
//
//
//    QueryStop nts1("name1", 3);
//    QueryStop nts2("name4", 5);
//    QueryStop nts3("line2_4", 0);
//
//    Query tur2;
//    tur2.push_back(nts1);
//    tur2.push_back(nts2);
//    tur2.push_back(nts3);
//
//
//    auto res2 = countTime(tur2, tt);
//
//    std::cout << res2.index() << std::endl;
//
//    if(res2.index() == 0) {
//        std::cout << std::get<StopTime>(res2);
//    }


    return 0;
}