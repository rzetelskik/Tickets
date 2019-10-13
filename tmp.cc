#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <variant>
#include <regex>
#include <climits>

using Price = unsigned long;
using ValidTime = unsigned long long;
using Ticket = std::tuple<std::string, Price, ValidTime>;
struct TicketComp {
    bool operator() (const Ticket& r, const Ticket& l) {
        Price rP = std::get<Price>(r);
        Price lP = std::get<Price>(l);

        if(rP == lP) {
            ValidTime rT = std::get<ValidTime>(r);
            ValidTime lT = std::get<ValidTime>(l);

            return rT >= lT;
        }
        return rP < lP;
    }
};
using TicketSet = std::set<Ticket, TicketComp>;
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
using CountResult = std::variant<StopTime, std::string, bool>; //TODO: change error variant?



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
    if(!std::regex_match(line, match, rgx)) {
        return parseError();
    }
    LineNum lineNum = stoull(match.str(1));
    std::vector<RouteStop> routeStops;
    StopTime prevStopTime = 0;
    std::string iterStr = match.str(2);
    std::regex iterRgx(R"( ([5-9]|1\d|2[0-1])\:([0-5]\d) ([a-zA-Z^_]+))");

    for(auto it = std::sregex_iterator(iterStr.begin(), iterStr.end(), iterRgx); it != std::sregex_iterator(); ++it) {
        std::smatch iterMatch = *it;
        RouteStop routeStop;
        StopTime stopTime = 60 * stoul(iterMatch.str(1)) + stoul(iterMatch.str(2));
        if(!isStopTimeCorrect(stopTime, prevStopTime)) return parseError();
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

    if(!std::regex_match(line, match, rgx)) {
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

    if(!std::regex_match(line, match, rgx)) {
        return parseError();
    }
    Query query;
    std::regex iterRgx(R"( ([a-zA-Z^_]+) (\d+))");
    std::string iterStr = match.str(1);
    for(auto it = std::sregex_iterator(iterStr.begin(), iterStr.end(), iterRgx); it != std::sregex_iterator(); ++it) {
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
    switch(getRequestType(line)) {
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

    return parseError();
}
//endregion

//region Processing
CountResult countTime(const Query& tour, const Timetable& timeTable) {
    StopTime arrivalTime = 0;
    StopTime startTime = 0;
    StopTime endTime = 0;

    for(std::size_t i = 0; i < tour.size() - 1; i++) {
        auto& current = tour[i];
        auto& next = tour[i + 1];

        const auto& currentName = current.first;
        const auto& line = timeTable.at(current.second);
        const auto& nextName = next.first;

        bool started = false;
        bool ended = false;

        for(auto& stop: line) {
            if(!started) {
                if(currentName == stop.first) {
                    if(i == 0) {
                        startTime = stop.second;
                    } else if(arrivalTime != stop.second) {
                        if(arrivalTime > stop.second) {
                            return CountResult(false);
                        } else {
                            return CountResult(stop.first);
                        }
                    }

                    arrivalTime = stop.second;
                    started = true;
                }
            } else {
                if(nextName == stop.first) {
                    ended = true;

                    if(i == tour.size() - 2) {
                        endTime = stop.second;
                    }

                    arrivalTime = stop.second;
                    break;
                }
            }
        }

        if(!started || !ended) {
            CountResult(false);
        }
    }

    return CountResult(endTime - startTime);
}

std::vector<std::string&> selectTickets(const TicketSet& tickets, StopTime totalTime) {
    const uint MAX_TICKETS = 3;
    std::string empty;
    unsigned long long minPrice = ULLONG_MAX;

    std::string& ticketA = empty;
    std::string& ticketB = empty;
    std::string& ticketC = empty;

    //std::string ticks[3] = {"", "", ""};

    //tickets are sorted ascending by price
    for(auto itA = tickets.cbegin(); itA != tickets.cend(); itA++) {
        unsigned long long currentPriceA = std::get<Price>(*itA);
        ValidTime currentTimeA = std::get<ValidTime>(*itA);

        if(currentTimeA >= totalTime) {
            if(currentPriceA <= minPrice) {
                minPrice = currentPriceA;
                //ticks[0] = std::get<std::string>(*itA);
                //ticks[1] = "";
                //ticks[2] = "";

                ticketA = std::get<std::string>(*itA);
                ticketB = empty;
                ticketC = empty;
            }
            break;
        }

        for(auto itB = itA; itB != tickets.cend(); itB++) {
            unsigned long long currentPriceB = currentPriceA + std::get<Price>(*itB);
            ValidTime currentTimeB = currentTimeA + std::get<ValidTime>(*itB);

            if(currentTimeB >= totalTime) {
                if(currentPriceB <= minPrice){
                    minPrice = currentPriceB;
                    //ticks[0] = std::get<std::string>(*itA);
                    //ticks[1] = std::get<std::string>(*itB);
                    //ticks[2] = "";

                    ticketA = std::get<std::string>(*itA);
                    ticketB = std::get<std::string>(*itB);
                    ticketC = empty;
                }
                break;
            }

            for(auto itC = itB; itC != tickets.cend(); itC++) {
                unsigned long long currentPriceC = currentPriceB + std::get<Price>(*itC);
                ValidTime currentTimeC = currentTimeB + std::get<ValidTime>(*itC);

                if(currentTimeC >= totalTime) {
                    if(currentPriceC <= minPrice){
                        minPrice = currentPriceC;
                        //ticks[0] = std::get<std::string>(*itA);
                        //ticks[1] = std::get<std::string>(*itB);
                        //ticks[2] = std::get<std::string>(*itC);

                        ticketA = std::get<std::string>(*itA);
                        ticketB = std::get<std::string>(*itB);
                        ticketC = std::get<std::string>(*itC);
                    }
                    break;
                }
            }
        }

    }

    std::vector<std::string&> result;
    /*for(const auto& t: ticks) {
        if(!t.empty())
            result.push_back(t);
    }*/

    if(!ticketA.empty())
        result.push_back(ticketA);
    if(!ticketB.empty())
        result.push_back(ticketB);
    if(!ticketC.empty())
        result.push_back(ticketC);

    return  result;
}
//endregion


int main() {
    std::string t1("10 min");
    std::string t2("30 min");
    std::string t3("60 min");

    Ticket ti1 = std::make_tuple(t1, 2, 10);
    Ticket ti2 = std::make_tuple(t2, 5, 30);
    Ticket ti3 = std::make_tuple(t3, 8, 60);

    TicketSet ts;
    ts.insert(ti3);
    ts.insert(ti1);
    ts.insert(ti2);

    auto res = selectTickets(ts, 20);
    std::cout << "czas: " << 20 << std::endl;
    for(auto r: res) {
        std::cout << r << std::endl;
    }
    std::cout << std::endl;

    auto res2 = selectTickets(ts, 45);
    std::cout << "czas: " << 45 << std::endl;
    for(auto r: res2) {
        std::cout << r << std::endl;
    }
    std::cout << std::endl;

    auto res3 = selectTickets(ts, 600);
    std::cout << "czas: " << 600 << std::endl;
    for(auto r: res3) {
        std::cout << r << std::endl;
    }
    std::cout << std::endl;

    auto res4 = selectTickets(ts, 70);
    std::cout << "czas: " << 70 << std::endl;
    for(auto r: res4) {
        std::cout << r << std::endl;
    }
    std::cout << std::endl;

    return 0;
}