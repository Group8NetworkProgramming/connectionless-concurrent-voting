#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "server.h"

// --- UDP Network Helpers ---
void udp_send(int sock, const void *msg, size_t len, struct sockaddr_in *dest) {
    sendto(sock, msg, len, 0, (struct sockaddr*)dest, sizeof(*dest));
}

void udp_recv(int sock, void *buf, size_t len, struct sockaddr_in *src) {
    socklen_t slen = sizeof(*src);
    recvfrom(sock, buf, len, 0, (struct sockaddr*)src, &slen);
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    int sockfd;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t len = sizeof(cliaddr);

    // 1. Create the UDP socket (SOCK_DGRAM)
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    // 2. Configure the server address structure
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY; // Listen on all network interfaces
    servaddr.sin_port = htons(PORT);

    // 3. Bind the socket to the port
    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("Bind failed");
        exit(1);
    }

    printf("--- SONU UDP Server Running on Port %d ---\n", PORT);
    
    // Log the successful server boot
    log_event("START", "SONU UDP Server started on port %d", PORT);

    // 4. The Infinite Server Loop
    while (1) {
        int cmd;
        
        // Wait for an incoming integer command from any client
        if (recvfrom(sockfd, &cmd, sizeof(int), 0, (struct sockaddr *)&cliaddr, &len) > 0) {
            
            // Route the command to the correct handler function
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
        }
    }
    
    // Clean up (This is rarely reached in a while(1) daemon, but good practice)
    close(sockfd);
    return 0;
}