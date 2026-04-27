#include <stdio.h>
#include <string.h>
#include "server.h"
#include "../shared/candidate.h"
#include <unistd.h>
#include <sys/file.h>

typedef struct {
    char student_id[15];
    char name[50];
    int  has_voted; 
    int  votes_cast;
} Voter;

void server_cast_vote(int sock, struct sockaddr_in *cli) {
    // ... [Code to send candidate list to client remains unchanged] ...
    
    struct { char voter_id[MAX_ID]; char candidate_id[MAX_ID]; } vote;
    udp_recv(sock, &vote, sizeof(vote), cli);
    char msg[256];
    
    // ... [Voter Check logic remains mostly unchanged, but add a SHARED lock when reading]
    FILE *vfp = fopen("voters.dat", "rb");
    int voter_found = 0, voter_voted = 0;
    if (vfp) {
        flock(fileno(vfp), LOCK_SH);
        // ... [fread loop] ...
        flock(fileno(vfp), LOCK_UN);
        fclose(vfp);
    }
    
    if (!voter_found) {
        strcpy(msg, "Error: Voter not registered.");
    } else if (voter_voted) {
        strcpy(msg, "Error: Voter has already cast a vote.");
    } else {
        int candidate_found = 0;
        FILE *cfp = fopen("candidates.dat", "r+b");
        if (cfp) {
            printf("[TRACE] Voting Process: Acquiring EXCLUSIVE lock on candidates.dat (PID: %d)\n", getpid());
            fflush(stdout);
            flock(fileno(cfp), LOCK_EX);
            
            Candidate temp;
            while (fread(&temp, sizeof(Candidate), 1, cfp)) {
                if (strcmp(temp.student_id, vote.candidate_id) == 0) {
                    candidate_found = 1; temp.votes++;
                    fseek(cfp, -(long)sizeof(Candidate), SEEK_CUR);
                    
                    printf("[TRACE] CPU Processing: Incrementing candidate vote count...\n");
                    fflush(stdout);
                    fwrite(&temp, sizeof(Candidate), 1, cfp);
                    snprintf(msg, sizeof(msg), "Vote cast for %s in the %s position!", temp.name, SONU_POSITIONS[temp.position_index]);
                    break;
                }
            }
            flock(fileno(cfp), LOCK_UN);
            fclose(cfp);
        }
        
        if (candidate_found) {
            FILE *vfp2 = fopen("voters.dat", "r+b");
            if (vfp2) {
                printf("[TRACE] Voting Process: Acquiring EXCLUSIVE lock on voters.dat (PID: %d)\n", getpid());
                fflush(stdout);
                flock(fileno(vfp2), LOCK_EX);
                
                Voter temp;
                while (fread(&temp, sizeof(Voter), 1, vfp2)) {
                    if (strcmp(temp.student_id, vote.voter_id) == 0) {
                        temp.has_voted = 1;
                        fseek(vfp2, -(long)sizeof(Voter), SEEK_CUR);
                        
                        printf("[TRACE] CPU Processing: Marking voter as 'has_voted'...\n");
                        fflush(stdout);
                        fwrite(&temp, sizeof(Voter), 1, vfp2);
                        break;
                    }
                }
                flock(fileno(vfp2), LOCK_UN);
                fclose(vfp2);
            }
        } else {
            strcpy(msg, "Error: Candidate not found.");
        }
    }
    udp_send(sock, msg, sizeof(msg), cli);
}

void server_send_large_text(int sock, struct sockaddr_in *cli, int is_tally) {
    static char buffer[16384]; 
    buffer[0] = '\0';
    
    FILE *fp = fopen("candidates.dat", "rb");
    if (!fp) {
        strcpy(buffer, "\nNo candidates database found.\n");
        udp_send(sock, buffer, sizeof(buffer), cli);
        // --- NEW LOG ---
        log_event("ERROR", "Failed to open candidates.dat for %s generation.", is_tally ? "tally" : "results");
        return;
    }

    Candidate all[200];
    int count = 0;
    while (fread(&all[count], sizeof(Candidate), 1, fp)) count++;
    fclose(fp);

    if (count == 0) {
        strcpy(buffer, "\nNo candidates registered.\n");
        udp_send(sock, buffer, sizeof(buffer), cli);
        // --- NEW LOG ---
        log_event("INFO", "%s requested but no candidates are registered.", is_tally ? "Tally" : "Results");
        return;
    }

    char temp[512];
    strcat(buffer, "\n==================================================================\n");
    strcat(buffer, is_tally ? "               SONU ELECTION — VOTE TALLY                         \n" : "            SONU ELECTION — OFFICIAL RESULTS                      \n");
    strcat(buffer, "==================================================================\n");
    
    for (int p = 1; p <= NUM_POSITIONS; p++) {
        int max_votes = -1;
        for (int i = 0; i < count; i++) {
            if (all[i].position_index == p) {
                if (is_tally) {
                    snprintf(temp, sizeof(temp), "  %-30s | %-15s | %d votes\n", all[i].name, SONU_POSITIONS[all[i].position_index], all[i].votes);
                    strcat(buffer, temp);
                }
                if (all[i].votes > max_votes) max_votes = all[i].votes;
            }
        }
        
        if (!is_tally && max_votes >= 0) {
            snprintf(temp, sizeof(temp), "\n  %s WINNER(S):\n  ---------------------------------------------\n", SONU_POSITIONS[p]);
            strcat(buffer, temp);
            for (int i = 0; i < count; i++) {
                if (all[i].position_index == p && all[i].votes == max_votes) {
                    snprintf(temp, sizeof(temp), "  * %s (%d votes)\n", all[i].name, all[i].votes);
                    strcat(buffer, temp);
                }
            }
        }
    }

    // --- NEW LOG ---
    log_event("SUCCESS", "Generated and sent %s to client.", is_tally ? "election tally" : "official results");
    
    udp_send(sock, buffer, sizeof(buffer), cli);
}