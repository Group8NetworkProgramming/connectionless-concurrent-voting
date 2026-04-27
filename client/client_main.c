#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include "../shared/protocol.h"
#include "../shared/candidate.h"

typedef struct {
    char student_id[15];
    char name[50];
    int  has_voted; 
    int  votes_cast;
} Voter;

#define PORT 8080
char SERVER_IP[50];

// --- UDP Network Helpers ---
void udp_send(int sock, const void *msg, size_t len, struct sockaddr_in *dest) {
    sendto(sock, msg, len, 0, (struct sockaddr*)dest, sizeof(*dest));
}

void udp_recv(int sock, void *buf, size_t len, struct sockaddr_in *src) {
    socklen_t slen = sizeof(*src);
    recvfrom(sock, buf, len, 0, (struct sockaddr*)src, &slen);
}

void send_cmd(int sock, int cmd, struct sockaddr_in *serv) {
    udp_send(sock, &cmd, sizeof(int), serv);
}

void clear_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <server_ip>\n", argv[0]);
        return 1;
    }
    strncpy(SERVER_IP, argv[1], 49);

    int sockfd;
    struct sockaddr_in servaddr;

    // --- STRICTLY UDP (SOCK_DGRAM) ---
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &servaddr.sin_addr);

    // Timeout to prevent freezing if a UDP packet is lost
    struct timeval tv;
    tv.tv_sec = 5; 
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int choice;
    while (1) {
        printf("\n=============================\n");
        printf("🗳️  SONU VOTING SYSTEM (UDP)\n");
        printf("=============================\n");
        printf("1. Register Voter\n");
        printf("2. Register Candidate\n");
        printf("3. View Candidates\n");
        printf("4. Cast Vote\n");
        printf("5. View Vote Tally\n");
        printf("6. Announce Results\n");
        printf("7. Exit\n");
        printf("Choice: ");
        
        if (scanf("%d", &choice) != 1) {
            clear_buffer(); 
            continue;
        }

        switch(choice) {
            case CMD_REG_VOTER: {
                send_cmd(sockfd, CMD_REG_VOTER, &servaddr);
                Voter v;
                printf("\n--- SONU Voter Registration ---\nEnter Student ID: ");
                scanf("%49s", v.student_id);
                clear_buffer();
                printf("Enter Full Name: ");
                fgets(v.name, 50, stdin);
                v.name[strcspn(v.name, "\n")] = 0;

                udp_send(sockfd, &v, sizeof(Voter), &servaddr);
                char response[256];
                udp_recv(sockfd, response, sizeof(response), &servaddr);
                printf("\n[SERVER]: %s\n", response);
                break;
            }
            case CMD_REG_CANDIDATE: {
                send_cmd(sockfd, CMD_REG_CANDIDATE, &servaddr);
                Candidate c;
                printf("\n--- SONU Candidate Registration ---\nEnter Student ID: ");
                scanf("%49s", c.student_id);
                clear_buffer();
                printf("Enter Full Name: ");
                fgets(c.name, 50, stdin);
                c.name[strcspn(c.name, "\n")] = 0;

                int pos;
                printf("\nAvailable SONU Positions:\n");
                for(int i = 1; i <= NUM_POSITIONS; i++) {
                    printf("%d. %s\n", i, SONU_POSITIONS[i]);
                }
                
                while(1) {
                    printf("Select Position (1 - %d): ", NUM_POSITIONS);
                    if (scanf("%d", &pos) == 1 && pos >= 1 && pos <= NUM_POSITIONS) {
                        c.position_index = pos;
                        break;
                    }
                    clear_buffer(); 
                    printf("[ERROR] Invalid choice.\n");
                }

                udp_send(sockfd, &c, sizeof(Candidate), &servaddr);
                char response[256];
                udp_recv(sockfd, response, sizeof(response), &servaddr);
                printf("\n[SERVER]: %s\n", response);
                break;
            }
            case CMD_VIEW_CANDIDATES: {
                send_cmd(sockfd, CMD_VIEW_CANDIDATES, &servaddr);
                int count;
                udp_recv(sockfd, &count, sizeof(int), &servaddr);
                
                if (count == 0) {
                    printf("\n[SERVER]: No candidates registered yet.\n");
                    break;
                }
                
                Candidate all[200];
                udp_recv(sockfd, all, sizeof(Candidate) * count, &servaddr);

                printf("\n============================= Registered Candidates =============================\n");
                printf("%-15s | %-30s | %-40s | %-5s\n", "Student ID", "Name", "Position", "Votes");
                printf("---------------------------------------------------------------------------------\n");
                for (int i = 0; i < count; i++) {
                    printf("%-15s | %-30s | %-40s | %-5d\n", all[i].student_id, all[i].name, SONU_POSITIONS[all[i].position_index], all[i].votes);
                }
                printf("=================================================================================\n");
                break;
            }
            case CMD_CAST_VOTE: {
                send_cmd(sockfd, CMD_CAST_VOTE, &servaddr);
                
                char voter_id[MAX_ID];
                printf("\n--- SONU Vote Casting ---\nEnter Your Student ID: ");
                scanf("%49s", voter_id);
                
                int count;
                udp_recv(sockfd, &count, sizeof(int), &servaddr);
                
                if (count == 0) {
                    printf("[SERVER]: No candidates registered yet.\n");
                    break;
                }
                
                Candidate all[200];
                udp_recv(sockfd, all, sizeof(Candidate) * count, &servaddr);
                
                printf("\nAvailable Candidates:\n");
                printf("%-3s | %-15s | %-30s | %-40s\n", "#", "Student ID", "Name", "Position");
                printf("-----|-----------------|-----|-----|--------|----------------------------------\n");
                for (int i = 0; i < count; i++) {
                    printf("%-3d | %-15s | %-30s | %-40s\n", i+1, all[i].student_id, all[i].name, SONU_POSITIONS[all[i].position_index]);
                }
                
                int cand_choice;
                while(1) {
                    printf("\nSelect Candidate (1 - %d): ", count);
                    if (scanf("%d", &cand_choice) == 1 && cand_choice >= 1 && cand_choice <= count) {
                        break;
                    }
                    clear_buffer();
                    printf("[ERROR] Invalid choice.\n");
                }
                
                struct {
                    char voter_id[MAX_ID];
                    char candidate_id[MAX_ID];
                } vote;
                
                strcpy(vote.voter_id, voter_id);
                strcpy(vote.candidate_id, all[cand_choice-1].student_id);
                
                udp_send(sockfd, &vote, sizeof(vote), &servaddr);
                char response[256];
                udp_recv(sockfd, response, sizeof(response), &servaddr);
                printf("\n[SERVER]: %s\n", response);
                break;
            }
            case CMD_VIEW_TALLY:
            case CMD_RESULTS: {
                send_cmd(sockfd, choice, &servaddr);
                static char buffer[16384];
                udp_recv(sockfd, buffer, sizeof(buffer), &servaddr);
                printf("%s", buffer);
                break;
            }
            case CMD_QUIT:
                printf("Exiting application. Goodbye!\n");
                close(sockfd);
                exit(0);
            default:
                printf("Invalid option.\n");
        }
    }
    return 0;
}