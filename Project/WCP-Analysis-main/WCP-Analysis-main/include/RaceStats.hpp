#pragma once

#include <iostream>
#include <cstdint>
#include <map>

class RaceStats {
private:
    std::map<std::tuple<int, int, uint64_t>, int> WW_Races;
    std::map<std::tuple<int, int, uint64_t>, int> WR_Races;
    std::map<std::tuple<int, int, uint64_t>, int> RW_Races;

public:
    enum RaceType {
        WW,
        WR,
        RW,
    };

    void add_race(int tid1, int tid2, uint64_t addr, RaceType raceType);

    void print_race_stats(FILE* file);
};

void RaceStats::add_race(int tid1, int tid2, uint64_t addr, RaceType raceType) {
    switch(raceType) {
        case RaceType::WW:
            ++WW_Races[{tid1, tid2, addr}];
            break;
        case RaceType::WR:
            ++WR_Races[{tid1, tid2, addr}];
            break;
        case RaceType::RW:
            ++RW_Races[{tid1, tid2, addr}];
            break;
        default:
            break;
    }
}

void RaceStats::print_race_stats(FILE* file) {
    for (auto& [race, count] : WW_Races) {
        auto& [tid1, tid2, addr] = race;
        fprintf(file, "%p W-W TID: %d TID: %d Count: %d\n", (void*)addr, tid1, tid2, count);
    }
    
    for (auto& [race, count] : WR_Races) {
        auto& [tid1, tid2, addr] = race;
        fprintf(file, "%p W-R TID: %d TID: %d Count: %d\n", (void*)addr, tid1, tid2, count);
    }
    
    for (auto& [race, count] : RW_Races) {
        auto& [tid1, tid2, addr] = race;
        fprintf(file, "%p R-W TID: %d TID: %d Count: %d\n", (void*)addr, tid1, tid2, count);
    }
}