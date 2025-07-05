#pragma once

#include <forward_list>
#include <variant>
#include <sys/mman.h>
#include "Utils.hpp"

using Metadata_Type = std::variant<LockMetadata, VariableMetadata>;
using Bucket_Type = std::forward_list<Metadata_Type>;

class ShadowMemory {
private:
    uint64_t* shadowBase;
    const size_t shadowSize = 1ULL << 45;   // 1ULL << 42 bucket pointers, Max 8 nodes per bucket
    size_t maxThreads;

    Bucket_Type& lock_and_get_bucket (size_t bucketIdx);

    void unlock_bucket (size_t bucketIdx);


public:
    ShadowMemory (int maxThreads);

    LockMetadata& lock_and_get_lock_metadata (Lock l);

    VariableMetadata& lock_and_get_variable_metadata (Variable x);

    void unlock_lock_metadata (Lock l);

    void unlock_variable_metadata (Variable x);
};


ShadowMemory::ShadowMemory (int maxThreads) : maxThreads(maxThreads) {
    shadowBase = (uint64_t*) mmap(nullptr, shadowSize, PROT_READ | PROT_WRITE, 
                                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, 
                                  -1, 0);

    if ((void*)shadowBase == MAP_FAILED) {
        perror("mmap failed");
        std::cerr << "Shadow Memory initialization failed" << std::endl;
        exit(EXIT_FAILURE);
    }
}

Bucket_Type& ShadowMemory::lock_and_get_bucket (size_t bucketIdx) {
    auto ptrToMarkableBucketPtr = shadowBase + bucketIdx;

    while(true) {
        auto markableBucketPtr = __atomic_load_n(ptrToMarkableBucketPtr, __ATOMIC_SEQ_CST);

        bool isMarked = (markableBucketPtr & 1);    // (markable pointer --> (addr << 1) + mark)

        if (!isMarked) {
            auto markedBucketPtr = (markableBucketPtr | 1);

            if ( __atomic_compare_exchange_n(ptrToMarkableBucketPtr, &markableBucketPtr, markedBucketPtr,
                                            true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
            {   // Bucket Locked

                auto bucketPtr = (std::forward_list<Metadata_Type>*)(markedBucketPtr >> 1);
                
                if (bucketPtr == nullptr) { // bucket not initialized yet

                    bucketPtr = new std::forward_list<Metadata_Type>;   // TODO: free in destructor?, difficult to do
                    auto newMarkedBucketPtr = ((uint64_t)bucketPtr << 1) | 1;
                    
                    __atomic_store_n(ptrToMarkableBucketPtr, newMarkedBucketPtr, __ATOMIC_SEQ_CST);
                }
                
                auto& bucket = *bucketPtr;

                return bucket;
            }
        }
    }
}

void ShadowMemory::unlock_bucket (size_t bucketIdx) {
    auto ptrToMarkableBucketPtr = shadowBase + bucketIdx;

    auto markableBucketPtr = __atomic_load_n(ptrToMarkableBucketPtr, __ATOMIC_SEQ_CST);

    auto unmarkedBucketPtr = (markableBucketPtr >> 1) << 1;

    __atomic_store_n(ptrToMarkableBucketPtr, unmarkedBucketPtr, __ATOMIC_SEQ_CST);
}

LockMetadata& ShadowMemory::lock_and_get_lock_metadata (Lock l) {
    size_t bucketIdx = (l.addr >> 2) % (shadowSize / sizeof(void*));
    auto& bucket = lock_and_get_bucket(bucketIdx);

    for (auto& node: bucket) {
        if(std::holds_alternative<LockMetadata>(node)) {
            auto& lockMetaData = std::get<LockMetadata>(node);

            if (lockMetaData.l == l) {
                return lockMetaData;
            }
        }
    }

    // node not found in bucket
    bucket.emplace_front(std::in_place_type<LockMetadata>, maxThreads, l);
    return std::get<LockMetadata>(bucket.front());
}

VariableMetadata& ShadowMemory::lock_and_get_variable_metadata (Variable x) {
    size_t bucketIdx = (x.addr >> 2) % (shadowSize / sizeof(void*));
    auto& bucket = lock_and_get_bucket(bucketIdx);

    for (auto& node: bucket) {
        if(std::holds_alternative<VariableMetadata>(node)) {
            auto& variableMetadata = std::get<VariableMetadata>(node);

            if (variableMetadata.x == x) {
                return variableMetadata;
            }
        }
    }

    // node not found in bucket
    bucket.emplace_front(std::in_place_type<VariableMetadata>, maxThreads, x);
    return std::get<VariableMetadata>(bucket.front());
}

void ShadowMemory::unlock_lock_metadata (Lock l) {
    size_t bucketIdx = (l.addr >> 2) % (shadowSize / sizeof(void*));

    unlock_bucket(bucketIdx);
}

void ShadowMemory::unlock_variable_metadata (Variable x) {
    size_t bucketIdx = (x.addr >> 2) % (shadowSize / sizeof(void*));
    
    unlock_bucket(bucketIdx);
}