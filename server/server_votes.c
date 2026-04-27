#include <stdio.h>
#include <string.h>
#include "server.h"
#include "../shared/candidate.h"

typedef struct {
    char student_id[15];
    char name[50];
    int  has_voted; 
    int  votes_cast;
} Voter;

void server_cast_vote(int sock, struct sockaddr_in *cli) {
    // 1. Send candidate count and list to client
    FILE *fp = fopen("candidates.dat", "rb");
    Candidate all[200];
    int count = 0;
    if (fp) {
        while (fread(&all[count], sizeof(Candidate), 1, fp)) count++;
        fclose(fp);
    }
    udp_send(sock, &count, sizeof(int), cli);
    if (count > 0) udp_send(sock, all, sizeof(Candidate) * count, cli);
    
    // 2. Receive vote packet (voter_id, candidate_id)
    struct {
        char voter_id[MAX_ID];
        char candidate_id[MAX_ID];
    } vote;
    udp_recv(sock, &vote, sizeof(vote), cli);
    
    char msg[256];
    
    // 3. Check if voter exists and hasn't voted
    Voter voter;
    FILE *vfp = fopen("voters.dat", "rb");
    int voter_found = 0, voter_voted = 0;
    
    if (vfp) {
        while (fread(&voter, sizeof(Voter), 1, vfp)) {
            if (strcmp(voter.student_id, vote.voter_id) == 0) {
                voter_found = 1; voter_voted = voter.has_voted; break;
            }
        }
        fclose(vfp);
    }
    
    if (!voter_found) {
        strcpy(msg, "Error: Voter not registered.");
        // --- NEW LOG ---
        log_event("WARN", "Vote attempt by unregistered ID: %s", vote.voter_id);
    } else if (voter_voted) {
        strcpy(msg, "Error: Voter has already cast a vote.");
        // --- NEW LOG ---
        log_event("WARN", "Duplicate vote attempt by Voter ID: %s", vote.voter_id);
    } else {
        // 4. Find candidate and increment votes
        int candidate_found = 0;
        FILE *cfp = fopen("candidates.dat", "r+b");
        if (cfp) {
            Candidate temp;
            while (fread(&temp, sizeof(Candidate), 1, cfp)) {
                if (strcmp(temp.student_id, vote.candidate_id) == 0) {
                    candidate_found = 1; temp.votes++;
                    fseek(cfp, -(long)sizeof(Candidate), SEEK_CUR);
                    fwrite(&temp, sizeof(Candidate), 1, cfp);
                    
                    snprintf(msg, sizeof(msg), "Vote cast for %s in the %s position!", temp.name, SONU_POSITIONS[temp.position_index]);
                    
                    // --- NEW LOG ---
                    log_event("SUCCESS", "Voter %s cast vote for Candidate %s (%s)", vote.voter_id, temp.name, SONU_POSITIONS[temp.position_index]);
                    break;
                }
            }
            fclose(cfp);
        }
        
        if (candidate_found) {
            // 5. Mark voter as voted
            FILE *vfp2 = fopen("voters.dat", "r+b");
            if (vfp2) {
                Voter temp;
                while (fread(&temp, sizeof(Voter), 1, vfp2)) {
                    if (strcmp(temp.student_id, vote.voter_id) == 0) {
                        temp.has_voted = 1;
                        fseek(vfp2, -(long)sizeof(Voter), SEEK_CUR);
                        fwrite(&temp, sizeof(Voter), 1, vfp2);
                        break;
                    }
                }
                fclose(vfp2);
            }
        } else {
            strcpy(msg, "Error: Candidate not found.");
            // --- NEW LOG ---
            log_event("ERROR", "Voter %s attempted to vote for non-existent Candidate ID: %s", vote.voter_id, vote.candidate_id);
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