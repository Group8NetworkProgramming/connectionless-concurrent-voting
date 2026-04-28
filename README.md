# SONU Election Server — Concurrency Implementation with Processes

## Overview

This project implements a **UDP-based Student Organisation of Nairobi University (SONU) Election Server** that supports voter registration, candidate registration, vote casting, live tallying, and results announcement. The server was converted from a single-process design into a **concurrent server using process-based concurrency** via `fork()`, with `flock()` for file-level locking to prevent data corruption, and comprehensive `[TRACE]` debug logging throughout.

---

## Changes Implemented

### 1. `fork()` — Process-Based Concurrency

**File:** `server/server_main.c`

Each incoming UDP request is handled by a **new child process** created with `fork()`. The parent process receives the initial command packet, then immediately forks a child to handle the rest of that request. The parent loops back to listen for the next packet without waiting, allowing **multiple clients to be served simultaneously**.

Key details:
- `signal(SIGCHLD, SIG_IGN)` is set before the main loop to automatically reap terminated child processes and prevent zombies from accumulating.
- After forking, the parent continues listening while the child handles the client request and then calls `exit(0)` to terminate cleanly — this is critical to prevent the child from looping back to `recvfrom()`.

### 2. `fflush(stdout)` — Preventing Duplicate Output

`fflush(stdout)` is called **before every `fork()`** and after every `printf()` statement. When `fork()` is called, the child inherits a copy of the parent's memory, including any buffered output sitting in `stdout`. Without flushing first, both parent and child would eventually flush the same buffered text, resulting in duplicated lines in the terminal. Calling `fflush()` ensures the buffer is empty before the fork.

### 3. `sleep(2)` — Simulated Processing Delay

Each child process calls `sleep(2)` immediately after being spawned. This introduces a deliberate 2-second delay that serves two purposes:
- It simulates real-world processing time (database queries, network latency, etc.).
- It makes concurrency **visible during demonstrations** — you can see multiple child processes alive at the same time when requests arrive in quick succession.

### 4. `flock()` — File-Level Locking

**Files:** `server/utils.c`, `server/server_voters.c`, `server/server_candidates.c`, `server/server_votes.c`

Since multiple child processes may read from or write to the same `.dat` files simultaneously, `flock()` is used to coordinate access and prevent data corruption.

**Lock types used:**

| Lock | Function | When Used |
|------|----------|-----------|
| `LOCK_SH` (Shared/Read) | `flock(fd, LOCK_SH)` | When a process only needs to **read** a file. Multiple processes can hold shared locks at the same time. |
| `LOCK_EX` (Exclusive/Write) | `flock(fd, LOCK_EX)` | When a process needs to **write** to a file. Only one process can hold an exclusive lock — all others block until it is released. |
| `LOCK_UN` (Unlock) | `flock(fd, LOCK_UN)` | Releases whichever lock the process is holding. |

**Files protected:**

| File | Read Lock (`LOCK_SH`) | Write Lock (`LOCK_EX`) |
|------|----------------------|----------------------|
| `voters.dat` | `is_id_registered()` — checking if a voter ID exists | `server_register_voter()` — appending a new voter record; `server_cast_vote()` — updating `has_voted` flag |
| `candidates.dat` | `is_id_registered()` — checking if a candidate ID exists; `server_view_candidates()` — reading all candidates; `server_send_large_text()` — reading for tally/results | `server_register_candidate()` — appending a new candidate record; `server_cast_vote()` — incrementing vote count |
| `server.log` | — | `log_event()` — appending log entries (exclusive lock prevents interleaved lines from concurrent processes) |

### 5. `[TRACE]` Debug Print Statements

**Files:** All server files

Every significant operation now has a `printf("[TRACE] ...")` statement immediately before it, followed by `fflush(stdout)`. The PID of the current process is included in every trace line so you can distinguish parent from children when multiple processes are running.

**Operations logged:**

| Category | What is Logged |
|----------|---------------|
| **Process Lifecycle** | Fork calls, child spawned (with PID), child sleeping, child terminating, parent resuming |
| **UDP Network I/O** | Bytes sent (with destination IP:port), bytes received (with source IP:port), waiting for packets |
| **File Opens** | Every `fopen()` call with filename and mode |
| **Lock Transitions** | Acquiring SHARED lock, acquiring EXCLUSIVE lock, lock acquired confirmation, releasing lock |
| **File Reads** | Number of records read, search results (ID found/not found) |
| **File Writes** | Record being written, candidate vote count incremented (old → new), voter marked as `has_voted` |
| **CPU Processing** | ID lookups, array scans, tally/results text generation |

---

## Project Structure

```
.
├── Makefile
├── README.md
├── shared/
│   ├── protocol.h          # Command codes and shared constants
│   ├── voter.h              # Voter struct definition
│   └── candidate.h          # Candidate struct definition
├── server/
│   ├── server.h             # Server function declarations
│   ├── server_main.c        # Main loop with fork() concurrency
│   ├── server_voters.c      # Voter registration handler
│   ├── server_candidates.c  # Candidate registration & viewing handlers
│   ├── server_votes.c       # Vote casting, tally & results handlers
│   └── utils.c              # File I/O helpers (with flock), logging
├── client/
│   └── client_main.c        # Client application
├── voters.dat               # Generated at runtime — voter records
├── candidates.dat           # Generated at runtime — candidate records
└── server.log               # Generated at runtime — event log
```

---

## How to Build and Run

### Prerequisites

- GCC compiler
- Linux/macOS environment (for `fork()` and `flock()` support)

### Build

```bash
# Build both server and client
make

# Or build them individually
make server
make client
```

This compiles:
- `server_app` from all files in `server/`
- `client_app` from `client/client_main.c`

### Run

**Terminal 1 — Start the server:**
```bash
./server_app
```

You should see:
```
--- SONU UDP Server Running on Port 8080 ---
[TRACE] Parent (PID: 12345) waiting for incoming command...
```

**Terminal 2 — Connect a client:**
```bash
./client_app
```

**Terminal 3 — Connect a second client (while the first is still active):**
```bash
./client_app
```

### Clean Up

```bash
# Remove compiled binaries
make clean

# Remove binaries AND all .dat databases and .log files
make wipe
```

---

## How to Verify Concurrency

With the server running, open two client terminals and perform operations on both simultaneously. In the server terminal, you will see `[TRACE]` lines from different child processes **interleaved**, each tagged with a different PID:

```
[TRACE] Incoming packet detected! Command code: 1 from 127.0.0.1:54321. Forking new process...
[TRACE] Child process (PID: 12346) spawned for command 1. Sleeping for 2s...
[TRACE] Parent (PID: 12345) forked child (PID: 12346). Resuming listen loop.
[TRACE] Parent (PID: 12345) waiting for incoming command...
[TRACE] Incoming packet detected! Command code: 2 from 127.0.0.1:54322. Forking new process...
[TRACE] Child process (PID: 12347) spawned for command 2. Sleeping for 2s...
[TRACE] Child process (PID: 12346): Acquiring EXCLUSIVE lock on voters.dat...
[TRACE] Child process (PID: 12347): Acquiring EXCLUSIVE lock on candidates.dat...
```

**What to look for:**

1. **Two different child PIDs** appearing in the output confirms both requests are being handled concurrently.
2. **Lock contention** — if both clients write to the same file, you'll see one child acquire the EXCLUSIVE lock first, and the second child's lock acquire will appear only after the first releases.
3. **No data corruption** — both records should appear correctly in the `.dat` files despite concurrent writes.