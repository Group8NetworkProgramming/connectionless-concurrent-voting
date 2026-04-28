#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <sys/file.h>
#include "../shared/voter.h"     // Included first
#include "../shared/candidate.h" // Included first
#include "server.h"

int is_id_registered(const char* id, const char* filename) {
    printf("[TRACE] is_id_registered (PID: %d): Opening '%s' to check for ID '%s'...\n", getpid(), filename, id);
    fflush(stdout);
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        printf("[TRACE] is_id_registered (PID: %d): File '%s' does not exist yet. Returning not registered.\n", getpid(), filename);
        fflush(stdout);
        return 0;
    }

    // --- Acquire shared lock for reading ---
    printf("[TRACE] is_id_registered (PID: %d): Acquiring SHARED lock on '%s'...\n", getpid(), filename);
    fflush(stdout);
    flock(fileno(fp), LOCK_SH);
    printf("[TRACE] is_id_registered (PID: %d): SHARED lock acquired. Scanning records...\n", getpid());
    fflush(stdout);

    int result = 0;
    if (strcmp(filename, "voters.dat") == 0) {
        Voter temp;
        while (fread(&temp, sizeof(Voter), 1, fp)) {
            if (strcmp(temp.student_id, id) == 0) { result = 1; break; }
        }
    } else {
        Candidate temp;
        while (fread(&temp, sizeof(Candidate), 1, fp)) {
            if (strcmp(temp.student_id, id) == 0) { result = 1; break; }
        }
    }

    printf("[TRACE] is_id_registered (PID: %d): ID '%s' %s in '%s'.\n",
           getpid(), id, result ? "FOUND" : "NOT FOUND", filename);
    fflush(stdout);

    // --- Release lock ---
    printf("[TRACE] is_id_registered (PID: %d): Releasing SHARED lock on '%s'.\n", getpid(), filename);
    fflush(stdout);
    flock(fileno(fp), LOCK_UN);
    fclose(fp);
    return result;
}

int read_record(const char *filename, long index, void *out, size_t record_size) {
    printf("[TRACE] read_record (PID: %d): Opening '%s' to read record at index %ld (record size: %zu)...\n",
           getpid(), filename, index, record_size);
    fflush(stdout);
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        printf("[TRACE] read_record (PID: %d): FAILED to open '%s'.\n", getpid(), filename);
        fflush(stdout);
        return 0;
    }

    // --- Acquire shared lock for reading ---
    printf("[TRACE] read_record (PID: %d): Acquiring SHARED lock on '%s'...\n", getpid(), filename);
    fflush(stdout);
    flock(fileno(f), LOCK_SH);
    printf("[TRACE] read_record (PID: %d): SHARED lock acquired.\n", getpid());
    fflush(stdout);

    printf("[TRACE] read_record (PID: %d): Seeking to byte offset %ld...\n", getpid(), index * record_size);
    fflush(stdout);
    fseek(f, index * record_size, SEEK_SET);

    printf("[TRACE] read_record (PID: %d): File Read — reading %zu bytes from disk...\n", getpid(), record_size);
    fflush(stdout);
    int result = fread(out, record_size, 1, f) == 1;
    printf("[TRACE] read_record (PID: %d): Read %s.\n", getpid(), result ? "successful" : "failed");
    fflush(stdout);

    // --- Release lock ---
    printf("[TRACE] read_record (PID: %d): Releasing SHARED lock on '%s'.\n", getpid(), filename);
    fflush(stdout);
    flock(fileno(f), LOCK_UN);
    fclose(f);
    return result;
}

int append_record(const char *filename, void *record, size_t record_size) {
    printf("[TRACE] append_record (PID: %d): Opening '%s' for appending (record size: %zu)...\n",
           getpid(), filename, record_size);
    fflush(stdout);
    FILE *f = fopen(filename, "ab+");
    if (f == NULL) {
        printf("[TRACE] append_record (PID: %d): FAILED to open '%s'!\n", getpid(), filename);
        fflush(stdout);
        return -1;
    }

    // --- Acquire exclusive lock for writing ---
    printf("[TRACE] append_record (PID: %d): Acquiring EXCLUSIVE lock on '%s'...\n", getpid(), filename);
    fflush(stdout);
    flock(fileno(f), LOCK_EX);
    printf("[TRACE] append_record (PID: %d): EXCLUSIVE lock acquired.\n", getpid());
    fflush(stdout);

    fseek(f, 0, SEEK_END);
    int new_id = (ftell(f) / record_size) + 1;
    printf("[TRACE] append_record (PID: %d): File Write — appending record (new ID: %d, %zu bytes) to '%s'...\n",
           getpid(), new_id, record_size, filename);
    fflush(stdout);

    fwrite(record, record_size, 1, f);

    // --- Release lock ---
    printf("[TRACE] append_record (PID: %d): Write complete. Releasing EXCLUSIVE lock on '%s'.\n", getpid(), filename);
    fflush(stdout);
    flock(fileno(f), LOCK_UN);
    fclose(f);

    printf("[TRACE] append_record (PID: %d): Record appended successfully to '%s'.\n", getpid(), filename);
    fflush(stdout);
    return new_id;
}

void log_event(const char *level, const char *format, ...) {
    // 1. Generate the timestamp
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

    // 2. Safely parse the variable arguments into a single string
    char message_buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message_buffer, sizeof(message_buffer), format, args);
    va_end(args);

    // 3. Print to the Server Terminal
    printf("[%s] [%s] (PID: %d) %s\n", time_str, level, getpid(), message_buffer);
    fflush(stdout);

    // 4. Save to the permanent log file with exclusive lock for process safety
    FILE *log_file = fopen("server.log", "a");
    if (log_file) {
        printf("[TRACE] log_event (PID: %d): Acquiring EXCLUSIVE lock on server.log...\n", getpid());
        fflush(stdout);
        flock(fileno(log_file), LOCK_EX);
        fprintf(log_file, "[%s] [%s] (PID: %d) %s\n", time_str, level, getpid(), message_buffer);
        flock(fileno(log_file), LOCK_UN);
        fclose(log_file);
    } else {
        printf("[!] CRITICAL: Could not open server.log for writing!\n");
        fflush(stdout);
    }
}
