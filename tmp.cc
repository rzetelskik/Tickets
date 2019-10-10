#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <variant>

using Ticket = std::tuple<std::string, unsigned long long, unsigned long long>;
using TicketSet = std::set<Ticket>;
using StopNameSet = std::unordered_set<std::string>;
using Stop = std::pair<const std::string&, uint>;
using Route = std::vector<Stop>;
using Timetable = std::unordered_map<uint, Route>;

using TourStop = std::pair<std::string, uint>;
using Tour = std::vector<TourStop>;

/*
 * uint -> for counted ride time
 * std::string -> stop name where passenger has to wait
 * bool -> false value when there is error in ride
 */
using FindingResult = std::variant<uint, std::string, bool>;

FindingResult countTime(const Tour& tour, const Timetable& timeTable) {
  uint arrivalTime = 0;
  uint startTime = 0;
  uint endTime = 0;

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
          }
          else if(arrivalTime != stop.second) {
            if(arrivalTime > stop.second) {
              return FindingResult(false);
            }
            else {
              return FindingResult(stop.first);
            }
          }

          arrivalTime = stop.second;
          started = true;
        }
      }
      else {
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
      FindingResult(false);
    }
  }

  return FindingResult(endTime - startTime);
}

int main() {
    //handleInput();

  //TEST

  StopNameSet names;
  Route r;

  std::string n1("name1");
  names.insert(n1);
  Stop s1 = Stop(n1, 10);

  std::string n2("name2");
  names.insert(n2);
  Stop s2 = Stop(n2, 20);

  std::string n3("name3");
  names.insert(n3);
  Stop s3 = Stop(n3, 30);

  std::string n4("name4");
  names.insert(n4);
  Stop s4 = Stop(n4, 40);

  std::string n5("name5");
  names.insert(n5);
  Stop s5 = Stop(n5, 50);

  std::string n6("name6");
  names.insert(n6);
  Stop s6 = Stop(n6, 60);

  r.push_back(s1);
  r.push_back(s2);
  r.push_back(s3);
  r.push_back(s4);
  r.push_back(s5);
  r.push_back(s6);




  std::string na1("line2_1");
  names.insert(na1);
  Stop ns1(na1, 15);

  Stop ns2(n4, 40);

  std::string na3("line2_3");
  names.insert(na3);
  Stop ns3(na3, 44);

  std::string na4("line2_4");
  names.insert(na4);
  Stop ns4(na4, 58);

  Stop ns5(n6, 70);

  Route r2;
  r2.push_back(ns1);
  r2.push_back(ns2);
  r2.push_back(ns3);
  r2.push_back(ns4);
  r2.push_back(ns5);

  Timetable tt;
  tt.insert({3, r});
  tt.insert({5, r2});


  TourStop ts1("name2", 3);
  TourStop ts2("name6", 0);
  Tour tour;

  tour.push_back(ts1);
  tour.push_back(ts2);


  TourStop nts1("name1", 3);
  TourStop nts2("name4", 5);
  TourStop nts3("line2_4", 0);

  Tour tur2;
  tur2.push_back(nts1);
  tur2.push_back(nts2);
  tur2.push_back(nts3);


  /*auto res = countTime(tour, tt);

  std::cout << res.index() << std::endl;

  if(res.index() == 0) {
    std::cout << std::get<uint>(res);
  }*/




  auto res2 = countTime(tur2, tt);

  std::cout << res2.index() << std::endl;

  if(res2.index() == 0) {
    std::cout << std::get<uint>(res2);
  }

  return 0;
}