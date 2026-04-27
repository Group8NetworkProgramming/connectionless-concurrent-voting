#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>   
#include "../shared/voter.h"
#include "server.h" 

void server_register_voter(int sock, struct sockaddr_in *cli) {
    Voter v;
    printf("[TRACE] Voter Registration: Waiting to receive voter data (PID: %d)...\n", getpid());
    fflush(stdout);
    udp_recv(sock, &v, sizeof(Voter), cli);
    printf("[TRACE] Voter Registration: Received data for student ID '%s'.\n", v.student_id);
    fflush(stdout);
    char msg[256];
    
    printf("[TRACE] Voter Registration: Checking if ID '%s' is already registered...\n", v.student_id);
    fflush(stdout);
    if (is_id_registered(v.student_id, "voters.dat")) {
        strcpy(msg, "Error: Voter ID already registered.");
        log_event("WARN", "Voter registration failed: Student ID '%s' is already registered.", v.student_id);
    } else {
        printf("[TRACE] Voter Registration: ID '%s' not found. Opening voters.dat for writing...\n", v.student_id);
        fflush(stdout);
        FILE *fp = fopen("voters.dat", "ab+");
        if (fp) {
            int fd = fileno(fp);
            
            printf("[TRACE] Attempting to acquire EXCLUSIVE lock on voters.dat (PID: %d)...\n", getpid());
            fflush(stdout);
            flock(fd, LOCK_EX); // Lock the file for writing
            
            v.has_voted = 0; v.votes_cast = 0;
            
            printf("[TRACE] Lock acquired. Writing voter record to disk...\n");
            fflush(stdout);
            fwrite(&v, sizeof(Voter), 1, fp);
            
            flock(fd, LOCK_UN); // Unlock the file
            fclose(fp);
            
            strcpy(msg, "Voter Registered Successfully!");
            log_event("SUCCESS", "Registered voter '%s' (ID: %s).", v.name, v.student_id);
        } else {
            strcpy(msg, "Server DB Error.");
            log_event("ERROR", "Failed to open voters.dat to register ID '%s'.", v.student_id);
        }
    }
    printf("[TRACE] Voter Registration: Sending response to client: '%s'\n", msg);
    fflush(stdout);
    udp_send(sock, msg, sizeof(msg), cli);
}