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
    // Ticket's price.
    using Price = unsigned long;
    // Ticket's validity time.
    using ValidTime = unsigned long long;
    // Map of tickets allowing for accessing its price by its name.
    using TicketMap = std::unordered_map<std::string, Price>;
    // Map of tickets sorted by price (ascending order).
    using TicketSortedMap = std::map<std::pair<Price, std::string>, ValidTime>;
    // Tram's arrival time at a stop.
    using StopTime = unsigned long;
    // Map of stops in a given route.
    using Route = std::unordered_map<std::string, StopTime>;
    // Line number (id).
    using LineNum = unsigned long long;
    // Map of routes.
    using Timetable = std::unordered_map<LineNum, Route>;
    // A request to add a route to the timetable.
    using AddRoute = std::pair<LineNum, Route>;
    // A request to add a ticket to the collection.
    using AddTicket = std::pair<std::string, std::pair<Price, ValidTime>>;
    // A stop requested by passenger on a given line.
    using QueryStop = std::pair<std::string, LineNum>;
    // A vector of all stops requested by passenger.
    using Query = std::vector<QueryStop>;
    // Variant allowing for keeping every valid request.
    using Request = std::variant<AddRoute, AddTicket, Query>;
    // Type of request (both valid or invalid).
    enum RequestType {
        ADD_ROUTE,
        ADD_TICKET,
        QUERY,
        IGNORE,
        ERROR_REQ
    };
    // A parsing result - type of request and the request itself (if valid).
    using ParseResult = std::pair<RequestType, std::optional<Request>>;
    // Type of travel time counting outcome (both valid or invalid).
    enum TravelTimeResultType {
        TRAVEL_TIME_FOUND,
        TRAVEL_TIME_WAIT,
        TRAVEL_TIME_ERR
    };
    // Variant allowing for keeping additional info for a response to query.
    using TravelTimeInfo = std::variant<StopTime, std::string>;
    // A travel time counting result - type of travel time counting result and additional info (if valid result).
    using TravelTimeCountingResult = std::pair<TravelTimeResultType, std::optional<TravelTimeInfo>>;
    // A section's validity check result - type of travel time counting result and start time/stop time (if valid).
    using SectionCheckResult = std::pair<TravelTimeResultType, std::optional<std::pair<StopTime, StopTime>>>;
    // A vector of tickets selected for a journey.
    using SelectedTickets = std::vector<std::string>;
    // Ticket iterator, used for iterating over a ticket map.
    using TicketIterator = std::_Rb_tree_const_iterator<std::pair<const std::pair<Price, std::string>, unsigned long long>>;
    // Up to date information (collective price, collective time) in a given iteration.
    using TicketIteratingInfo = std::pair<unsigned long long, ValidTime>;
    // Status of a given set of tickets in regard to the current best set.
    enum TicketStatus {
        NEW_BEST,
        TOO_EXPENSIVE,
        TOO_SHORT
    };
    // A ticket selection result - ticket set's status and the information about it.
    using TicketSelectionResult = std::pair<TicketStatus, TicketIteratingInfo>;
    // Type of a response to a request (both valid or invalid).
    enum ResponseType {
        FOUND,
        WAIT,
        NOT_FOUND,
        NO_RESPONSE,
        ERROR_RESP
    };
    // Variant allowing for containing a response to a valid request.
    using Response = std::variant<std::string, std::vector<std::string>>;
    // A processing result - type of response and a response itself (if request was valid).
    using ProcessResult = std::pair<ResponseType, std::optional<Response>>;

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
            return ERROR_REQ;
        }
    }

    inline ParseResult parseIgnore() {
        return ParseResult(IGNORE, std::nullopt);
    }

    inline ParseResult parseError() {
        return ParseResult(ERROR_REQ, std::nullopt);
    }

    inline bool isStopTimeCorrect(StopTime stopTime, StopTime prevStopTime) {
        static const int minutesLowerBound = 355, minutesUpperBound = 1281;
        return (stopTime > prevStopTime && stopTime >= minutesLowerBound && stopTime <= minutesUpperBound);
    }

    inline bool isStopRepeated(const std::string& stopName, const Route& route) {
        return (route.find(stopName) != route.end());
    }

    inline std::optional<StopTime> getRouteStopTime(const std::string& strHour, const std::string& strMinute) {
        try {
            return 60 * stoul(strHour) + stoul(strMinute);
        } catch (std::exception& e) {
            return std::nullopt;
        }
    }

    std::optional<Route> parseRouteStops(const std::string& str, const std::string& rgxHour,
            const std::string& rgxMinute, const std::string& rgxStopName) {
        static const size_t rgxHourGroupPos = 1, rgxMinuteGroupPos = 2, rgxStopNameGroupPos = 3;
        Route route;
        StopTime prevStopTime = 0;

        std::regex rgx(" (" + rgxHour + "):(" + rgxMinute + ") (" + rgxStopName + ")");
        for (auto it = std::sregex_iterator(str.begin(), str.end(), rgx); it != std::sregex_iterator(); ++ it) {
            std::smatch match = *it;

            const std::string& stopName = match.str(rgxStopNameGroupPos);
            if (isStopRepeated(stopName, route)) {
                return std::nullopt;
            }

            auto stopTime = getRouteStopTime(match.str(rgxHourGroupPos), match.str(rgxMinuteGroupPos));
            if (! stopTime.has_value() || ! isStopTimeCorrect(stopTime.value(), prevStopTime)) {
                return std::nullopt;
            }

            route.insert({stopName, stopTime.value()});
            prevStopTime = stopTime.value();
        }

        return route;
    }

    ParseResult parseAddRoute(const std::string& line) {
        static const std::string rgxLineNum = R"(\d+)", rgxHour = R"([5-9]|1\d|2[0-1])", rgxMinute = R"([0-5]\d)",
            rgxStopName = R"([a-zA-Z^_]+)";
        static const size_t rgxLineGroupPos = 1, rgxIterGroupPos = 2;

        std::regex rgx("^(" + rgxLineNum + ")((?: (?:" + rgxHour + "):" + rgxMinute + " " + rgxStopName + ")+)$");
        std::smatch match;

        if (! std::regex_match(line, match, rgx)) {
            return parseError();
        }

        LineNum lineNum = stoull(match.str(rgxLineGroupPos));

        auto route = parseRouteStops(match.str(rgxIterGroupPos), rgxHour, rgxMinute, rgxStopName);
        if (! route.has_value()) {
            return parseError();
        }

        return ParseResult(ADD_ROUTE, AddRoute(lineNum, route.value()));
    }

    inline bool isPriceCorrect(Price price) {
        return (price > 0.00);
    }

    inline std::optional<Price> getFullPrice(const std::string& strIntegerPart, const std::string& strDecimalPart) {
        try {
            return 100 * std::stoul(strIntegerPart) + std::stoul(strDecimalPart);
        } catch (std::exception& e) {
            return std::nullopt;
        }
    }

    inline std::optional<ValidTime> getTicketTime(const std::string& strTime) {
        try {
            return std::stoull(strTime);
        } catch (std::exception& e) {
            return std::nullopt;
        }
    }

    ParseResult parseAddTicket(const std::string& line) {
        static const std::string rgxTicketName = R"(([a-zA-Z ]+))", rgxTicketPrice = R"((\d+)\.(\d{2}))",
            rgxTicketTime = R"(([1-9]\d*))";
        static const size_t rgxTicketNamePos = 1, rgxTicketPriceIntegerPos = 2, rgxTicketPriceDecimalPos = 3,
            rgxTicketTimePos = 4;

        std::regex rgx("^" + rgxTicketName + " " + rgxTicketPrice + " " + rgxTicketTime + "$");
        std::smatch match;

        if (! std::regex_match(line, match, rgx)) {
            return parseError();
        }

        std::string name = match.str(rgxTicketNamePos);

        auto price = getFullPrice(match.str(rgxTicketPriceIntegerPos), match.str(rgxTicketPriceDecimalPos));
        if (! price.has_value() || ! isPriceCorrect(price.value())) {
            return parseError();
        }

        auto validTime = getTicketTime(match.str(rgxTicketTimePos));
        if (! validTime.has_value()) {
            return parseError();
        }

        AddTicket addTicket = {name, {price.value(), validTime.value()}};
        return ParseResult(ADD_TICKET, Request(addTicket));
    }

    inline std::optional<StopTime> getQueryStopTime(const std::string& strStopTime) {
        try {
            return std::stoul(strStopTime);
        } catch (std::exception& e) {
            return std::nullopt;
        }
    }

    std::optional<Query> parseQueryStops(const std::string& str, const std::string& rgxStopName,
            const std::string& rgxStopTime) {
        static const size_t rgxStopNamePos = 1, rgxStopTimePos = 2;
        Query query;

        std::regex rgx(" (" + rgxStopName + ") (" + rgxStopTime + ")");
        for (auto it = std::sregex_iterator(str.begin(), str.end(), rgx);
             it != std::sregex_iterator(); ++ it) {
            std::smatch match = *it;

            std::string name = match.str(rgxStopNamePos);

            auto stopTime = getQueryStopTime(match.str(rgxStopTimePos));
            if (! stopTime.has_value()) {
                return std::nullopt;
            }

            QueryStop queryStop = {name, stopTime.value()};
            query.push_back(queryStop);
        }

        return query;
    }

    ParseResult parseQuery(const std::string& line) {
        static const std::string rgxQueryChar = R"(\?)", rgxStopName = R"([a-zA-Z^_]+)", rgxStopTime = R"(\d+)";
        static const size_t rgxIterStrPos = 1;

        std::regex rgx("^" + rgxQueryChar + "((?: " + rgxStopName + " " + rgxStopTime + ")+) (" + rgxStopName + ")$");
        std::smatch match;

        if (! std::regex_match(line, match, rgx)) {
            return parseError();
        }

        auto query = parseQueryStops(match.str(rgxIterStrPos), rgxStopName, rgxStopTime);
        if (! query.has_value()) {
            return parseError();
        }

        size_t rgxLasStopNamePos = match.size() - 1;
        QueryStop queryStop = {match.str(rgxLasStopNamePos), 0};
        query.value().push_back(queryStop);

        return ParseResult(QUERY, query.value());
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
            default:
                break;
        }

        return parseError();
    }

    SectionCheckResult checkSection(const std::string& from, const std::string& to, const Route& line,
            StopTime& arrivalTime) {
        auto startStop = line.find(from);
        auto endStop = line.find(to);

        if (startStop == line.end() || endStop == line.end()) {
            return SectionCheckResult(TRAVEL_TIME_ERR, std::nullopt);
        } else if (startStop->second > endStop->second) {
            return SectionCheckResult(TRAVEL_TIME_ERR, std::nullopt);
        } else if (arrivalTime != 0 && arrivalTime != startStop->second) {
            return (startStop->second > arrivalTime) ? SectionCheckResult(TRAVEL_TIME_WAIT, std::nullopt)
                : SectionCheckResult(TRAVEL_TIME_ERR, std::nullopt);
        } else {
            arrivalTime = endStop->second;
            return SectionCheckResult(TRAVEL_TIME_FOUND, std::pair(startStop->second, endStop->second));
        }
    }

    TravelTimeCountingResult countTravelTime(const Query& tour, const Timetable& timetable) {
        StopTime arrivalTime = 0;
        StopTime startTime = 0;
        StopTime endTime = 0;

        for (std::size_t i = 0; i < tour.size() - 1; i ++) {
            auto& current = tour[i];
            auto& next = tour[i + 1];

            const auto& currentName = current.first;
            const auto& nextName = next.first;

            const auto& line = timetable.find(current.second);
            if(line == timetable.end()) {
                return TravelTimeCountingResult(TRAVEL_TIME_ERR, std::nullopt);
            }

            auto result = checkSection(currentName, nextName, line->second, arrivalTime);
            switch (result.first) {
                case TRAVEL_TIME_WAIT:
                    return TravelTimeCountingResult(TRAVEL_TIME_WAIT, currentName);
                case TRAVEL_TIME_FOUND:
                    if (i == 0)
                        startTime = result.second->first;
                    if (i == tour.size() - 2)
                        endTime = result.second->second;
                    break;
                case TRAVEL_TIME_ERR:
                    return TravelTimeCountingResult(TRAVEL_TIME_ERR, std::nullopt);
            }
        }

        return TravelTimeCountingResult(TRAVEL_TIME_FOUND, TravelTimeInfo(endTime - startTime));
    }

    TicketSelectionResult compareTicket(StopTime totalTime, const TicketIterator& it, TicketIteratingInfo info,
                                        unsigned long long minPrice) {
        const auto& key = it->first;
        auto price = key.first;
        auto time = it->second;

        unsigned long long currentPrice = info.first + price;
        ValidTime currentTime = info.second + time;

        if (currentTime > totalTime) {
            if (currentPrice <= minPrice) {
                return TicketSelectionResult(NEW_BEST, TicketIteratingInfo(currentPrice, currentTime));
            }
            return TicketSelectionResult(TOO_EXPENSIVE, TicketIteratingInfo(currentPrice, currentTime));
        }
        return TicketSelectionResult(TOO_SHORT, TicketIteratingInfo(currentPrice, currentTime));
    }

    std::pair<bool, bool> updateTicketSelectionLoop(TicketSelectionResult& result) {
        bool breakLoop = false;
        bool updateBestTickets = false;

        switch (result.first) {
            case NEW_BEST:
                updateBestTickets = true;
                breakLoop = true;
                break;
            case TOO_EXPENSIVE:
                breakLoop = true;
                break;
            case TOO_SHORT:
                break;
        }

        return {updateBestTickets, breakLoop};
    }

    void select3rdTicket(const TicketSortedMap& tickets, StopTime totalTime, SelectedTickets& bestTickets,
            unsigned long long& minPrice, const TicketIterator& it, const TicketIteratingInfo& prev,
            const std::string& nameA, const std::string& nameB) {
        for (auto itC = it; itC != tickets.cend(); itC ++) {
            const auto& name = itC->first.second;

            auto result = compareTicket(totalTime, itC, prev, minPrice);
            auto updateResult = updateTicketSelectionLoop(result);

            if(updateResult.first) {
                minPrice = result.second.first;
                bestTickets = {nameA, nameB, name};
            }
            if (updateResult.second) {
                break;
            }
        }
    }

    void select2ndTicket(const TicketSortedMap& tickets, StopTime totalTime, SelectedTickets& bestTickets,
            unsigned long long& minPrice, const TicketIterator& it, const TicketIteratingInfo& prev,
            const std::string& nameA) {
        for (auto itB = it; itB != tickets.cend(); itB++) {
            const auto &name = itB->first.second;

            auto result = compareTicket(totalTime, itB, prev, minPrice);
            auto updateResult = updateTicketSelectionLoop(result);

            if(updateResult.first) {
                minPrice = result.second.first;
                bestTickets = {nameA, name};
            }
            if(updateResult.second) {
                break;
            }

            select3rdTicket(tickets, totalTime, bestTickets, minPrice, itB, result.second, nameA, name);
        }
    }


    SelectedTickets selectTickets(const TicketSortedMap& tickets, StopTime totalTime) {
        unsigned long long minPrice = ULLONG_MAX;
        SelectedTickets bestTickets;

        for (auto it = tickets.cbegin(); it != tickets.cend(); it ++) {
            const auto& name = it->first.second;

            auto result = compareTicket(totalTime, it, TicketIteratingInfo(0, 0), minPrice);
            auto updateResult = updateTicketSelectionLoop(result);

            if(updateResult.first) {
                minPrice = result.second.first;
                bestTickets = {name};
            }
            if (updateResult.second) {
                break;
            }

            select2ndTicket(tickets, totalTime, bestTickets, minPrice, it, result.second, name);
        }

        return bestTickets;
    }

    inline ProcessResult processError() {
        return ProcessResult(ERROR_RESP, std::nullopt);
    }

    inline ProcessResult processNoResponse() {
        return ProcessResult(NO_RESPONSE, std::nullopt);
    }

    inline bool isLineRepeated(const LineNum& lineNum, const Timetable& timetable) {
        return (timetable.find(lineNum) != timetable.end());
    }

    ProcessResult processAddRoute(const AddRoute& addRoute, Timetable& timetable) {
        if (isLineRepeated(addRoute.first, timetable)) {
            return processError();
        }

        timetable.insert(addRoute);

        return ProcessResult(NO_RESPONSE, std::nullopt);
    }

    inline bool isTicketNameRepeated(const std::string& ticketName, const TicketMap& ticketMap) {
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

    ProcessResult processCountingFound(SelectedTickets& tickets, unsigned int& ticketCounter) {
        if (!tickets.empty()) {
            ticketCounter += tickets.size();
            return ProcessResult(FOUND, tickets);
        } else {
            return ProcessResult(NOT_FOUND, std::nullopt);
        }
    }

    ProcessResult processQuery(const Query& query, Timetable& timetable,TicketSortedMap& ticketMap,
            unsigned int& ticketCounter) {
        auto countingResult = countTravelTime(query, timetable);

        switch (countingResult.first) {
            case TRAVEL_TIME_FOUND: {
                SelectedTickets tickets = selectTickets(ticketMap,
                        std::get<StopTime>(countingResult.second.value_or(0)));
                return processCountingFound(tickets, ticketCounter);
            }
            case TRAVEL_TIME_WAIT:
                return ProcessResult(WAIT, Response(std::get<std::string>(countingResult.second.value())));
            case TRAVEL_TIME_ERR:
                return ProcessResult(ERROR_RESP, std::nullopt);
        }

        return ProcessResult(NOT_FOUND, std::nullopt);
    }

    ProcessResult processRequest(const ParseResult& parseResult, TicketMap& ticketMap, TicketSortedMap& ticketSortedMap,
            Timetable& timetable, uint& ticketCounter) {
        switch (parseResult.first) {
            case ADD_ROUTE:
                return processAddRoute(std::get<AddRoute>(parseResult.second.value()), timetable);
            case ADD_TICKET:
                return processAddTicket(std::get<AddTicket>(parseResult.second.value()), ticketMap, ticketSortedMap);
            case QUERY:
                return processQuery(std::get<Query>(parseResult.second.value()), timetable,
                        ticketSortedMap, ticketCounter);
            case IGNORE:
                return processNoResponse();
            default:
                break;
        }
        return processError();
    }

    void printFound(const std::vector<std::string>& tickets) {
        bool first = true;

        std::cout << "! ";
        for (auto it = tickets.crbegin(); it != tickets.crend(); it++) {
            if (! first) {
                std::cout << "; ";
            } else {
                first = false;
            }

            std::cout << *it;
        }
        std::cout << std::endl;
    }

    void printWait(const std::string& stop) {
        std::cout << ":-( " << stop << std::endl;
    }

    void printNotFound() {
        std::cout << ":-|" << std::endl;
    }

    void printError(const std::string& inputLine, unsigned int lineCounter) {
        std::cerr << "Error in line " << lineCounter << ": " << inputLine << std::endl;
    }

    void printOutput(const ProcessResult& processResult, const std::string& inputLine, unsigned int lineCounter) {
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
        lineCounter ++;
    }
    printTicketCount(ticketCounter);

    return 0;
}
