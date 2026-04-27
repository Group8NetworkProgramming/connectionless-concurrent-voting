
Concurrent UDP Server (Fork + Flock)

Changes implemented:

1. fork()
- Each request handled by child process

2. fflush()
- Prevent duplicate output

3. sleep(2)
- Simulates processing delay

4. flock()
- Prevents concurrent file corruption
- Applied on:
    - voters.dat
    - candidates.dat

5. Debug prints
- Shows locking, writing, unlocking
- Shows process IDs

How to run:
gcc *.c -o server
./server
