#include <stdio.h>
#include <stdlib.h>
#include "server.h"
#include <time.h>   
#include <string.h>
#include <stdarg.h>
#include "../shared/candidate.h"
#include <unistd.h>
#include <sys/file.h>

typedef struct {
    char student_id[15];
    char name[50];
    int  has_voted; 
    int  votes_cast;
} Voter;

int is_id_registered(const char* id, const char* filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) return 0;
    
    flock(fileno(fp), LOCK_SH); // Shared lock for reading
    printf("[TRACE] Checking ID existence with SHARED lock on %s...\n", filename);
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
    flock(fileno(fp), LOCK_UN);
    fclose(fp); 
    return result;
}

int read_record(const char *filename, long index, void *out, size_t record_size) {
    FILE *f = fopen(filename, "rb");
    if (f == NULL) return 0;

    fseek(f, index * record_size, SEEK_SET);
    int result = fread(out, record_size, 1, f) == 1;
    fclose(f);
    return result;
}

int append_record(const char *filename, void *record, size_t record_size) {
    FILE *f = fopen(filename, "ab+");
    if (f == NULL) return -1;

    fseek(f, 0, SEEK_END);
    int new_id = (ftell(f) / record_size) + 1;

    fwrite(record, record_size, 1, f);
    fclose(f);
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

    // 3. Print to the Server Terminal (so you can see it live!)
    printf("[%s] [%s] %s\n", time_str, level, message_buffer);

    // 4. Save to the permanent log file
    FILE *log_file = fopen("server.log", "a");
    if (log_file) {
        fprintf(log_file, "[%s] [%s] %s\n", time_str, level, message_buffer);
        fclose(log_file);
    } else {
        printf("[!] CRITICAL: Could not open server.log for writing!\n");
    }
}