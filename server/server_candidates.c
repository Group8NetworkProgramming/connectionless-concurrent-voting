#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include "server.h"
#include "../shared/candidate.h"

// Candidate data is now passed in — already received by the parent before fork().
void server_register_candidate(int sock, struct sockaddr_in *cli, Candidate *c) {
    printf("[TRACE] Candidate Registration (PID: %d): Processing candidate ID '%s', name '%s', position %d.\n",
           getpid(), c->student_id, c->name, c->position_index);
    fflush(stdout);
    char msg[256];

    // --- Check for duplicate ID ---
    printf("[TRACE] Candidate Registration (PID: %d): CPU Processing — checking if ID '%s' is already registered...\n",
           getpid(), c->student_id);
    fflush(stdout);
    if (is_id_registered(c->student_id, "candidates.dat")) {
        strcpy(msg, "Error: Candidate ID already registered.");
        log_event("WARN", "Candidate registration failed: '%s' already registered.", c->student_id);
    } else {
        // --- Open file for writing ---
        printf("[TRACE] Candidate Registration (PID: %d): ID '%s' not found. Opening candidates.dat for writing...\n",
               getpid(), c->student_id);
        fflush(stdout);
        FILE *fp = fopen("candidates.dat", "ab+");
        if (fp) {
            int fd = fileno(fp);

            // --- Acquire exclusive lock ---
            printf("[TRACE] Candidate Registration (PID: %d): Acquiring EXCLUSIVE lock on candidates.dat...\n", getpid());
            fflush(stdout);
            flock(fd, LOCK_EX);
            printf("[TRACE] Candidate Registration (PID: %d): EXCLUSIVE lock acquired.\n", getpid());
            fflush(stdout);

            // --- CPU processing: initialise vote count ---
            printf("[TRACE] Candidate Registration (PID: %d): CPU Processing — initialising votes to 0...\n", getpid());
            fflush(stdout);
            c->votes = 0;

            // --- File write ---
            printf("[TRACE] Candidate Registration (PID: %d): File Write — writing candidate record (%zu bytes) to candidates.dat...\n",
                   getpid(), sizeof(Candidate));
            fflush(stdout);
            fwrite(c, sizeof(Candidate), 1, fp);

            // --- Release lock ---
            printf("[TRACE] Candidate Registration (PID: %d): Releasing EXCLUSIVE lock on candidates.dat.\n", getpid());
            fflush(stdout);
            flock(fd, LOCK_UN);
            fclose(fp);

            snprintf(msg, sizeof(msg), "Candidate registered!");
            log_event("SUCCESS", "Registered candidate '%s' (ID: %s) for position index %d.", c->name, c->student_id, c->position_index);
        } else {
            strcpy(msg, "Server DB Error.");
            printf("[TRACE] Candidate Registration (PID: %d): FAILED to open candidates.dat!\n", getpid());
            fflush(stdout);
            log_event("ERROR", "Failed to open candidates.dat.");
        }
    }

    // --- Send response ---
    printf("[TRACE] Candidate Registration (PID: %d): Sending response to client: '%s'\n", getpid(), msg);
    fflush(stdout);
    udp_send(sock, msg, sizeof(msg), cli);
}

void server_view_candidates(int sock, struct sockaddr_in *cli) {
    // No follow-up data needed — this is a read-only query
    printf("[TRACE] View Candidates (PID: %d): Opening candidates.dat for reading...\n", getpid());
    fflush(stdout);
    FILE *fp = fopen("candidates.dat", "rb");
    Candidate all[200];
    int count = 0;

    if (fp) {
        int fd = fileno(fp);

        // --- Acquire shared lock ---
        printf("[TRACE] View Candidates (PID: %d): Acquiring SHARED lock on candidates.dat...\n", getpid());
        fflush(stdout);
        flock(fd, LOCK_SH);
        printf("[TRACE] View Candidates (PID: %d): SHARED lock acquired.\n", getpid());
        fflush(stdout);

        // --- File read ---
        printf("[TRACE] View Candidates (PID: %d): File Read — reading candidate records from disk...\n", getpid());
        fflush(stdout);
        while (fread(&all[count], sizeof(Candidate), 1, fp)) count++;
        printf("[TRACE] View Candidates (PID: %d): Read %d candidate records.\n", getpid(), count);
        fflush(stdout);

        // --- Release lock ---
        printf("[TRACE] View Candidates (PID: %d): Releasing SHARED lock on candidates.dat.\n", getpid());
        fflush(stdout);
        flock(fd, LOCK_UN);
        fclose(fp);

        log_event("INFO", "Sent candidate list (%d candidates) to a client.", count);
    } else {
        printf("[TRACE] View Candidates (PID: %d): candidates.dat not found or empty.\n", getpid());
        fflush(stdout);
    }

    // --- Send count ---
    printf("[TRACE] View Candidates (PID: %d): Sending candidate count (%d) to client.\n", getpid(), count);
    fflush(stdout);
    udp_send(sock, &count, sizeof(int), cli);

    // --- Send records ---
    if (count > 0) {
        printf("[TRACE] View Candidates (PID: %d): Sending %d candidate records (%zu bytes) to client.\n",
               getpid(), count, sizeof(Candidate) * count);
        fflush(stdout);
        udp_send(sock, all, sizeof(Candidate) * count, cli);
    }
}
