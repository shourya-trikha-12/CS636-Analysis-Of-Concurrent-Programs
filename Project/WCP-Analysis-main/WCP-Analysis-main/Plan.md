# Plan

## Evaluation 2 (Mar 24-28, 2025)
- Implementation of WCP Analysis Class for race detection that can handle the following events
    - `read(x)/write(x)`    (memory operations)
    - `acq(l)/rel(l)`   (lock operations)
    - `fork()/join()`   (thread operations)
- Generate test cases to verify the correctness of the implementation

## Evaluation 3 (Apr 14-18, 2025)
- Write a Pin tool and integrate with the WCP class to achieve end-to-end race detection for a given binary executable
- Do Testing and Performance Analysis
- Optimize (if possible)