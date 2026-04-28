# SONU Election Server — Concurrency Implementation (Receive-Before-Fork)

## Overview

This project implements a **UDP-based SONU Election Server** that was converted from a single-process design into a **concurrent server using process-based concurrency** via `fork()`. A critical race condition in the original fork design was identified and fixed using the **receive-before-fork** pattern, `flock()` was added for file-level locking, and comprehensive `[TRACE]` logging was added throughout.

---

## The Race Condition Problem

The original SONU protocol uses **two UDP packets per request**: first a command code (`int`), then the follow-up data (a `Voter`, `Candidate`, or vote struct). The first attempt at concurrency forked a child after receiving the command code, and the child then called `recvfrom()` to get the follow-up data. This created a **race condition**:

```
Parent calls recvfrom() ──► receives command code ──► fork()
                                                        │
                          ┌─────────────────────────────┤
                          ▼                             ▼
                    CHILD process                 PARENT process
                    calls recvfrom()              loops back to recvfrom()
                    waiting for voter data        waiting for next command
                          │                             │
                          └──── BOTH compete for the same UDP packet! ────┘
```

The parent wins the race, reads the `Voter` struct's raw bytes as a command code (e.g., ASCII `'1'` = decimal 49), and forks another child for this bogus "command 49". Meanwhile the real child is stuck waiting for data that has already been consumed.

---

## The Fix: Receive-Before-Fork (Option B)

The parent now receives **all packets for a request before forking**. The child inherits the complete data through `fork()` memory copy and **never calls `recvfrom()`**.

```
Parent calls recvfrom() ──► receives command code
Parent calls recvfrom() ──► receives follow-up data (Voter/Candidate/vote)
Parent calls fork()
          │
          ├──► CHILD: has all data in memory, processes and responds, exits
          │
          └──► PARENT: loops back to recvfrom() for next command
               (no race — child never touches the socket for receiving)
```

### File Changed: `server_main.c`

The main loop now has a `switch` block between the command `recvfrom()` and `fork()` that receives the appropriate follow-up data based on the command type:

| Command | Follow-up Data Received by Parent |
|---|---|
| `CMD_REG_VOTER` | `Voter` struct (76 bytes) |
| `CMD_REG_CANDIDATE` | `Candidate` struct |
| `CMD_CAST_VOTE` | `{voter_id, candidate_id}` struct |
| `CMD_VIEW_CANDIDATES` | None (read-only query) |
| `CMD_VIEW_TALLY` | None (read-only query) |
| `CMD_RESULTS` | None (read-only query) |

### Files Changed: `server.h`, `server_voters.c`, `server_candidates.c`, `server_votes.c`

Handler function signatures were updated to accept pre-received data as parameters instead of calling `udp_recv()` internally:

```c
// BEFORE (child calls recvfrom — causes race condition):
void server_register_voter(int sock, struct sockaddr_in *cli);

// AFTER (data passed in — child never calls recvfrom):
void server_register_voter(int sock, struct sockaddr_in *cli, Voter *v);
```

---

## Changes Implemented

### 1. `fork()` — Process-Based Concurrency

Each incoming request is handled by a **child process** created with `fork()`. After the parent has received all data for a request, it forks. The child processes the request and sends the response. The parent loops back to listen immediately.

- **`sigaction(SIGCHLD, ...)`** with `waitpid(WNOHANG)` is used to reap terminated children and prevent zombie processes.
- **`exit(0)`** at the end of each child is critical — without it, the child would loop back to `recvfrom()` and create a duplicate listener.

### 2. `fflush(stdout)` — Preventing Duplicate Output

`fflush(stdout)` is called **before every `fork()`** and after every `printf()`. When `fork()` is called, the child inherits a copy of the parent's memory, including buffered stdout. Without flushing first, both processes would print the same buffered text.

### 3. `sleep(2)` — Simulated Processing Delay

Each child calls `sleep(2)` immediately after being spawned. This makes concurrency visible during testing — you can send two requests quickly and see both children alive simultaneously in the trace output.

### 4. `flock()` — File-Level Locking

Since multiple child processes may access the same `.dat` files simultaneously, `flock()` prevents data corruption.

| Lock Type | When Used | Why |
|---|---|---|
| `LOCK_SH` (Shared) | `is_id_registered()`, `read_record()`, `server_view_candidates()`, `server_send_large_text()`, voter lookup in `server_cast_vote()` | Multiple processes can read simultaneously without conflict. |
| `LOCK_EX` (Exclusive) | `append_record()`, `server_register_voter()`, `server_register_candidate()`, candidate vote update in `server_cast_vote()`, voter status update in `server_cast_vote()`, `log_event()` | Only one process can write at a time. All others block until the lock is released. |

**Files protected:** `voters.dat`, `candidates.dat`, `server.log`

### 5. `[TRACE]` Debug Logging

Every significant operation has a `printf("[TRACE] ...")` with `fflush(stdout)`. PID is included in every line.

| Category | What is Logged |
|---|---|
| **UDP Socket Ops** | Socket creation, bind, every `recvfrom()` (with byte count and source IP:port), every `sendto()` (with byte count and dest IP:port) |
| **Process Lifecycle** | Pre-fork data receive, `fork()` call, child spawned (with PID and command), `sleep(2)`, child terminating, parent resuming |
| **File I/O** | Every `fopen()` with filename and mode, every `fread()` with byte count, every `fwrite()` with byte count, every `fclose()` |
| **Lock Transitions** | Acquiring SHARED lock, acquiring EXCLUSIVE lock, lock acquired confirmation, releasing lock |
| **CPU Processing** | ID lookups (found/not found), vote count increment (old → new), voter status update, tally/results text generation |

---

## Project Structure

```
├── Makefile
├── README.md
├── shared/
│   ├── protocol.h
│   ├── voter.h
│   └── candidate.h
├── server/
│   ├── server.h              # Updated handler signatures (receive-before-fork)
│   ├── server_main.c         # Parent receives all data, then forks child
│   ├── server_voters.c       # Accepts pre-received Voter* parameter
│   ├── server_candidates.c   # Accepts pre-received Candidate* parameter
│   ├── server_votes.c        # Accepts pre-received vote data parameter
│   └── utils.c               # File I/O with flock(), logging with flock()
└── client/
    └── client_main.c
```

---

## How to Build and Run

### Build

```bash
# Build both server and client
make

# Or build individually
make server
make client
```

### Run

**Terminal 1 — Start the server:**
```bash
./server_app
```

**Terminal 2 — Connect a client:**
```bash
./client_app
```

**Terminal 3 — Connect a second client simultaneously:**
```bash
./client_app
```

### Clean Up

```bash
make clean    # Remove compiled binaries
make wipe     # Remove binaries AND all .dat and .log files
```

---

## How to Verify the Fix

With the server running, register a voter from a client. The server trace should show:

```
[TRACE] Parent (PID: 100): Received command code 1 from 127.0.0.1:54321
[TRACE] Parent (PID: 100): Command is REG_VOTER. Receiving voter data before fork...
[TRACE] UDP RECV: Received 76 bytes from 127.0.0.1:54321
[TRACE] Parent (PID: 100): Voter data received (ID: 'STU001'). Ready to fork.
[TRACE] Parent (PID: 100): Forking child process to handle command 1...
[TRACE] Child (PID: 101): Process spawned for command 1.
[TRACE] Child (PID: 101): Simulating processing delay (sleep 2s)...
[TRACE] Parent (PID: 100): Child (PID: 101) forked successfully. Resuming listen loop.
[TRACE] Parent (PID: 100): Waiting for incoming command packet...
```

**What confirms the fix works:**
- The parent receives **both** the command (4 bytes) and the voter data (76 bytes) **before** "Forking child process" appears.
- The child **never** shows a `UDP RECV` line — it uses data inherited from the parent.
- No "Unknown command code 49" errors appear.
- A second client's request can be processed while the first child is sleeping.

---

## Summary of Changes Per File

| File | Changes |
|---|---|
| `server.h` | Updated handler signatures to accept pre-received data pointers. Added `sys/types.h` and `unistd.h`. Changed `udp_recv` return type to `ssize_t`. |
| `server_main.c` | Parent now receives all follow-up data before `fork()`. Added SIGCHLD handler with `waitpid`. Added TRACE logging on every socket op, fork, and recv. |
| `server_voters.c` | `server_register_voter()` now takes `Voter *v` parameter. Removed internal `udp_recv()` call. Added TRACE logging on every lock, file write, and CPU step. |
| `server_candidates.c` | `server_register_candidate()` now takes `Candidate *c` parameter. Removed internal `udp_recv()` call. Added TRACE logging throughout. |
| `server_votes.c` | `server_cast_vote()` now takes `void *vote_data` parameter. Removed internal `udp_recv()` call. Added `flock(LOCK_SH)` on voter lookup, `flock(LOCK_EX)` on candidate update and voter update. Added TRACE logging throughout. `server_send_large_text()` now uses `flock(LOCK_SH)` when reading candidates. |
| `utils.c` | Added `flock(LOCK_SH)` to `is_id_registered()` and `read_record()`. Added `flock(LOCK_EX)` to `append_record()` and `log_event()`. Added TRACE logging on every operation. |
