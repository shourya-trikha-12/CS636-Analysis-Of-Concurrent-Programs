#pragma once

#include <cstdint>
#include <set>
#include <span>
#include <vector>
#include <queue>
#include <map>
#include "VectorClock.hpp"

using tid_t = int;
using timestamp_t = int;
using addr_t = uint64_t;

#define GRANUALARITY 4

struct Lock {
    addr_t addr;

    bool operator== (const Lock& other) const;

    bool operator!= (const Lock& other) const;
};

struct Variable {
    addr_t addr;

    bool operator< (const Variable& other) const;

    bool operator== (const Variable& other) const;
};

struct mem_region {
    addr_t addr;
    int size;

    bool operator< (const mem_region& other) const;

    bool operator== (const mem_region& other) const;
};

// using Variable = mem_region;
using L_Type = std::map<Variable, VectorClock>;

using ReadSet = std::vector<Variable>;
using WriteSet = std::vector<Variable>;

using LockSet = std::vector<Lock>;

using Queue_Type = std::vector<std::queue<VectorClock>>;

struct LockMetadata {
    VectorClock H_l;
    VectorClock P_l;
    Queue_Type Acq;
    Queue_Type Rel;
    L_Type L_r;
    L_Type L_w;
    Lock l;

    LockMetadata (size_t maxThreads, Lock l);
};

struct VariableMetadata {
    VectorClock R;
    VectorClock W;
    Variable x;

    VariableMetadata (size_t maxThreads, Variable x);
};

bool Lock::operator== (const Lock& other) const {
    return addr == other.addr;
}

bool Lock::operator!= (const Lock& other) const {
    return !(*this == other);
}

bool Variable::operator< (const Variable& other) const {
    return addr < other.addr;
}

bool Variable::operator== (const Variable& other) const {
    return addr == other.addr;
}


bool mem_region::operator< (const mem_region& other) const {
    return addr < other.addr || (addr == other.addr && size < other.size);
}

bool mem_region::operator== (const mem_region& other) const {
    return addr == other.addr && size == other.size;
}

LockMetadata::LockMetadata (size_t maxThreads, Lock l)
    : H_l(maxThreads, 0), P_l(maxThreads, 0), Acq(maxThreads), Rel(maxThreads), l(l) {}

VariableMetadata::VariableMetadata (size_t maxThreads, Variable x)
    : R(maxThreads, 0), W(maxThreads, 0), x(x) {}