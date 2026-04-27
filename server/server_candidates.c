#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include "server.h"
#include "../shared/candidate.h"

void server_register_candidate(int sock, struct sockaddr_in *cli) {
    Candidate c;
    printf("[TRACE] Candidate Registration: Waiting to receive candidate data (PID: %d)...\n", getpid());
    fflush(stdout);
    udp_recv(sock, &c, sizeof(Candidate), cli);
    printf("[TRACE] Candidate Registration: Received data for student ID '%s', name '%s'.\n", c.student_id, c.name);
    fflush(stdout);
    char msg[256];
    
    printf("[TRACE] Candidate Registration: Checking if ID '%s' is already registered...\n", c.student_id);
    fflush(stdout);
    if (is_id_registered(c.student_id, "candidates.dat")) {
        strcpy(msg, "Error: Candidate ID already registered.");
        log_event("WARN", "Candidate registration failed: '%s' registered.", c.student_id);
    } else {
        printf("[TRACE] Candidate Registration: ID '%s' not found. Opening candidates.dat for writing...\n", c.student_id);
        fflush(stdout);
        FILE *fp = fopen("candidates.dat", "ab+");
        if (fp) {
            int fd = fileno(fp);
            
            printf("[TRACE] Acquiring EXCLUSIVE lock on candidates.dat for registration (PID: %d)...\n", getpid());
            fflush(stdout);
            flock(fd, LOCK_EX);
            
            c.votes = 0;
            printf("[TRACE] Writing candidate record to disk...\n");
            fflush(stdout);
            fwrite(&c, sizeof(Candidate), 1, fp);
            
            flock(fd, LOCK_UN);
            fclose(fp);
            
            snprintf(msg, sizeof(msg), "Candidate registered!");
            log_event("SUCCESS", "Registered candidate '%s' (ID: %s) for position index %d.", c.name, c.student_id, c.position_index);
        } else {
            strcpy(msg, "Server DB Error.");
            log_event("ERROR", "Failed to open candidates.dat.");
        }
    }
    printf("[TRACE] Candidate Registration: Sending response to client: '%s'\n", msg);
    fflush(stdout);
    udp_send(sock, msg, sizeof(msg), cli);
}

void server_view_candidates(int sock, struct sockaddr_in *cli) {
    printf("[TRACE] View Candidates: Opening candidates.dat for reading (PID: %d)...\n", getpid());
    fflush(stdout);
    FILE *fp = fopen("candidates.dat", "rb");
    Candidate all[200];
    int count = 0;
    
    if (fp) {
        int fd = fileno(fp);
        printf("[TRACE] View Candidates: Acquiring SHARED lock on candidates.dat (PID: %d)...\n", getpid());
        fflush(stdout);
        flock(fd, LOCK_SH);
        
        printf("[TRACE] View Candidates: Reading candidate records from disk...\n");
        fflush(stdout);
        while (fread(&all[count], sizeof(Candidate), 1, fp)) count++;
        printf("[TRACE] View Candidates: Read %d candidate records.\n", count);
        fflush(stdout);
        
        printf("[TRACE] View Candidates: Releasing SHARED lock on candidates.dat.\n");
        fflush(stdout);
        flock(fd, LOCK_UN);
        fclose(fp);
        
        log_event("INFO", "Sent candidate list (%d candidates) to a client.", count);
    } else {
        printf("[TRACE] View Candidates: candidates.dat not found or could not be opened.\n");
        fflush(stdout);
    }
    
    printf("[TRACE] View Candidates: Sending candidate count (%d) to client.\n", count);
    fflush(stdout);
    udp_send(sock, &count, sizeof(int), cli);
    if (count > 0) {
        printf("[TRACE] View Candidates: Sending %d candidate records (%zu bytes) to client.\n", count, sizeof(Candidate) * count);
        fflush(stdout);
        udp_send(sock, all, sizeof(Candidate) * count, cli);
    }
}