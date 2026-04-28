#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include "../shared/voter.h"
#include "server.h"

// Voter data is now passed in — already received by the parent before fork().
// This child process never calls recvfrom().
void server_register_voter(int sock, struct sockaddr_in *cli, Voter *v) {
    printf("[TRACE] Voter Registration (PID: %d): Processing voter ID '%s', name '%s'.\n",
           getpid(), v->student_id, v->name);
    fflush(stdout);
    char msg[256];

    // --- Check for duplicate ID ---
    printf("[TRACE] Voter Registration (PID: %d): CPU Processing — checking if ID '%s' is already registered...\n",
           getpid(), v->student_id);
    fflush(stdout);
    if (is_id_registered(v->student_id, "voters.dat")) {
        strcpy(msg, "Error: Voter ID already registered.");
        log_event("WARN", "Voter registration failed: Student ID '%s' is already registered.", v->student_id);
    } else {
        // --- Open file for writing ---
        printf("[TRACE] Voter Registration (PID: %d): ID '%s' not found. Opening voters.dat for writing...\n",
               getpid(), v->student_id);
        fflush(stdout);
        FILE *fp = fopen("voters.dat", "ab+");
        if (fp) {
            int fd = fileno(fp);

            // --- Acquire exclusive lock ---
            printf("[TRACE] Voter Registration (PID: %d): Acquiring EXCLUSIVE lock on voters.dat...\n", getpid());
            fflush(stdout);
            flock(fd, LOCK_EX);
            printf("[TRACE] Voter Registration (PID: %d): EXCLUSIVE lock acquired.\n", getpid());
            fflush(stdout);

            // --- CPU processing: initialise fields ---
            printf("[TRACE] Voter Registration (PID: %d): CPU Processing — initialising voter fields (has_voted=0, votes_cast=0)...\n", getpid());
            fflush(stdout);
            v->has_voted = 0;
            v->votes_cast = 0;

            // --- File write ---
            printf("[TRACE] Voter Registration (PID: %d): File Write — writing voter record (%zu bytes) to voters.dat...\n",
                   getpid(), sizeof(Voter));
            fflush(stdout);
            fwrite(v, sizeof(Voter), 1, fp);

            // --- Release lock ---
            printf("[TRACE] Voter Registration (PID: %d): Releasing EXCLUSIVE lock on voters.dat.\n", getpid());
            fflush(stdout);
            flock(fd, LOCK_UN);
            fclose(fp);

            strcpy(msg, "Voter Registered Successfully!");
            log_event("SUCCESS", "Registered voter '%s' (ID: %s).", v->name, v->student_id);
        } else {
            strcpy(msg, "Server DB Error.");
            printf("[TRACE] Voter Registration (PID: %d): FAILED to open voters.dat!\n", getpid());
            fflush(stdout);
            log_event("ERROR", "Failed to open voters.dat to register ID '%s'.", v->student_id);
        }
    }

    // --- Send response ---
    printf("[TRACE] Voter Registration (PID: %d): Sending response to client: '%s'\n", getpid(), msg);
    fflush(stdout);
    udp_send(sock, msg, sizeof(msg), cli);
}
