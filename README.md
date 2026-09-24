# Concurrent LSM-Tree Subsystem: Readers-Writers Engine

**Note on Repository Scope:** 
*This repository serves as a code showcase for an advanced concurrency subsystem built on top of an LSM-Tree (Log-Structured Merge-Tree) Key-Value store (Kiwi/LKL). To respect the original academic copyrights of the base engine, this repository contains **only the core files I directly engineered and modified** (`db.c`, `db.h`, `kiwi.c`, `bench.c`, `bench.h`). It highlights systems programming, multithreading, and synchronization concepts in C.*

## The Problem: Lock Contention and I/O Bottlenecks
The base Key-Value engine was either single-threaded or relied on a crude, global Mutual Exclusion (Mutex) lock. While a global lock guarantees thread safety, it completely paralyzes the system under heavy load. If a thread performed a slow I/O operation (like flushing the memtable to an SST file on disk), the entire database would freeze, blocking all other read and write requests.

## The Solution: State-Based Readers-Writers Lock with Writer Priority
To transform the engine into a highly responsive concurrent system, I engineered a custom **Readers-Writers synchronization mechanism** using POSIX Threads (`pthreads`), Mutexes, and Condition Variables. 

* **Concurrent Reads:** Multiple threads can now query the database simultaneously without blocking each other, exponentially increasing GET throughput.
* **Writer Priority:** To prevent "writer starvation" (where a continuous stream of readers prevents data updates), the condition variables are explicitly structured to give priority to waiting writers.
* **Safe Compaction:** When the Memtable fills up, the writer thread safely halts incoming operations, flushes the data to the disk (SST files), and gracefully wakes up the waiting threads without deadlocks.

## Code Walkthrough: Where to Look
If you are reviewing this code, here are the key areas demonstrating my implementation:

* **`engine/db.c` & `engine/db.h` (The Synchronization Core):** 
  Check the `db_add` and `db_get` functions. You will see the implementation of the state-based lock utilizing `pthread_cond_wait` and `pthread_cond_signal`. Notice how the lock is dynamically acquired and released to update the `active_writers` and `active_readers` counters, keeping the critical sections as small as possible.
* **`bench/kiwi.c` (The Benchmarking Suite & Memory Safety):**
  Check the `mixed_worker_thread` function. I implemented a robust multithreaded benchmarking tool utilizing `rand_r` with unique thread-based seeds for thread-safe randomization. Additionally, I managed dynamic heap allocations (e.g., `sv_read.mem = malloc(1)`) to safely interface with the engine's internal `realloc` calls, completely avoiding memory corruption and segmentation faults.
* **Performance Metrics (`stats_mutex`):** 
  Also in `kiwi.c`, observe the isolated `stats_mutex`. Instead of locking the entire operation, I used a dedicated, short-lived mutex strictly for safely aggregating microsecond-precision performance metrics (`gettimeofday`) across multiple threads.

## Experimental Proof & Benchmarks

To validate the architecture, I subjected the subsystem to heavy concurrent loads using 100,000 mixed operations across multiple worker threads.

*(Insert Screenshot 1 Here: Terminal showing the `mixed_test` running and the "Memtable full! Starting compaction myself..." message proving the safe background compaction)*

### Throughput Results
By testing a realistic database workload (e.g., 20% Writes, 80% Reads), the system proved the efficiency of the Readers-Writers mechanism:
* **GET operations** achieved massive throughput due to concurrent read allowances.
* **ADD operations** executed safely, smoothly triggering compactions without causing deadlocks.
* As thread count increased, the system remained highly responsive, successfully bypassing the classic I/O freezing issues of a single-threaded architecture.

*(Insert Screenshot 2 Here: Terminal showing the "DETAILED PERFORMANCE STATISTICS" with the Ops/sec metrics)*

---
*Built with C and POSIX Threads on Linux.*
