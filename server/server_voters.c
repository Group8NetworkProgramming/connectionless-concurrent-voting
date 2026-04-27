
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>
#include "server.h"
#include "../shared/voter.h"

void server_register_voter(int sock, struct sockaddr_in *cli) {
    Voter v;
    udp_recv(sock, &v, sizeof(Voter), cli);
    char msg[256];

    if (is_id_registered(v.student_id, "voters.dat")) {
        strcpy(msg, "Error: Voter ID already registered.");
    } else {
        FILE *fp = fopen("voters.dat", "ab+");
        if (fp) {
            printf("[CHILD %d] Locking voters.dat...\n", getpid());
            flock(fileno(fp), LOCK_EX);

            printf("[CHILD %d] Writing voter...\n", getpid());
            v.has_voted = 0;
            v.votes_cast = 0;
            fwrite(&v, sizeof(Voter), 1, fp);

            fflush(fp);
            flock(fileno(fp), LOCK_UN);
            printf("[CHILD %d] Unlocking voters.dat...\n", getpid());

            fclose(fp);
            strcpy(msg, "Voter Registered Successfully!");
        } else {
            strcpy(msg, "Server DB Error.");
        }
    }
    udp_send(sock, msg, sizeof(msg), cli);
}
