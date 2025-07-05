# 🔍 WCP Analysis

## 🧠 Overview

- This project implements a dynamic analysis tool using the Intel PIN dynamic binary instrumentation framework to detect predictable data races in multithreaded programs based on the Weak Causal Precedence (WCP) model.

- The WCP model is a partial-order relation that extends the traditional happens-before (HB) relation by relaxing certain ordering constraints, allowing it to detect a wider class of data races while avoiding false positives. Compared to HB, WCP provides greater coverage of potential concurrency bugs without sacrificing soundness.

- This tool instruments a given binary executable at runtime and performs the following:
    - Monitors memory accesses (reads/writes) and synchronization events (lock acquires and releases, thread creation and termination, etc.)
    - Applies the WCP relation to infer potential races that are predictable (i.e., would appear under some feasible thread schedule)
    - Reports detected races along with their memory addresses, access types and threads involved

- By leveraging dynamic binary instrumentation, the tool requires no source code changes or recompilation of the target program. It works directly on binaries, making it suitable for analyzing legacy applications or third-party code.

## 🧪 Compilation

- We name the PIN root directory (containing the pin executable) as `DIR` (It could be something like
`∼/pin-external-3.31-98869-gfa6f126a8-gcc-linux`).

- Create a new directory `DIR/source/tools/WCPPinTool` and place the following in the newly created directory:
    - `WCPPinTool.cpp`
    - `makefile`, `makefile.rules`
    - `run.sh`, `test.sh`
    - `include` directory
    - `test` directory


- Create a directory `obj-intel64` in the `WCPPinTool` directory and use `make obj-intel64/WCPPinTool.so` to compile the PIN tool as follows:
```
DIR/source/tools/WCPPinTool$ mkdir obj-intel64
DIR/source/tools/WCPPinTool$ make obj-intel64/WCPPinTool.so
```

## 🚀 Running the Tool

- - Replace the value of PIN root directory on line 4 of `run.sh` by `DIR`. Use the following command to run the WCP Analysis for an executable. The maximum number of threads used by the target program should be provided using the `-t` flag (the default is 8). An output file can be optionally provided (the default is `wcp_analysis.out`). The output file contains information about detect data races (if any):
```
DIR/source/tools/WCPPinTool$ ./run.sh <executable> [-t <maxThreads>] [-o <output>]
```
- The `test` directory contains several racy and non-racy test cases. To run all the tests, Use the `test.sh` script. It creates a directory `output` and places the results of the analysis in it.
```
DIR/source/tools/WCPPinTool$ ./test.sh
```
- Note: The macro `LOG_TRACE` can be defined in the `WCPPinTool.cpp` file to generate a trace of the processed events in the file trace.out
