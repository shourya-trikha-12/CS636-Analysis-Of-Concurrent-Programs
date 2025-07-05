#pragma once

#include <vector>
#include <iostream>
#include <stdexcept>

class VectorClock {
private:
    std::vector<int> v;

public:
    VectorClock();

    VectorClock(size_t maxThreads, int initValue);

    void join(const VectorClock& other);

    int& operator[](size_t i);

    bool operator<=(const VectorClock& other);

    void increment(int tid);

    size_t size();

    void print();
};


VectorClock::VectorClock() : v(0, 0) {
    std::cerr << "Error: Default Constructor for VectorClock called\n";
}

VectorClock::VectorClock(size_t maxThreads, int initValue) : v(maxThreads, initValue) {}

void VectorClock::join(const VectorClock& other) {
    // size_t sz = std::min(v.size(), other.v.size());
    
    for (size_t i = 0; i < v.size(); ++i) {
        v[i] = (other.v[i] > v[i] ? other.v[i] : v[i]);
    }
}

int& VectorClock::operator[](size_t i) {
    return v[i];
}

bool VectorClock::operator<=(const VectorClock& other) {
    size_t sz = std::min(v.size(), other.v.size());

    for (size_t i = 0; i < sz; ++i) {
        if (v[i] > other.v[i]) return false;
    }

    return true;
}

void VectorClock::increment(int tid) {
    ++v[tid];
}

size_t VectorClock::size() {
    return v.size();
}

void VectorClock::print() {
    for (auto &e: v) {
        std::cout << e << ", ";
    }
    std::cout << '\n';
}