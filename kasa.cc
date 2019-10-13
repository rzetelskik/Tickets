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
using TicketMap = std::unordered_map<std::string, Price>;
using TicketSortedMap = std::map<std::pair<Price, std::string>, ValidTime>;
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

using Response = std::variant<std::string, std::vector<std::string>>;
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
    std::regex rgx(R"(^([a-zA-Z ]+) (\d+)\.(\d{2}) ([1-9]\d?+)$)");
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

/*std::pair<Price, ValidTime> getTicketDataByName(const std::string& name, const TicketMap& ticketMap,
        const TicketSortedMap& ticketSortedMap) {
    //TODO tu sie moze wyjebac cos w sumie
    Price price = ticketMap.find(name)->second;

    return ticketSortedMap.find({price, name})->second;
}*/

enum CountingResultType {
    COUNTING_FOUND,
    COUNTING_WAIT,
    COUNTING_NOT_FOUND
};
using CountingInfo = std::variant<StopTime, std::string>;
using CountingResult = std::pair<CountingResultType, std::optional<CountingInfo>>;

CountingResult countTime(const Query& tour, const Timetable& timeTable) {
    StopTime arrivalTime = 0;
    StopTime startTime = 0;
    StopTime endTime = 0;

    for (std::size_t i = 0; i < tour.size() - 1; i++) {
        auto& current = tour[i];
        auto& next = tour[i + 1];

        const auto& currentName = current.first;
        const auto& line = timeTable.at(current.second);
        const auto& nextName = next.first;

        auto startStop = line.find(currentName);
        auto endStop = line.find(nextName);

        if(startStop == line.end() || endStop == line.end()) {
            return CountingResult(COUNTING_NOT_FOUND, std::nullopt);
        }
        else if(startStop->second > endStop->second) {
            return CountingResult(COUNTING_NOT_FOUND, std::nullopt);
        }
        else if(i != 0 && arrivalTime != startStop->second) {
            if(arrivalTime < startStop->second) {
                return CountingResult(COUNTING_WAIT, CountingInfo(startStop->first));
            }
            else {
                return CountingResult(COUNTING_NOT_FOUND, std::nullopt);
            }
        }
        else {
            arrivalTime = endStop->second;

            if(i == 0) {
                startTime = startStop->second;
            }
            if(i == tour.size() - 2) {
                endTime = endStop->second;
            }
        }
    }

    return CountingResult(COUNTING_FOUND, CountingInfo(endTime - startTime));
}

std::vector<std::string> selectTickets(const TicketSortedMap& tickets, StopTime totalTime) {
    const uint MAX_TICKETS = 3;
    std::string empty;
    unsigned long long minPrice = ULLONG_MAX;

    std::string ticketA = empty;
    std::string ticketB = empty;
    std::string ticketC = empty;

    //tickets are sorted ascending by price
    for(auto itA = tickets.cbegin(); itA != tickets.cend(); itA++) {
        const auto& keyA = itA->first;

        const auto& nameA = keyA.second;
        auto priceA = keyA.first;
        auto timeA = itA->second;

        unsigned long long currentPriceA = priceA;
        ValidTime currentTimeA = timeA;

        if(currentTimeA >= totalTime) {
            if(currentPriceA <= minPrice) {
                minPrice = currentPriceA;

                ticketA = nameA;
                ticketB = empty;
                ticketC = empty;
            }
            break;
        }

        for(auto itB = itA; itB != tickets.cend(); itB++) {
            const auto& keyB = itB->first;

            const auto& nameB = keyB.second;
            auto priceB = keyB.first;
            auto timeB = itB->second;

            unsigned long long currentPriceB = currentPriceA + priceB;
            ValidTime currentTimeB = currentTimeA + timeB;

            if(currentTimeB >= totalTime) {
                if(currentPriceB <= minPrice){
                    minPrice = currentPriceB;

                    ticketA = nameA;
                    ticketB = nameB;
                    ticketC = empty;
                }
                break;
            }

            for(auto itC = itB; itC != tickets.cend(); itC++) {
                const auto& keyC = itC->first;

                const auto& nameC = keyC.second;
                auto priceC = keyC.first;
                auto timeC = itC->second;

                unsigned long long currentPriceC = currentPriceB + priceC;
                ValidTime currentTimeC = currentTimeB + timeC;

                if(currentTimeC >= totalTime) {
                    if(currentPriceC <= minPrice){
                        minPrice = currentPriceC;

                        ticketA = nameA;
                        ticketB = nameB;
                        ticketC = nameC;
                    }
                    break;
                }
            }
        }

    }

    std::vector<std::string> result;

    if(!ticketA.empty())
        result.push_back(ticketA);
    if(!ticketB.empty())
        result.push_back(ticketB);
    if(!ticketC.empty())
        result.push_back(ticketC);

    return  result;
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
    ticketSortedMap.insert({{price, name}, validTime});
}

ProcessResult processAddTicket(const AddTicket& addTicket, TicketMap& ticketMap, TicketSortedMap& ticketSortedMap) {
    const std::string& ticketName = std::get<0>(addTicket);
    if (isTicketNameRepeated(ticketName, ticketMap)) {
        return processError();
    }

    insertTicket(addTicket, ticketMap, ticketSortedMap);

    return processNoResponse();
}

ProcessResult processQuery(const Query& query, Timetable& timetable, TicketSortedMap& ticketMap) {
    auto countingResult = countTime(query, timetable);

    switch (countingResult.first) {
        case COUNTING_FOUND:
        {
            auto tickets = selectTickets(ticketMap, std::get<StopTime>(countingResult.second.value_or(0)));

            if(!tickets.empty()) {
                return ProcessResult(FOUND, tickets);
            }
            else {
                return ProcessResult(NOT_FOUND, std::nullopt);
            }
        }
        case COUNTING_WAIT:
            return ProcessResult(WAIT, Response(std::get<std::string>(countingResult.second.value())));
        case COUNTING_NOT_FOUND:
            return ProcessResult(NOT_FOUND, std::nullopt);
    }

    return ProcessResult(NOT_FOUND, std::nullopt);
}

ProcessResult processRequest(const ParseResult& parseResult, TicketMap& ticketMap, TicketSortedMap& ticketSortedMap,
        Timetable& timetable) {
    switch (parseResult.first) {
        case ADD_ROUTE:
            return processAddRoute(std::get<AddRoute>(parseResult.second.value()), timetable);
        case ADD_TICKET:
            return processAddTicket(std::get<AddTicket>(parseResult.second.value()), ticketMap, ticketSortedMap);
        case QUERY:
            return processQuery(std::get<Query>(parseResult.second.value()), timetable, ticketSortedMap);
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
    bool first = true;
    for (auto& ticket: tickets) {
        if(!first)
            std::cout << "; ";
        else
            first = false;

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

    return 0;
}