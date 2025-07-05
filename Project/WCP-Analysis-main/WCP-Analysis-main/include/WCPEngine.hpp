#pragma once

#include <vector>
#include <stack>
#include <map>
#include <atomic>
#include "Utils.hpp"
#include "VectorClock.hpp"
#include "WCP.hpp"
#include "ShadowMemory.hpp"
#include "RaceStats.hpp"

class WCPEngine {
private:
    std::vector<LockSet> lockSets;

    std::vector<std::vector<Variable>> RVariables;
    std::vector<std::vector<Variable>> WVariables;

    std::vector<std::stack<size_t>> RVarStartIdx;
    std::vector<std::stack<size_t>> WVarStartIdx;

    WCP wcp;

    int maxThreads;
    ShadowMemory shadowMemory;

    std::vector<RaceStats> raceStats;

    std::vector<int> numChildren;
    std::vector<std::queue<tid_t>> joinableChildren;
    std::vector<std::atomic<bool>> joinableChildrenSyncFlags;


public:
    WCPEngine(int maxThreads);

    void acquire(tid_t t, Lock l, bool checkThreadJoin = true);

    void release(tid_t t, Lock l, bool checkThreadJoin = true);

    void read(tid_t t, Variable x, bool checkThreadJoin = true);

    void write(tid_t t, Variable x, bool checkThreadJoin = true);

    void thread_begin(tid_t t, tid_t parent);

    void thread_end(tid_t t, tid_t parent);

    void before_pthread_create(tid_t t);

    void print_stats(FILE* file);

    void check_and_do_thread_join(tid_t t);
};

WCPEngine::WCPEngine(int maxThreads)
    : lockSets(maxThreads), RVariables(maxThreads), WVariables(maxThreads),
      RVarStartIdx(maxThreads), WVarStartIdx(maxThreads), wcp(maxThreads),
      maxThreads(maxThreads), shadowMemory(maxThreads), raceStats(maxThreads),
      numChildren(maxThreads), joinableChildren(maxThreads), joinableChildrenSyncFlags(maxThreads)
{
    for (tid_t t = 0; t < maxThreads; ++t) {
        joinableChildrenSyncFlags[t].store(false);
    }
}

void WCPEngine::acquire(tid_t t, Lock l, bool checkThreadJoin) {
    if(checkThreadJoin) {
        check_and_do_thread_join(t);
    }

    l.addr = (l.addr << 2) >> 2; // 4-byte alignment

    auto& lockMd = shadowMemory.lock_and_get_lock_metadata(l);

    wcp.acquire(t, l, lockMd.H_l, lockMd.P_l, lockMd.Acq);

    shadowMemory.unlock_lock_metadata(l);

    lockSets[t].push_back(l);
    RVarStartIdx[t].push(RVariables[t].size());
    WVarStartIdx[t].push(WVariables[t].size());
}

void WCPEngine::release(tid_t t, Lock l, bool checkThreadJoin) {
    if(checkThreadJoin) {
        check_and_do_thread_join(t);
    }

    l.addr = (l.addr << 2) >> 2; // 4-byte alignment

    ReadSet R(RVariables[t].begin() + RVarStartIdx[t].top(), RVariables[t].end());
    WriteSet W(WVariables[t].begin() + WVarStartIdx[t].top(), WVariables[t].end());
    
    auto& lockMd = shadowMemory.lock_and_get_lock_metadata(l);

    wcp.release(t, l, lockMd.H_l, lockMd.P_l, lockMd.Acq,
                lockMd.Rel, lockMd.L_r, lockMd.L_w, R, W);

    shadowMemory.unlock_lock_metadata(l);

    if (lockSets[t].size() == 0) {
        std::cout << "ERROR: Release of Lock " << (void*)l.addr << " without acquiring it first"  << std::endl;
        exit(EXIT_FAILURE);
    }

    if (lockSets[t].back() != l) {
        std::cout << "ERROR: Lock acquires and releases not well nested. Lock " << (void*)l.addr << " acquired later than " << (void*)lockSets[t].back().addr << " but released earlier" << std::endl;
        exit(EXIT_FAILURE);
    }
    
    lockSets[t].pop_back();
    RVarStartIdx[t].pop();
    WVarStartIdx[t].pop();
    if(lockSets[t].size() == 0) {
        RVariables[t].resize(0);
        WVariables[t].resize(0);
    }
}

void WCPEngine::read(tid_t t, Variable x, bool checkThreadJoin) {
    if(checkThreadJoin) {
        check_and_do_thread_join(t);
    }

    x.addr = (x.addr << 2) >> 2; // 4-byte alignment

    wcp.read(t, x, lockSets[t], shadowMemory);
    
    auto C_t = wcp.get_thread_vc(t);
    
    auto& VarMd = shadowMemory.lock_and_get_variable_metadata(x);
    auto& RClock = VarMd.R;
    auto& WClock = VarMd.W;
    
    RClock[t] = C_t[t];

    for (int i = 0; i < maxThreads; ++i) {
        if (i != t && WClock[i] > C_t[i]) {
            raceStats[t].add_race(t, i, x.addr, RaceStats::RaceType::WR);
        }
    }

    shadowMemory.unlock_variable_metadata(x);

    if(lockSets[t].size() > 0) {
        RVariables[t].push_back(x);
    }
}

void WCPEngine::write(tid_t t, Variable x, bool checkThreadJoin) {
    if(checkThreadJoin) {
        check_and_do_thread_join(t);
    }

    x.addr = (x.addr << 2) >> 2; // 4-byte alignment

    wcp.write(t, x, lockSets[t], shadowMemory);
    
    auto C_t = wcp.get_thread_vc(t);
    
    auto& VarMd = shadowMemory.lock_and_get_variable_metadata(x);
    auto& RClock = VarMd.R;
    auto& WClock = VarMd.W;
    
    WClock[t] = C_t[t];

    for (int i = 0; i < maxThreads; ++i) {
        if (i != t && WClock[i] > C_t[i]) {
            raceStats[t].add_race(t, i, x.addr, RaceStats::RaceType::WW);
        }
    }

    for (int i = 0; i < maxThreads; ++i) {
        if (i != t && RClock[i] > C_t[i]) {
            raceStats[t].add_race(t, i, x.addr, RaceStats::RaceType::RW);
        }
    }

    shadowMemory.unlock_variable_metadata(x);
    
    if(lockSets[t].size() > 0) {
        WVariables[t].push_back(x);
    }
}

void WCPEngine::thread_begin(tid_t t, tid_t parent) {
    Lock pseudoLock{.addr = ((addr_t)parent * maxThreads + t) * 8};
    Variable pseudoVar{.addr = ((addr_t)parent * maxThreads + t) * 8 + 4};

    acquire(parent, pseudoLock, false);
    write(parent, pseudoVar, false);
    release(parent, pseudoLock, false);

    acquire(t, pseudoLock, false);
    read(t, pseudoVar, false);
    release(t, pseudoLock, false);
}

void WCPEngine::thread_end(tid_t t, tid_t parent) {
    bool expected;
    do {
        expected = false;
    } while (!joinableChildrenSyncFlags[parent].compare_exchange_weak(expected, true));

    joinableChildren[parent].push(t);

    joinableChildrenSyncFlags[parent].store(false);
}

void WCPEngine::before_pthread_create(tid_t t) {
    ++numChildren[t];
}

void WCPEngine::print_stats(FILE* file) {
    for (auto &threadRaceStats: raceStats) {
        threadRaceStats.print_race_stats(file);
    }
}

void WCPEngine::check_and_do_thread_join(tid_t t) {
    if (numChildren[t] == 0) return;

    bool expected;
    do {
        expected = false;
    } while (!joinableChildrenSyncFlags[t].compare_exchange_weak(expected, true));
    
    while (joinableChildren[t].size()) {
        auto child = joinableChildren[t].front();
        joinableChildren[t].pop();
        --numChildren[t];
        
        Lock pseudoLock{.addr = ((addr_t)child * maxThreads + t) * 8};
        Variable pseudoVar{.addr = ((addr_t)child * maxThreads + t) * 8 + 4};
        
        acquire(child, pseudoLock, false);
        write(child, pseudoVar, false);
        release(child, pseudoLock, false);

        acquire(t, pseudoLock, false);
        read(t, pseudoVar, false);
        release(t, pseudoLock, false);
    }
    
    joinableChildrenSyncFlags[t].store(false);
}