#include <stdio.h>
#include <set>
#include <map>
#include <mutex>
#include <atomic>
#include <pthread.h>
#include <string>
#include "pin.H"
#include "include/WCPEngine.hpp"

// #define LOG_TRACE

#ifdef LOG_TRACE
    #define LOG_INFO(fmt, ...) fprintf(globals->trace, fmt, ##__VA_ARGS__)
#else
    #define LOG_INFO(fmt, ...) do {} while (0)
#endif

struct GlobalData {
    FILE* trace;
    
    WCPEngine wcpEngine;
    
    std::map<OS_THREAD_ID, THREADID> osToPinTid;
    std::map<pthread_t, THREADID> pthreadToPinTid;
    PIN_MUTEX lock;
    std::vector<std::atomic<int>> parChildSyncFlags;
    std::vector<THREADID> currChildPinTid;
    
    ADDRINT mainImgLow = 0;
    ADDRINT mainImgHigh = 0;

    ADDRINT pthreadJoinAddr;

    GlobalData(int maxThreads, const char* traceOutputFilePath) :
        wcpEngine(maxThreads), parChildSyncFlags(maxThreads), currChildPinTid(maxThreads)
    {
        #ifdef LOG_TRACE
            trace = fopen(traceOutputFilePath, "w");
        #endif

        PIN_MutexInit(&lock);

        for (int i = 0; i < maxThreads; ++i) {
            parChildSyncFlags[i].store(0);
        }
    }

    ~GlobalData() {
        #ifdef LOG_TRACE
            fclose(trace);
        #endif
        
        PIN_MutexFini(&lock);
    }
};

GlobalData* globals;

TLS_KEY tlsKey;
struct ThreadData {
    ADDRINT lockAddr;
    bool prevAcqFailed{false};
    ADDRINT prevWriteAddr;
    std::multiset<ADDRINT> lockSet;
    std::multiset<ADDRINT> rwlockSet;
};

VOID RecordMemRead(THREADID tid, VOID* ip, VOID* addr, UINT32 accessSize) { 
    LOG_INFO("TID: %d, ip: %p, R %p, %d bytes\n", tid, ip, addr, accessSize); 
    
    globals->wcpEngine.read(tid, Variable{.addr = (addr_t)addr});
}

VOID RecordMemWrite(THREADID tid, VOID* ip, VOID* addr, UINT32 accessSize) {
    LOG_INFO("TID: %d, ip: %p, W %p, %d bytes\n", tid, ip, addr, accessSize); 

    globals->wcpEngine.write(tid, Variable{.addr = (addr_t)addr});
}

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID* v)
{
    ADDRINT addr = INS_Address(ins);

    if (addr < globals->mainImgLow || addr >= globals->mainImgHigh || INS_IsAtomicUpdate(ins) || INS_LockPrefix(ins) || INS_Opcode(ins) == XED_ICLASS_XCHG)
        return; // Skip non-user code (e.g., libc, etc.) and atomic instructions

    UINT32 memOps = INS_MemoryOperandCount(ins);

    // Iterate over each memory operand of the instruction.
    for (UINT32 memOp = 0; memOp < memOps; memOp++)
    {
        UINT32 accessSize = INS_MemoryOperandSize(ins, memOp);

        if (INS_MemoryOperandIsRead(ins, memOp))
        {
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordMemRead,
                                    IARG_THREAD_ID,
                                    IARG_INST_PTR,
                                    IARG_MEMORYOP_EA, memOp,
                                    IARG_UINT32, accessSize,
                                    IARG_END);
        }

        if (INS_MemoryOperandIsWritten(ins, memOp))
        {
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordMemWrite,
                                    IARG_THREAD_ID,
                                    IARG_INST_PTR,
                                    IARG_MEMORYOP_EA, memOp,
                                    IARG_UINT32, accessSize,
                                    IARG_END);
        }
    }
}

VOID after_pthread_mutex_lock(THREADID tid, ADDRINT lockAddr) {
    LOG_INFO("TID: %d, Mutex locked: %p\n", tid, (VOID*)lockAddr);

    globals->wcpEngine.acquire(tid, Lock{.addr = lockAddr});
}

VOID after_pthread_trylock(THREADID tid, ADDRINT lockAddr, ADDRINT retVal) {
    if (retVal == 0) {
        LOG_INFO("TID: %d, Mutex locked: %p\n", tid, (VOID*)lockAddr);

        globals->wcpEngine.acquire(tid, Lock{.addr = lockAddr});
    }
}

VOID before_pthread_mutex_unlock(THREADID tid, ADDRINT lockAddr) {
    LOG_INFO("TID: %d, Mutex unlocked: %p\n", tid, (VOID*)lockAddr);

    globals->wcpEngine.release(tid, Lock{.addr = lockAddr});
}

VOID after_pthread_rwlock_wrlock(THREADID tid, ADDRINT lockAddr) {
    ThreadData *tdata = static_cast<ThreadData*>(PIN_GetThreadData(tlsKey, tid));
    tdata->rwlockSet.insert(lockAddr);

    LOG_INFO("TID: %d, Mutex locked: %p\n", tid, (VOID*)lockAddr);

    globals->wcpEngine.acquire(tid, Lock{.addr = lockAddr});
}

VOID after_pthread_rwlock_trywrlock(THREADID tid, ADDRINT lockAddr, ADDRINT retVal) {
    if (retVal == 0) {
        ThreadData *tdata = static_cast<ThreadData*>(PIN_GetThreadData(tlsKey, tid));
        tdata->rwlockSet.insert(lockAddr);

        LOG_INFO("TID: %d, Mutex locked: %p\n", tid, (VOID*)lockAddr);

        globals->wcpEngine.acquire(tid, Lock{.addr = lockAddr});
    }
}

VOID before_pthread_rwlock_unlock(THREADID tid, ADDRINT lockAddr) {
    ThreadData *tdata = static_cast<ThreadData*>(PIN_GetThreadData(tlsKey, tid));

    if(tdata->rwlockSet.count(lockAddr)) {
        tdata->rwlockSet.erase(tdata->rwlockSet.find(lockAddr));

        LOG_INFO("TID: %d, Mutex unlocked: %p\n", tid, (VOID*)lockAddr);
        
        globals->wcpEngine.release(tid, Lock{.addr = lockAddr});
    }
}

VOID before_pthread_create(THREADID tid) {
    LOG_INFO("TID: %d, Before Pthread Create\n", tid);

    globals->wcpEngine.before_pthread_create(tid);
}

VOID after_pthread_create(THREADID tid, ADDRINT threadPtr) {
    
    while (!globals->parChildSyncFlags[tid].load());    // wait for child to inherit parent's vector clocks

    globals->parChildSyncFlags[tid].store(0);
    THREADID childPinTid = globals->currChildPinTid[tid];

    pthread_t childPthread_t = 0;
    PIN_SafeCopy(&childPthread_t, (void*)threadPtr, sizeof(pthread_t));

    globals->pthreadToPinTid[childPthread_t] = childPinTid;

    LOG_INFO("TID: %d, After Pthread Create\n", tid);
}

void instrumentLockRoutines(IMG img, const char* funcName, AFUNPTR func, bool before) {
    RTN lockRtn = RTN_FindByName(img, funcName);
    if (RTN_Valid(lockRtn)) {
        RTN_Open(lockRtn);
        RTN_InsertCall(lockRtn, (before ? IPOINT_BEFORE : IPOINT_AFTER), func,
                       IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // mutex address in %rdi
                       IARG_END);
        RTN_Close(lockRtn);
    }
}

void instrumentTryLockRoutines(IMG img, const char* funcName, AFUNPTR func) {
    RTN lockRtn = RTN_FindByName(img, funcName);
    if (RTN_Valid(lockRtn)) {
        RTN_Open(lockRtn);
        RTN_InsertCall(lockRtn, IPOINT_AFTER, func,
                       IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // mutex address in %rdi
                       IARG_FUNCRET_EXITPOINT_VALUE, // return value in in %rax
                       IARG_END);
        RTN_Close(lockRtn);
    }
}

VOID ImageLoad(IMG img, VOID *v) {
    if (IMG_IsMainExecutable(img)) {
        globals->mainImgLow = IMG_LowAddress(img);
        globals->mainImgHigh = IMG_HighAddress(img);
    }

    RTN pthreadCreateRtn = RTN_FindByName(img, "pthread_create");
    if (RTN_Valid(pthreadCreateRtn)) {
        RTN_Open(pthreadCreateRtn);
        RTN_InsertCall(pthreadCreateRtn, IPOINT_BEFORE, (AFUNPTR)before_pthread_create,
                       IARG_THREAD_ID,
                       IARG_END);
        RTN_InsertCall(pthreadCreateRtn, IPOINT_AFTER, (AFUNPTR)after_pthread_create,
                       IARG_THREAD_ID,          // Parent TID
                       IARG_REG_VALUE, REG_RDI, // First argument (pthread_t *thread)
                       IARG_END);
        RTN_Close(pthreadCreateRtn);
    }

    // Note: There is something wrong with pthread_join, program doesn't execute the corresponding analysis routine
    // RTN pthreadJoinRtn = RTN_FindByName(img, "pthread_join");
    // if (RTN_Valid(pthreadJoinRtn)) {
    //     RTN_Open(pthreadJoinRtn);
    //     RTN_InsertCall(pthreadJoinRtn, IPOINT_AFTER, (AFUNPTR)after_pthread_joins,
    //                    IARG_THREAD_ID,          // Parent TID
    //                    IARG_REG_VALUE, REG_RDI, // First argument (pthread_t thread)
    //                    IARG_END);
    //     RTN_Close(pthreadJoinRtn);
    // }

    instrumentLockRoutines(img, "pthread_mutex_lock", (AFUNPTR)after_pthread_mutex_lock, false);
    instrumentLockRoutines(img, "pthread_mutex_unlock", (AFUNPTR)before_pthread_mutex_unlock, true);
    
    instrumentLockRoutines(img, "pthread_spin_lock", (AFUNPTR)after_pthread_mutex_lock, false);
    instrumentLockRoutines(img, "pthread_spin_unlock", (AFUNPTR)before_pthread_mutex_unlock, true);
    
    instrumentLockRoutines(img, "pthread_rwlock_wrlock", (AFUNPTR)after_pthread_rwlock_wrlock, false);
    instrumentLockRoutines(img, "pthread_rwlock_unlock", (AFUNPTR)before_pthread_rwlock_unlock, true);
    
    
    instrumentTryLockRoutines(img, "pthread_rwlock_trywrlock", (AFUNPTR)after_pthread_rwlock_trywrlock);
    instrumentTryLockRoutines(img, "pthread_mutex_trylock", (AFUNPTR)after_pthread_trylock);
    instrumentTryLockRoutines(img, "pthread_spin_trylock", (AFUNPTR)after_pthread_trylock);
}

VOID ThreadStart(THREADID tid, CONTEXT *ctx, INT32 flags, VOID *v) {
    if (tid >= globals->parChildSyncFlags.size()) {
        std::cout << "ERROR: Number of threads in target process exceeded " << globals->parChildSyncFlags.size() << '\n';
        std::cout << "HINT: Pass number of threads used by target process using -t flag" << std::endl;
        exit(EXIT_FAILURE);
    }

    ThreadData *tdata = new ThreadData();

    PIN_SetThreadData(tlsKey, tdata, tid);

    OS_THREAD_ID parentOsTid = PIN_GetParentTid();

    PIN_MutexLock(&globals->lock);
    
    if (parentOsTid != INVALID_OS_THREAD_ID && globals->osToPinTid.count(parentOsTid)) {
        THREADID parentTid = globals->osToPinTid[parentOsTid];
        globals->currChildPinTid[parentTid] = tid;

        globals->osToPinTid[PIN_GetTid()] = PIN_ThreadId();

        PIN_MutexUnlock(&globals->lock);

        globals->wcpEngine.thread_begin(tid, parentTid);
        
        globals->parChildSyncFlags[parentTid].store(1);

        LOG_INFO("Thread Begin, TID: %d, Parent TID: %d\n", tid, parentTid);
    }
    
    else {
        globals->osToPinTid[PIN_GetTid()] = PIN_ThreadId();

        PIN_MutexUnlock(&globals->lock);

        LOG_INFO("Thread Begin, TID: %d, No Parent\n", tid);
    }

    
}

VOID ThreadFini(THREADID tid, const CONTEXT *ctxt, INT32 code, VOID *v) {
    ThreadData *tdata = static_cast<ThreadData*>(PIN_GetThreadData(tlsKey, tid));
    
    OS_THREAD_ID parentOsTid = PIN_GetParentTid();

    PIN_MutexLock(&globals->lock);
    
    if (parentOsTid != INVALID_OS_THREAD_ID && globals->osToPinTid.count(parentOsTid)) {
        THREADID parentTid = globals->osToPinTid[parentOsTid];

        PIN_MutexUnlock(&globals->lock);
        
        globals->wcpEngine.thread_end(tid, parentTid);
        
        LOG_INFO("Thread Ended, TID: %d, Parent TID: %d\n", tid, parentTid);
    }
    
    else {
        PIN_MutexUnlock(&globals->lock);

        LOG_INFO("Thread Ended, TID: %d, No Parent\n", tid);
    }


    delete tdata;
}

KNOB<std::string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "wcp_analysis.out", "Specify output file name");

KNOB<int> KnobMaxThreads(KNOB_MODE_WRITEONCE, "pintool",
    "t", "8", "Specify maximum number of threads used by target process");

VOID Fini(INT32 code, VOID* v)
{
    FILE* statsFile = fopen(KnobOutputFile.Value().c_str(), "w");
    globals->wcpEngine.print_stats(statsFile);
    fclose(statsFile);

    std::cout << "----- WCP Analysis stats written to " << KnobOutputFile.Value().c_str() << " -----" << std::endl;

    LOG_INFO("#eof\n");

    delete globals;
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    PIN_ERROR("This Pintool prints a trace of memory addresses\n" + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char* argv[])
{   
    PIN_InitSymbols();
    if (PIN_Init(argc, argv)) return Usage();

    std::cout << "----- Starting WCP Analysis -----" << std::endl;

    tlsKey = PIN_CreateThreadDataKey(nullptr); // no destructor needed

    const int maxThreads = KnobMaxThreads.Value();
    globals = new GlobalData(maxThreads, "trace.out");

    IMG_AddInstrumentFunction(ImageLoad, nullptr);
    INS_AddInstrumentFunction(Instruction, nullptr);
    
    PIN_AddThreadStartFunction(ThreadStart, nullptr);
    PIN_AddThreadFiniFunction(ThreadFini, nullptr);

    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();

    return 0;
}
