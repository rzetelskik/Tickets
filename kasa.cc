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
auto ticketCmp = [](const Ticket& t1, const Ticket& t2) { return std::get<1>(t1) < std::get<1>(t2);}; //TODO no tutaj troche slabo, jakis lepszy pomysl?
using TicketSet = std::set<Ticket, decltype(ticketCmp)>;
using TicketNameSet = std::unordered_set<std::string>;
using StopNameSet = std::unordered_set<std::string>;
using StopTime = unsigned long; //TODO: change type?
using Stop = std::pair<const std::string&, StopTime>;
using Route = std::vector<Stop>;
using LineNum = unsigned long long;
using Timetable = std::unordered_map<LineNum, Route>;
using RouteStop = std::pair<std::string, StopTime>;
using AddRoute = std::pair<LineNum, std::vector<RouteStop>>;
using QueryStop = std::pair<std::string, StopTime>;
using Query = std::vector<QueryStop>;
using Request = std::variant<AddRoute, Ticket, Query>;

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

bool isStopNameRepeated(const std::string& stopName, const std::unordered_set<std::string>& visitedStops) {
    return (visitedStops.find(stopName) != visitedStops.end());
}

ParseResult parseAddRoute(const std::string& line) {
    std::regex rgx(R"(^(\d+)((?: (?:[5-9]|1\d|2[0-1]):[0-5]\d [a-zA-Z^_]+)+)$)");
    std::smatch match;

    //TODO exceptions
    if(!std::regex_match(line, match, rgx)) {
        return parseError();
    }
    LineNum lineNum = stoull(match.str(1));
    std::vector<RouteStop> routeStops;
    std::unordered_set<std::string> visitedStopsNames;
    StopTime prevStopTime = 0;
    std::string iterStr = match.str(2);
    std::regex iterRgx(R"( ([5-9]|1\d|2[0-1])\:([0-5]\d) ([a-zA-Z^_]+))");

    for(auto it = std::sregex_iterator(iterStr.begin(), iterStr.end(), iterRgx); it != std::sregex_iterator(); ++it) {
        std::smatch iterMatch = *it;

        const std::string& stopName = iterMatch.str(3);
        if (isStopNameRepeated(stopName, visitedStopsNames)) {
            return parseError();
        }

        StopTime stopTime = 60 * stoul(iterMatch.str(1)) + stoul(iterMatch.str(2));
        if(!isStopTimeCorrect(stopTime, prevStopTime)) {
            return parseError();
        }

        RouteStop routeStop = {stopName, stopTime};
        routeStops.push_back(routeStop);

        prevStopTime = stopTime;
        visitedStopsNames.insert(stopName);
    }

    return ParseResult(ADD_ROUTE, AddRoute(lineNum, routeStops));
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

    Ticket addTicket = {name, fullPrice, validTime};
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

ProcessResult processAddRoute(const AddRoute& addRoute, StopNameSet& stopNameSet,
        Timetable& timetable) {
    if (isLineRepeated(addRoute.first, timetable)) {
        return processError();
    }
    
    Route newRoute;
    for (const RouteStop& curr: addRoute.second) {
        auto ret = stopNameSet.insert(curr.first);
        const std::string& stopName = *ret.first;

        newRoute.push_back(std::make_pair(stopName, curr.second));
    }

    timetable.insert({addRoute.first, newRoute});

    return ProcessResult(NO_RESPONSE, std::nullopt);
}

bool isTicketNameRepeated(const std::string& ticketName, const TicketNameSet& ticketNameSet) {
    return (ticketNameSet.find(ticketName) != ticketNameSet.end());
}

ProcessResult processAddTicket(const Ticket& ticket, TicketNameSet& ticketNameSet, TicketSet& ticketSet) {
    const std::string& ticketName = std::get<0>(ticket);
    if (isTicketNameRepeated(ticketName, ticketNameSet)) {
        return processError();
    }

    ticketNameSet.insert(ticketName);
    ticketSet.insert(ticket);

    return processNoResponse();
}

ProcessResult processQuery() {
    return processError();
}

ProcessResult processRequest(const ParseResult& parseResult, TicketNameSet& ticketNameSet, TicketSet& ticketSet,
        StopNameSet& stopNameSet, Timetable& timetable) {
    switch (parseResult.first) {
        case ADD_ROUTE:
            return processAddRoute(std::get<AddRoute>(parseResult.second.value()), stopNameSet, timetable);
        case ADD_TICKET:
            return processAddTicket(std::get<Ticket>(parseResult.second.value()), ticketNameSet, ticketSet);
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


int main() {
    TicketSet ticketSet(ticketCmp); //TODO trzeba to ogarnac, usunac using? i sprawdzic czy to sortowanie wgl dziala
    TicketNameSet ticketNameSet;
    StopNameSet stopNameSet;
    Timetable timetable;

    unsigned int counter = 1;
    std::string buffer;

    while(std::getline(std::cin, buffer)) {
        ParseResult parseResult = parseInputLine(buffer);
        ProcessResult processResult = processRequest(parseResult, ticketNameSet, ticketSet, stopNameSet, timetable);
        counter++;
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