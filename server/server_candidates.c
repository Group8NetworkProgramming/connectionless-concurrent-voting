#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include "server.h"
#include "../shared/candidate.h"

void server_register_candidate(int sock, struct sockaddr_in *cli) {
    Candidate c;
    udp_recv(sock, &c, sizeof(Candidate), cli);
    char msg[256];
    
    if (is_id_registered(c.student_id, "candidates.dat")) {
        strcpy(msg, "Error: Candidate ID already registered.");
        log_event("WARN", "Candidate registration failed: '%s' registered.", c.student_id);
    } else {
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
    udp_send(sock, msg, sizeof(msg), cli);
}

void server_view_candidates(int sock, struct sockaddr_in *cli) {
    FILE *fp = fopen("candidates.dat", "rb");
    Candidate all[200];
    int count = 0;
    
    if (fp) {
        int fd = fileno(fp);
        printf("[TRACE] Acquiring SHARED lock on candidates.dat for viewing (PID: %d)...\n", getpid());
        fflush(stdout);
        flock(fd, LOCK_SH);
        
        while (fread(&all[count], sizeof(Candidate), 1, fp)) count++;
        
        flock(fd, LOCK_UN);
        fclose(fp);
        
        log_event("INFO", "Sent candidate list (%d candidates) to a client.", count);
    }
    
    udp_send(sock, &count, sizeof(int), cli);
    if (count > 0) udp_send(sock, all, sizeof(Candidate) * count, cli);
}