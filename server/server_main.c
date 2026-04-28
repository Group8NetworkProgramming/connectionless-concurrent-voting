#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include "../shared/voter.h"
#include "../shared/candidate.h"
#include "../shared/protocol.h"
#include "server.h"

// --- UDP Network Helpers ---
void udp_send(int sock, const void *msg, size_t len, struct sockaddr_in *dest) {
    printf("[TRACE] UDP SEND: Sending %zu bytes to %s:%d (PID: %d)\n",
           len, inet_ntoa(dest->sin_addr), ntohs(dest->sin_port), getpid());
    fflush(stdout);
    ssize_t sent = sendto(sock, msg, len, 0, (struct sockaddr*)dest, sizeof(*dest));
    printf("[TRACE] UDP SEND: %zd bytes sent successfully.\n", sent);
    fflush(stdout);
}

ssize_t udp_recv(int sock, void *buf, size_t len, struct sockaddr_in *src) {
    socklen_t slen = sizeof(*src);
    printf("[TRACE] UDP RECV: Waiting to receive up to %zu bytes (PID: %d)...\n", len, getpid());
    fflush(stdout);
    ssize_t received = recvfrom(sock, buf, len, 0, (struct sockaddr*)src, &slen);
    printf("[TRACE] UDP RECV: Received %zd bytes from %s:%d\n",
           received, inet_ntoa(src->sin_addr), ntohs(src->sin_port));
    fflush(stdout);
    return received;
}

// --- Zombie prevention via SIGCHLD handler ---
static void sigchld_handler(int sig) {
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    int sockfd;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t len = sizeof(cliaddr);

    // Register SIGCHLD handler to reap child processes and prevent zombies
    printf("[TRACE] Registering SIGCHLD handler to auto-reap child processes...\n");
    fflush(stdout);
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    // --- Create UDP socket ---
    printf("[TRACE] Creating UDP socket (SOCK_DGRAM)...\n");
    fflush(stdout);
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(1);
    }
    printf("[TRACE] UDP socket created successfully (fd: %d).\n", sockfd);
    fflush(stdout);

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    // --- Bind socket ---
    printf("[TRACE] Binding socket to port %d...\n", PORT);
    fflush(stdout);
    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("Bind failed");
        exit(1);
    }
    printf("[TRACE] Socket bound successfully to port %d.\n", PORT);
    fflush(stdout);

    printf("--- SONU UDP Server Running on Port %d (PID: %d) ---\n", PORT, getpid());
    log_event("START", "SONU UDP Server started on port %d (PID: %d)", PORT, getpid());

    // ===== MAIN LOOP =====
    while (1) {
        int cmd;

        printf("[TRACE] Parent (PID: %d): Waiting for incoming command packet...\n", getpid());
        fflush(stdout);

        // STEP 1: Parent receives the command code
        ssize_t bytes = recvfrom(sockfd, &cmd, sizeof(int), 0,
                                 (struct sockaddr *)&cliaddr, &len);

        if (bytes <= 0) continue;

        printf("[TRACE] Parent (PID: %d): Received command code %d from %s:%d (%zd bytes).\n",
               getpid(), cmd, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port), bytes);
        fflush(stdout);

        // ================================================================
        // STEP 2: Parent receives ALL follow-up data BEFORE forking.
        // This prevents the race condition where both parent and child
        // compete for the next packet on the shared UDP socket.
        // ================================================================

        // Buffers to hold follow-up data — filled by parent, inherited by child
        Voter voter_data;
        Candidate candidate_data;
        struct { char voter_id[MAX_ID]; char candidate_id[MAX_ID]; } vote_data;

        switch (cmd) {
            case CMD_REG_VOTER:
                printf("[TRACE] Parent (PID: %d): Command is REG_VOTER. Receiving voter data before fork...\n", getpid());
                fflush(stdout);
                udp_recv(sockfd, &voter_data, sizeof(Voter), &cliaddr);
                printf("[TRACE] Parent (PID: %d): Voter data received (ID: '%s', Name: '%s'). Ready to fork.\n",
                       getpid(), voter_data.student_id, voter_data.name);
                fflush(stdout);
                break;

            case CMD_REG_CANDIDATE:
                printf("[TRACE] Parent (PID: %d): Command is REG_CANDIDATE. Receiving candidate data before fork...\n", getpid());
                fflush(stdout);
                udp_recv(sockfd, &candidate_data, sizeof(Candidate), &cliaddr);
                printf("[TRACE] Parent (PID: %d): Candidate data received (ID: '%s', Name: '%s'). Ready to fork.\n",
                       getpid(), candidate_data.student_id, candidate_data.name);
                fflush(stdout);
                break;

            case CMD_CAST_VOTE:
                printf("[TRACE] Parent (PID: %d): Command is CAST_VOTE. Receiving vote data before fork...\n", getpid());
                fflush(stdout);
                udp_recv(sockfd, &vote_data, sizeof(vote_data), &cliaddr);
                printf("[TRACE] Parent (PID: %d): Vote data received (Voter: '%s', Candidate: '%s'). Ready to fork.\n",
                       getpid(), vote_data.voter_id, vote_data.candidate_id);
                fflush(stdout);
                break;

            case CMD_VIEW_CANDIDATES:
            case CMD_VIEW_TALLY:
            case CMD_RESULTS:
                // These commands have no follow-up data — just the command code
                printf("[TRACE] Parent (PID: %d): Command %d requires no follow-up data. Ready to fork.\n", getpid(), cmd);
                fflush(stdout);
                break;

            default:
                printf("[TRACE] Parent (PID: %d): Unknown command %d. Will fork and log.\n", getpid(), cmd);
                fflush(stdout);
                break;
        }

        // STEP 3: Flush stdout before fork to prevent duplicated output
        printf("[TRACE] Parent (PID: %d): All data received. Flushing stdout before fork()...\n", getpid());
        fflush(stdout);

        // STEP 4: Fork child process
        printf("[TRACE] Parent (PID: %d): Forking child process to handle command %d...\n", getpid(), cmd);
        fflush(stdout);

        pid_t pid = fork();

        if (pid < 0) {
            perror("Fork failed");
            printf("[TRACE] Parent (PID: %d): fork() FAILED for command %d.\n", getpid(), cmd);
            fflush(stdout);
            continue;
        }
        else if (pid == 0) {
            // ==================== CHILD PROCESS ====================
            printf("[TRACE] Child (PID: %d): Process spawned for command %d.\n", getpid(), cmd);
            fflush(stdout);

            // sleep(2) makes concurrency visible: while this child sleeps,
            // the parent is already back at recvfrom() and can accept another
            // request simultaneously.
            printf("[TRACE] Child (PID: %d): Simulating processing delay (sleep 2s)...\n", getpid());
            fflush(stdout);
            sleep(2);

            printf("[TRACE] Child (PID: %d): Processing request now. Dispatching handler...\n", getpid());
            fflush(stdout);

            // The child uses data that was already received by the parent
            // and inherited through fork(). No recvfrom() calls here.
            switch (cmd) {
                case CMD_REG_VOTER:
                    printf("[TRACE] Child (PID: %d): Dispatching server_register_voter() with pre-received data.\n", getpid());
                    fflush(stdout);
                    server_register_voter(sockfd, &cliaddr, &voter_data);
                    break;

                case CMD_REG_CANDIDATE:
                    printf("[TRACE] Child (PID: %d): Dispatching server_register_candidate() with pre-received data.\n", getpid());
                    fflush(stdout);
                    server_register_candidate(sockfd, &cliaddr, &candidate_data);
                    break;

                case CMD_VIEW_CANDIDATES:
                    printf("[TRACE] Child (PID: %d): Dispatching server_view_candidates() (no follow-up data needed).\n", getpid());
                    fflush(stdout);
                    server_view_candidates(sockfd, &cliaddr);
                    break;

                case CMD_CAST_VOTE:
                    printf("[TRACE] Child (PID: %d): Dispatching server_cast_vote() with pre-received vote data.\n", getpid());
                    fflush(stdout);
                    server_cast_vote(sockfd, &cliaddr, &vote_data);
                    break;

                case CMD_VIEW_TALLY:
                    printf("[TRACE] Child (PID: %d): Dispatching server_send_large_text(tally).\n", getpid());
                    fflush(stdout);
                    server_send_large_text(sockfd, &cliaddr, 1);
                    break;

                case CMD_RESULTS:
                    printf("[TRACE] Child (PID: %d): Dispatching server_send_large_text(results).\n", getpid());
                    fflush(stdout);
                    server_send_large_text(sockfd, &cliaddr, 0);
                    break;

                default:
                    printf("[TRACE] Child (PID: %d): Unknown command %d. Logging and exiting.\n", getpid(), cmd);
                    fflush(stdout);
                    log_event("WARN", "Unknown command code received: %d", cmd);
                    break;
            }

            printf("[TRACE] Child (PID: %d): Request complete. Closing socket and terminating.\n", getpid());
            fflush(stdout);
            close(sockfd);
            exit(0); // CRITICAL: Child must exit so it doesn't loop back to recvfrom
        }

        // ==================== PARENT PROCESS ====================
        printf("[TRACE] Parent (PID: %d): Child (PID: %d) forked successfully. Resuming listen loop.\n\n", getpid(), pid);
        fflush(stdout);
        // Parent does NOT wait — SIGCHLD handler reaps children asynchronously
    }

    close(sockfd);
    return 0;
}
