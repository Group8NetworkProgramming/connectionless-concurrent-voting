
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>
#include "server.h"
#include "../shared/candidate.h"

void server_register_candidate(int sock, struct sockaddr_in *cli) {
    Candidate c;
    udp_recv(sock, &c, sizeof(Candidate), cli);
    char msg[256];

    if (is_id_registered(c.student_id, "candidates.dat")) {
        strcpy(msg, "Error: Candidate ID already registered.");
        log_event("WARN", "Candidate registration failed: %s", c.student_id);
    } else {
        FILE *fp = fopen("candidates.dat", "ab+");
        if (fp) {
            printf("[CHILD %d] Locking candidates.dat...\n", getpid());
            flock(fileno(fp), LOCK_EX);

            printf("[CHILD %d] Writing candidate...\n", getpid());
            c.votes = 0;
            fwrite(&c, sizeof(Candidate), 1, fp);

            fflush(fp);
            flock(fileno(fp), LOCK_UN);
            printf("[CHILD %d] Unlocking candidates.dat...\n", getpid());

            fclose(fp);
            strcpy(msg, "Candidate registered!");
        } else {
            strcpy(msg, "Server DB Error.");
        }
    }
    udp_send(sock, msg, sizeof(msg), cli);
}
