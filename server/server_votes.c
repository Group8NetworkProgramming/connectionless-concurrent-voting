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
    printf("[TRACE] Cast Vote: Waiting to receive vote data (PID: %d)...\n", getpid());
    fflush(stdout);
    udp_recv(sock, &vote, sizeof(vote), cli);
    printf("[TRACE] Cast Vote: Received vote — voter ID '%s' voting for candidate ID '%s'.\n", vote.voter_id, vote.candidate_id);
    fflush(stdout);
    char msg[256];
    
    // ... [Voter Check logic remains mostly unchanged, but add a SHARED lock when reading]
    printf("[TRACE] Cast Vote: Opening voters.dat to verify voter status...\n");
    fflush(stdout);
    FILE *vfp = fopen("voters.dat", "rb");
    int voter_found = 0, voter_voted = 0;
    if (vfp) {
        printf("[TRACE] Cast Vote: Acquiring SHARED lock on voters.dat for voter lookup (PID: %d)...\n", getpid());
        fflush(stdout);
        flock(fileno(vfp), LOCK_SH);
        printf("[TRACE] Cast Vote: Reading voter records to find ID '%s'...\n", vote.voter_id);
        fflush(stdout);
        // ... [fread loop] ...
        printf("[TRACE] Cast Vote: Releasing SHARED lock on voters.dat.\n");
        fflush(stdout);
        flock(fileno(vfp), LOCK_UN);
        fclose(vfp);
    } else {
        printf("[TRACE] Cast Vote: Could not open voters.dat!\n");
        fflush(stdout);
    }
    printf("[TRACE] Cast Vote: Voter lookup result — found=%d, already_voted=%d\n", voter_found, voter_voted);
    fflush(stdout);
    
    if (!voter_found) {
        strcpy(msg, "Error: Voter not registered.");
    } else if (voter_voted) {
        strcpy(msg, "Error: Voter has already cast a vote.");
    } else {
        int candidate_found = 0;
        printf("[TRACE] Cast Vote: Opening candidates.dat to record vote...\n");
        fflush(stdout);
        FILE *cfp = fopen("candidates.dat", "r+b");
        if (cfp) {
            printf("[TRACE] Cast Vote: Acquiring EXCLUSIVE lock on candidates.dat (PID: %d)\n", getpid());
            fflush(stdout);
            flock(fileno(cfp), LOCK_EX);
            printf("[TRACE] Cast Vote: EXCLUSIVE lock acquired. Searching for candidate ID '%s'...\n", vote.candidate_id);
            fflush(stdout);
            
            Candidate temp;
            while (fread(&temp, sizeof(Candidate), 1, cfp)) {
                if (strcmp(temp.student_id, vote.candidate_id) == 0) {
                    candidate_found = 1; temp.votes++;
                    fseek(cfp, -(long)sizeof(Candidate), SEEK_CUR);
                    
                    printf("[TRACE] CPU Processing: Incrementing vote count for '%s' to %d...\n", temp.name, temp.votes);
                    fflush(stdout);
                    printf("[TRACE] File Write: Writing updated candidate record back to candidates.dat...\n");
                    fflush(stdout);
                    fwrite(&temp, sizeof(Candidate), 1, cfp);
                    snprintf(msg, sizeof(msg), "Vote cast for %s in the %s position!", temp.name, SONU_POSITIONS[temp.position_index]);
                    break;
                }
            }
            printf("[TRACE] Cast Vote: Releasing EXCLUSIVE lock on candidates.dat.\n");
            fflush(stdout);
            flock(fileno(cfp), LOCK_UN);
            fclose(cfp);
        } else {
            printf("[TRACE] Cast Vote: Failed to open candidates.dat for writing!\n");
            fflush(stdout);
        }
        
        if (candidate_found) {
            printf("[TRACE] Cast Vote: Candidate found. Now updating voter record...\n");
            fflush(stdout);
            printf("[TRACE] Cast Vote: Opening voters.dat for voter status update...\n");
            fflush(stdout);
            FILE *vfp2 = fopen("voters.dat", "r+b");
            if (vfp2) {
                printf("[TRACE] Cast Vote: Acquiring EXCLUSIVE lock on voters.dat (PID: %d)\n", getpid());
                fflush(stdout);
                flock(fileno(vfp2), LOCK_EX);
                printf("[TRACE] Cast Vote: EXCLUSIVE lock acquired. Searching for voter ID '%s'...\n", vote.voter_id);
                fflush(stdout);
                
                Voter temp;
                while (fread(&temp, sizeof(Voter), 1, vfp2)) {
                    if (strcmp(temp.student_id, vote.voter_id) == 0) {
                        temp.has_voted = 1;
                        fseek(vfp2, -(long)sizeof(Voter), SEEK_CUR);
                        
                        printf("[TRACE] CPU Processing: Marking voter '%s' as has_voted=1...\n", temp.name);
                        fflush(stdout);
                        printf("[TRACE] File Write: Writing updated voter record back to voters.dat...\n");
                        fflush(stdout);
                        fwrite(&temp, sizeof(Voter), 1, vfp2);
                        break;
                    }
                }
                printf("[TRACE] Cast Vote: Releasing EXCLUSIVE lock on voters.dat.\n");
                fflush(stdout);
                flock(fileno(vfp2), LOCK_UN);
                fclose(vfp2);
            } else {
                printf("[TRACE] Cast Vote: Failed to open voters.dat for update!\n");
                fflush(stdout);
            }
        } else {
            strcpy(msg, "Error: Candidate not found.");
        }
    }
    printf("[TRACE] Cast Vote: Sending response to client: '%s'\n", msg);
    fflush(stdout);
    udp_send(sock, msg, sizeof(msg), cli);
}

void server_send_large_text(int sock, struct sockaddr_in *cli, int is_tally) {
    static char buffer[16384]; 
    buffer[0] = '\0';
    
    printf("[TRACE] %s Generation: Opening candidates.dat for reading (PID: %d)...\n", is_tally ? "Tally" : "Results", getpid());
    fflush(stdout);
    FILE *fp = fopen("candidates.dat", "rb");
    if (!fp) {
        strcpy(buffer, "\nNo candidates database found.\n");
        printf("[TRACE] %s Generation: candidates.dat not found. Sending error to client.\n", is_tally ? "Tally" : "Results");
        fflush(stdout);
        udp_send(sock, buffer, sizeof(buffer), cli);
        log_event("ERROR", "Failed to open candidates.dat for %s generation.", is_tally ? "tally" : "results");
        return;
    }

    Candidate all[200];
    int count = 0;
    printf("[TRACE] %s Generation: Reading all candidate records from disk...\n", is_tally ? "Tally" : "Results");
    fflush(stdout);
    while (fread(&all[count], sizeof(Candidate), 1, fp)) count++;
    fclose(fp);
    printf("[TRACE] %s Generation: Read %d candidate records.\n", is_tally ? "Tally" : "Results", count);
    fflush(stdout);

    if (count == 0) {
        strcpy(buffer, "\nNo candidates registered.\n");
        printf("[TRACE] %s Generation: No candidates found. Sending empty response.\n", is_tally ? "Tally" : "Results");
        fflush(stdout);
        udp_send(sock, buffer, sizeof(buffer), cli);
        log_event("INFO", "%s requested but no candidates are registered.", is_tally ? "Tally" : "Results");
        return;
    }

    printf("[TRACE] %s Generation: CPU Processing — building formatted %s text...\n", is_tally ? "Tally" : "Results", is_tally ? "tally" : "results");
    fflush(stdout);
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

    log_event("SUCCESS", "Generated and sent %s to client.", is_tally ? "election tally" : "official results");
    
    printf("[TRACE] %s Generation: Sending %zu bytes of formatted text to client.\n", is_tally ? "Tally" : "Results", strlen(buffer));
    fflush(stdout);
    udp_send(sock, buffer, sizeof(buffer), cli);
}