#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <variant>
#include <regex>
#include <climits>

namespace {
    using Price = unsigned long;
    using ValidTime = unsigned long long;
    using TicketMap = std::unordered_map<std::string, Price>;
    using TicketSortedMap = std::map<std::pair<Price, std::string>, ValidTime>;
    using StopTime = unsigned long; //TODO: change type?
    using Route = std::unordered_map<std::string, StopTime>;
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

    enum CountingResultType {
        COUNTING_FOUND,
        COUNTING_WAIT,
        COUNTING_NOT_FOUND
    };

    using CountingInfo = std::variant<StopTime, std::string>;
    using CountingResult = std::pair<CountingResultType, std::optional<CountingInfo>>;
    using SelectedTickets = std::vector<std::string>;

    enum ResponseType {
        FOUND,
        WAIT,
        NOT_FOUND,
        NO_RESPONSE,
        ERROR_RESP
    };

    using Response = std::variant<std::string, std::vector<std::string>>;
    using ProcessResult = std::pair<ResponseType, std::optional<Response>>;

    RequestType getRequestType(const std::string &line) {
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
        //TODO const the predefined values
        return (stopTime > prevStopTime && stopTime >= 355 && stopTime <= 1281);
    }

    bool isStopRepeated(const std::string &stopName, const Route &route) {
        return (route.find(stopName) != route.end());
    }

    ParseResult parseAddRoute(const std::string &line) {
        //TODO split regex + add static const to one-time strings
        std::regex rgx(R"(^(\d+)((?: (?:[5-9]|1\d|2[0-1]):[0-5]\d [a-zA-Z^_]+)+)$)");
        std::smatch match;

        //TODO exceptions
        if (!std::regex_match(line, match, rgx)) {
            return parseError();
        }
        //TODO name the group numbers, statics consts/pairs, wtv
        LineNum lineNum = stoull(match.str(1));
        Route route;
        StopTime prevStopTime = 0;
        std::string iterStr = match.str(2);
        std::regex iterRgx(R"( ([5-9]|1\d|2[0-1])\:([0-5]\d) ([a-zA-Z^_]+))");

        for (auto it = std::sregex_iterator(iterStr.begin(), iterStr.end(), iterRgx);
             it != std::sregex_iterator(); ++it) {
            std::smatch iterMatch = *it;

            const std::string &stopName = iterMatch.str(3);
            if (isStopRepeated(stopName, route)) {
                return parseError();
            }

            StopTime stopTime = 60 * stoul(iterMatch.str(1)) + stoul(iterMatch.str(2));
            if (!isStopTimeCorrect(stopTime, prevStopTime)) {
                return parseError();
            }

            route.insert({stopName, stopTime});

            prevStopTime = stopTime;
        }

        return ParseResult(ADD_ROUTE, AddRoute(lineNum, route));
    }

    bool isPriceCorrect(const Price &price) {
        return (price > 0.00);
    }

    ParseResult parseAddTicket(const std::string &line) {
        std::regex rgx(R"(^([a-zA-Z ]+) (\d+)\.(\d{2}) ([1-9]\d*)$)");
        std::smatch match;

        if (!std::regex_match(line, match, rgx)) {
            return parseError();
        }

        //TODO exceptions
        std::string name = match.str(1);

        Price integerPart = std::stoul(match.str(2));
        Price decimalPart = std::stoul(match.str(3));
        Price fullPrice = 100 * integerPart + decimalPart;

        if (!isPriceCorrect(fullPrice)) {
            return parseError();
        }

        ValidTime validTime = std::stoull(match.str(4));

        AddTicket addTicket = {name, {fullPrice, validTime}};
        return ParseResult(ADD_TICKET, Request(addTicket));
    }

    ParseResult parseQuery(const std::string &line) {
        std::regex rgx(R"(^\?((?: [a-zA-Z^_]+ \d+)+) ([a-zA-Z^_]+)$)");
        std::smatch match;

        //TODO exceptions
        if (!std::regex_match(line, match, rgx)) {
            return parseError();
        }
        Query query;
        std::regex iterRgx(R"( ([a-zA-Z^_]+) (\d+))");
        std::string iterStr = match.str(1);
        for (auto it = std::sregex_iterator(iterStr.begin(), iterStr.end(), iterRgx);
             it != std::sregex_iterator(); ++it) {
            std::smatch iterMatch = *it;

            QueryStop queryStop = {iterMatch.str(1), std::stoul(iterMatch.str(2))};
            query.push_back(queryStop);
        }

        QueryStop queryStop = {match.str(match.size() - 1), 0};
        query.push_back(queryStop);

        return ParseResult(QUERY, query);
    }

    ParseResult parseInputLine(const std::string &line) {
        switch (getRequestType(line)) {
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


    using SectionCheckResult = std::pair<CountingResultType, std::optional<std::pair<StopTime, StopTime>>>;

    SectionCheckResult checkSection(const std::string& from, const std::string& to, const Route& line,
        StopTime& arrivalTime) {
        auto startStop = line.find(from);
        auto endStop = line.find(to);

        if(startStop == line.end() || endStop == line.end()) {
            return SectionCheckResult(COUNTING_NOT_FOUND, std::nullopt);
        }
        else if(startStop->second > endStop->second) {
            return SectionCheckResult(COUNTING_NOT_FOUND, std::nullopt);
        }
        else if(arrivalTime != 0 && arrivalTime != startStop->second) {
            return (startStop->second > arrivalTime)
                ? SectionCheckResult(COUNTING_WAIT, std::nullopt)
                : SectionCheckResult(COUNTING_NOT_FOUND, std::nullopt);;
        }
        else {
            arrivalTime = endStop->second;
            return SectionCheckResult(COUNTING_FOUND, std::pair(startStop->second, endStop->second));
        }
    }

    CountingResult countTime(const Query &tour, const Timetable &timeTable) {
        StopTime arrivalTime = 0;
        StopTime startTime = 0;
        StopTime endTime = 0;

        for (std::size_t i = 0; i < tour.size() - 1; i++) {
            auto &current = tour[i];
            auto &next = tour[i + 1];

            const auto &currentName = current.first;
            const auto &nextName = next.first;

            auto result = checkSection(currentName, nextName, timeTable.at(current.second), arrivalTime);
            switch (result.first) {
                case COUNTING_NOT_FOUND:
                    return  CountingResult(COUNTING_NOT_FOUND, std::nullopt);
                case COUNTING_WAIT:
                    return  CountingResult(COUNTING_WAIT, currentName);
                case COUNTING_FOUND:
                    if(i == 0)
                        startTime = result.second->first;
                    if(i == tour.size() - 2)
                        endTime = result.second->second;
                    break;
            }
        }

        return CountingResult(COUNTING_FOUND, CountingInfo(endTime - startTime));
    }

    using TicketIterator = std::_Rb_tree_const_iterator<std::pair<const std::pair<ulong, std::string>, unsigned  long long>>;
    using IteratingInfo = std::pair<unsigned long long, ValidTime>;
    enum TICKET_STATUS {
        NEW_BEST,
        TOO_EXPENSIVE,
        TOO_SHORT
    };
    using TicketSelectingResult = std::pair<TICKET_STATUS, IteratingInfo>;

    TicketSelectingResult checkTicket(StopTime totalTime, const TicketIterator& it, IteratingInfo info, unsigned long long minPrice) {
        const auto &key = it->first;
        auto price = key.first;
        auto time = it->second;

        unsigned long long currentPrice = info.first + price;
        ValidTime currentTime = info.second + time;

        if(currentTime > totalTime) {
            if(currentPrice <= minPrice) {
                return TicketSelectingResult(NEW_BEST, IteratingInfo(currentPrice, currentTime));
            }
            return TicketSelectingResult(TOO_EXPENSIVE, IteratingInfo(currentPrice, currentTime));
        }
        return TicketSelectingResult(TOO_SHORT, IteratingInfo(currentPrice, currentTime));
    }

    std::pair<bool, bool> updateLoop(TicketSelectingResult& result, unsigned long long& minPrice) {
        bool breakLoop = false;
        bool updateBestTickets = false;

        switch (result.first) {
            case NEW_BEST:
                minPrice = result.second.first;
                updateBestTickets = true;
                breakLoop = true;
                break;

            case TOO_EXPENSIVE:
                breakLoop = true;
                break;

            case TOO_SHORT:
                break;
        }

        return std::pair(updateBestTickets, breakLoop);
    }

    void select3rdTicket(const TicketSortedMap& tickets, StopTime totalTime, SelectedTickets& bestTickets,
                         unsigned long long minPrice, const TicketIterator& it, const IteratingInfo& prev,
                         const std::string& nameA, const std::string& nameB) {
        for (auto itC = it; itC != tickets.cend(); itC++) {
            const auto &name = itC->first.second;

            auto result = checkTicket(totalTime, itC, prev, minPrice);
            auto updateResult = updateLoop(result, minPrice);

            if(updateResult.first) {
                bestTickets = {nameA, nameB, name};
            }
            if(updateResult.second) {
                break;
            }
        }
    }

    void select2ndTicket(const TicketSortedMap& tickets, StopTime totalTime, SelectedTickets& bestTickets,
                         unsigned long long minPrice, const TicketIterator& it, const IteratingInfo& prev,
                         const std::string& nameA) {
        for (auto itB = it; itB != tickets.cend(); itB++) {
            const auto &name = itB->first.second;

            auto resultB = checkTicket(totalTime, itB, prev, minPrice);
            auto updateResultB = updateLoop(resultB, minPrice);

            if(updateResultB.first) {
                bestTickets = {nameA, name};
            }
            if(updateResultB.second) {
                break;
            }

            select3rdTicket(tickets, totalTime, bestTickets, minPrice, itB, resultB.second, nameA, name);
        }
    }


    SelectedTickets selectTickets(const TicketSortedMap &tickets, StopTime totalTime) {
        unsigned long long minPrice = ULLONG_MAX;
        SelectedTickets bestTickets;

        //tickets are sorted ascending by price
        for (auto it = tickets.cbegin(); it != tickets.cend(); it++) {
            const auto &name = it->first.second;

            auto result = checkTicket(totalTime, it, IteratingInfo(0, 0), minPrice);
            auto updateResult = updateLoop(result, minPrice);

            if(updateResult.first) {
                bestTickets = {name};
            }
            if(updateResult.second) {
                break;
            }
            
            select2ndTicket(tickets, totalTime, bestTickets, minPrice, it, result.second, name);
        }

        return bestTickets;
    }

    ProcessResult processError() {
        return ProcessResult(ERROR_RESP, std::nullopt);
    }

    ProcessResult processNoResponse() {
        return ProcessResult(NO_RESPONSE, std::nullopt);
    }

    bool isLineRepeated(const LineNum &lineNum, const Timetable &timetable) {
        return (timetable.find(lineNum) != timetable.end());
    }

    ProcessResult processAddRoute(const AddRoute &addRoute, Timetable &timetable) {
        if (isLineRepeated(addRoute.first, timetable)) {
            return processError();
        }

        timetable.insert(addRoute);

        return ProcessResult(NO_RESPONSE, std::nullopt);
    }

    bool isTicketNameRepeated(const std::string &ticketName, const TicketMap &ticketMap) {
        return (ticketMap.find(ticketName) != ticketMap.end());
    }

    void insertTicket(const AddTicket &addTicket, TicketMap &ticketMap, TicketSortedMap &ticketSortedMap) {
        const std::string &name = addTicket.first;
        Price price = addTicket.second.first;
        ValidTime validTime = addTicket.second.second;

        ticketMap.insert({name, price});
        ticketSortedMap.insert({{price, name}, validTime});
    }

    ProcessResult processAddTicket(const AddTicket &addTicket, TicketMap &ticketMap, TicketSortedMap &ticketSortedMap) {
        const std::string &ticketName = std::get<0>(addTicket);
        if (isTicketNameRepeated(ticketName, ticketMap)) {
            return processError();
        }

        insertTicket(addTicket, ticketMap, ticketSortedMap);

        return processNoResponse();
    }

    ProcessResult
    processQuery(const Query &query, Timetable &timetable, TicketSortedMap &ticketMap, uint &ticketCounter) {
        auto countingResult = countTime(query, timetable);

        switch (countingResult.first) {
            case COUNTING_FOUND: {
                auto tickets = selectTickets(ticketMap, std::get<StopTime>(countingResult.second.value_or(0)));

                if (!tickets.empty()) {
                    ticketCounter += tickets.size();
                    return ProcessResult(FOUND, tickets);
                } else {
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

    ProcessResult processRequest(const ParseResult &parseResult, TicketMap &ticketMap, TicketSortedMap &ticketSortedMap,
                                 Timetable &timetable, uint &ticketCounter) {
        switch (parseResult.first) {
            case ADD_ROUTE:
                return processAddRoute(std::get<AddRoute>(parseResult.second.value()), timetable);
            case ADD_TICKET:
                return processAddTicket(std::get<AddTicket>(parseResult.second.value()), ticketMap, ticketSortedMap);
            case QUERY:
                return processQuery(std::get<Query>(parseResult.second.value()), timetable, ticketSortedMap,
                                    ticketCounter);
            case IGNORE:
                return processNoResponse();
            default:
                break;
        }
        return processError();
    }

    void printFound(const std::vector<std::string> &tickets) {
        std::cout << "! ";
        bool first = true;
        for (auto &ticket: tickets) {
            if (!first)
                std::cout << "; ";
            else
                first = false;

            std::cout << ticket;
        }
        std::cout << std::endl;
    }

    void printWait(const std::string &stop) {
        std::cout << ":-( " << stop << std::endl;
    }

    void printNotFound() {
        std::cout << ":-|" << std::endl;
    }

    void printError(const std::string &inputLine, unsigned int lineCounter) {
        std::cerr << "Error in line " << lineCounter << ": " << inputLine << std::endl;
    }

    void printOutput(const ProcessResult &processResult, const std::string &inputLine, unsigned int lineCounter) {
        switch (processResult.first) {
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

    void printTicketCount(unsigned int ticketCounter) {
        std::cout << ticketCounter << std::endl;
    }

}

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    TicketMap ticketMap;
    TicketSortedMap ticketSortedMap;
    Timetable timetable;

    unsigned int ticketCounter = 0;
    unsigned int lineCounter = 1;
    std::string buffer;

    while (std::getline(std::cin, buffer)) {
        ParseResult parseResult = parseInputLine(buffer);
        ProcessResult processResult = processRequest(parseResult, ticketMap, ticketSortedMap, timetable, ticketCounter);
        printOutput(processResult, buffer, lineCounter);
        lineCounter++;
    }
    printTicketCount(ticketCounter);

    return 0;
}