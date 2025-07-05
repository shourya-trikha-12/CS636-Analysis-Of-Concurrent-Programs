#pragma once

#include <vector>
#include <set>
#include <map>
#include <queue>
#include <mutex>
#include "VectorClock.hpp"
#include "Utils.hpp"
#include "ShadowMemory.hpp"

class WCP {
private:
    std::vector<VectorClock> P_t;
    std::vector<VectorClock> H_t;
    std::vector<int> N;

    std::vector<bool> isPrevEventRel;
    int maxThreads;
    size_t currThreadCount;

public:
    WCP (int maxThreads); 

    /**
     * @brief handles the lock acquire event
     * @param t the threadID of the acquiring thread
     * @param l the lock which is acquired
     * @param H_l the H_l clock of the thread
     * @param P_l the P_l clock of the thread
     * @param Acq the Acq queue corresponding to the lock l
     */
    void acquire (tid_t t, Lock l, const VectorClock& H_l,
                  const VectorClock& P_l, Queue_Type& Acq);
    
    /**
     * @brief handles the lock release event
     * @param t the threadID of the releasing thread
     * @param l the lock which is released
     * @param H_l the H_l clock of the thread
     * @param P_l the P_l clock of the thread
     * @param Acq the Acq queue corresponding to the lock l
     * @param Rel the Rel queue corresponding to the lock l
     * @param L_r the L_r map corresponding to the lock l
     * @param L_w the L_w map corresponding to the lock l
     * @param R the set of variables read inside the critical section corresponding to the release
     * @param W the set of variables written inside the critical section corresponding to the release
     */
    void release (tid_t t, Lock l, VectorClock& H_l, VectorClock& P_l,
                  Queue_Type& Acq, Queue_Type& Rel, L_Type& L_r, L_Type& L_w,
                  const ReadSet& R, const WriteSet& W);

    /**
     * @brief handles the read event
     * @param t the threadID of the reader thread
     * @param x the variable that is read
     * @param L the set of locks corresponding to the enclosing critical sections
     * @param shadowMemory the shadow memory object that maintains the metadata
     */
    void read (tid_t t, Variable x, const LockSet& L, ShadowMemory& shadowMemory);
    
    /**
     * @brief handles the write event
     * @param t the threadID of the writer thread
     * @param x the variable that is written
     * @param L the set of locks corresponding to the enclosing critical sections
     * @param shadowMemory the shadow memory object that maintains the metadata
     */
    void write (tid_t t, Variable x, const LockSet& L, ShadowMemory& shadowMemory);

    /**
     * @brief returns the vector clock C_t for a thread; caller should only call this for its own thread ID
     * @param tid the threadID whose vector clock is returned
     */
    VectorClock get_thread_vc(tid_t t);
};

WCP::WCP (int maxThreads)
    : P_t(maxThreads, VectorClock(maxThreads, 0)), H_t(maxThreads, VectorClock(maxThreads, 0)),
      N(maxThreads, 1), isPrevEventRel(maxThreads, false),
      maxThreads(maxThreads), currThreadCount(1) //TODO: currThreadCount?
{
    for (tid_t i = 0; i < maxThreads; ++i) {
        H_t[i].increment(i);
    }
}

void WCP::acquire (tid_t t, Lock l, const VectorClock& H_l,
                   const VectorClock& P_l, Queue_Type& Acq) {
    if (isPrevEventRel[t]) {
        ++N[t];
        H_t[t].increment(t);
    }

    // TODO: Need to make access to Acq thread-safe
    // if (!Acq.count(l)) {
    //     Acq.emplace(l, std::vector<std::queue<VectorClock>>(maxThreads));
    //     H_l.emplace(l, VectorClock(maxThreads, 0));
    //     P_l.emplace(l, VectorClock(maxThreads, 0));
    // }

    H_t[t].join(H_l);
    P_t[t].join(P_l);
    
    auto C_t = get_thread_vc(t);

    for (tid_t i = 0; i < maxThreads; ++i) {
        if (i != t) Acq[i].push(C_t);
    }

    isPrevEventRel[t] = false;
}

void WCP::release (tid_t t, Lock l, VectorClock& H_l, VectorClock& P_l,
                   Queue_Type& Acq, Queue_Type& Rel, L_Type& L_r, L_Type& L_w,
                   const ReadSet& R, const WriteSet& W) {
    if (isPrevEventRel[t]) {
        ++N[t];
        H_t[t].increment(t);
    }
    
    auto C_t = get_thread_vc(t);

    while (Acq[t].size() > 0 && Acq[t].front() <= C_t) {
        Acq[t].pop();
        P_t[t].join(Rel[t].front());
        Rel[t].pop();
    }

    for (auto& x : R) {
        if (!L_r.count(x)) {
            L_r.emplace(x, H_t[t]);
        }
        else{
            L_r[x].join(H_t[t]);
        }
    }

    for (auto& x : W) {
        if (!L_w.count(x)) {
            L_w.emplace(x, H_t[t]);
        }
        else{
            L_w[x].join(H_t[t]);
        }
    }

    H_l = H_t[t];
    P_l = P_t[t];

    for (tid_t i = 0; i < maxThreads; ++i) {
        if (i != t) Rel[i].push(H_t[t]);
    }

    isPrevEventRel[t] = true;
}

void WCP::read (tid_t t, Variable x, const LockSet& L, ShadowMemory& shadowMemory) {
    if (isPrevEventRel[t]) {
        ++N[t];
        H_t[t].increment(t);
    }

    for (auto& lock : L) {
        auto& lockMd = shadowMemory.lock_and_get_lock_metadata(lock);
        auto& L_w = lockMd.L_w;

        if(L_w.count(x)) {
            P_t[t].join(L_w.find(x)->second);
        }

        shadowMemory.unlock_lock_metadata(lock);
    }

    isPrevEventRel[t] = false;
}

void WCP::write (tid_t t, Variable x, const LockSet& L, ShadowMemory& shadowMemory) {
    if (isPrevEventRel[t]) {
        ++N[t];
        H_t[t].increment(t);
    }
    
    for (auto& lock: L) {
        auto& lockMd = shadowMemory.lock_and_get_lock_metadata(lock);
        auto& L_r = lockMd.L_r;
        auto& L_w = lockMd.L_w;

        if(L_r.count(x)){
            P_t[t].join(L_r.find(x)->second);
        }
        if(L_w.count(x)){
            P_t[t].join(L_w.find(x)->second);
        }

        shadowMemory.unlock_lock_metadata(lock);
    }

    isPrevEventRel[t] = false;
}

VectorClock WCP::get_thread_vc(tid_t t) {
    auto C_t = P_t[t];
    C_t[t] = N[t];
    
    return C_t;
}
