#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include "server.h"

// --- UDP Network Helpers ---
void udp_send(int sock, const void *msg, size_t len, struct sockaddr_in *dest) {
    printf("[TRACE] UDP SEND: Sending %zu bytes to %s:%d (PID: %d)\n", len, inet_ntoa(dest->sin_addr), ntohs(dest->sin_port), getpid());
    fflush(stdout);
    ssize_t sent = sendto(sock, msg, len, 0, (struct sockaddr*)dest, sizeof(*dest));
    printf("[TRACE] UDP SEND: %zd bytes sent successfully.\n", sent);
    fflush(stdout);
}

void udp_recv(int sock, void *buf, size_t len, struct sockaddr_in *src) {
    socklen_t slen = sizeof(*src);
    printf("[TRACE] UDP RECV: Waiting to receive up to %zu bytes (PID: %d)...\n", len, getpid());
    fflush(stdout);
    ssize_t received = recvfrom(sock, buf, len, 0, (struct sockaddr*)src, &slen);
    printf("[TRACE] UDP RECV: Received %zd bytes from %s:%d\n", received, inet_ntoa(src->sin_addr), ntohs(src->sin_port));
    fflush(stdout);
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    int sockfd;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t len = sizeof(cliaddr);

    // Prevent child processes from becoming zombies
    signal(SIGCHLD, SIG_IGN); 

    printf("[TRACE] Creating UDP socket...\n");
    fflush(stdout);
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY; 
    servaddr.sin_port = htons(PORT);

    printf("[TRACE] Binding socket to port %d...\n", PORT);
    fflush(stdout);
    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("Bind failed");
        exit(1);
    }

    printf("--- SONU UDP Server Running on Port %d ---\n", PORT);
    log_event("START", "SONU UDP Server started on port %d", PORT);

    while (1) {
        int cmd;
        
        printf("[TRACE] Parent (PID: %d) waiting for incoming command...\n", getpid());
        fflush(stdout);
        if (recvfrom(sockfd, &cmd, sizeof(int), 0, (struct sockaddr *)&cliaddr, &len) > 0) {
            
            printf("[TRACE] Incoming packet detected! Command code: %d from %s:%d. Forking new process...\n", cmd, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
            fflush(stdout);

            pid_t pid = fork();

            if (pid < 0) {
                perror("Fork failed");
            } 
            else if (pid == 0) {
                // CHILD PROCESS
                printf("[TRACE] Child process (PID: %d) spawned for command %d. Sleeping for 2s...\n", getpid(), cmd);
                fflush(stdout);
                sleep(2); // Simulated processing delay

                switch(cmd) {
                    case CMD_REG_VOTER:       
                        server_register_voter(sockfd, &cliaddr); 
                        break;
                    case CMD_REG_CANDIDATE:   
                        server_register_candidate(sockfd, &cliaddr); 
                        break;
                    case CMD_VIEW_CANDIDATES: 
                        server_view_candidates(sockfd, &cliaddr); 
                        break;
                    case CMD_CAST_VOTE:       
                        server_cast_vote(sockfd, &cliaddr); 
                        break;
                    case CMD_VIEW_TALLY:      
                        server_send_large_text(sockfd, &cliaddr, 1); 
                        break;
                    case CMD_RESULTS:         
                        server_send_large_text(sockfd, &cliaddr, 0); 
                        break;
                    default:
                        log_event("WARN", "Unknown command code received: %d", cmd);
                        break;
                }

                printf("[TRACE] Child process (PID: %d) complete. Terminating process.\n", getpid());
                fflush(stdout);
                exit(0); // CRITICAL: Child must exit so it doesn't loop back to recvfrom
            }
            // PARENT PROCESS continues immediately to the next loop iteration to listen for new packets
            printf("[TRACE] Parent (PID: %d) forked child (PID: %d). Resuming listen loop.\n", getpid(), pid);
            fflush(stdout);
        }
    }
    close(sockfd);
    return 0;
}