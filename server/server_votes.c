#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include "../shared/protocol.h"  
#include "../shared/voter.h"     
#include "../shared/candidate.h" 
#include "server.h"

// Vote data is now passed in — already received by the parent before fork().
void server_cast_vote(int sock, struct sockaddr_in *cli, void *vote_data) {
    struct { char voter_id[MAX_ID]; char candidate_id[MAX_ID]; } vote;
    memcpy(&vote, vote_data, sizeof(vote));

    printf("[TRACE] Cast Vote (PID: %d): Processing vote — voter '%s' voting for candidate '%s'.\n",
           getpid(), vote.voter_id, vote.candidate_id);
    fflush(stdout);
    char msg[256];

    // ===== STEP 1: Verify voter exists and hasn't voted =====
    printf("[TRACE] Cast Vote (PID: %d): Opening voters.dat to verify voter status...\n", getpid());
    fflush(stdout);
    FILE *vfp = fopen("voters.dat", "rb");
    int voter_found = 0, voter_voted = 0;
    if (vfp) {
        // --- Acquire shared lock for reading ---
        printf("[TRACE] Cast Vote (PID: %d): Acquiring SHARED lock on voters.dat...\n", getpid());
        fflush(stdout);
        flock(fileno(vfp), LOCK_SH);
        printf("[TRACE] Cast Vote (PID: %d): SHARED lock acquired on voters.dat.\n", getpid());
        fflush(stdout);

        // --- File read: scan voter records ---
        printf("[TRACE] Cast Vote (PID: %d): File Read — scanning voter records for ID '%s'...\n",
               getpid(), vote.voter_id);
        fflush(stdout);
        Voter v;
        while (fread(&v, sizeof(Voter), 1, vfp)) {
            if (strcmp(v.student_id, vote.voter_id) == 0) {
                voter_found = 1;
                voter_voted = v.has_voted;
                printf("[TRACE] Cast Vote (PID: %d): Found voter '%s' (has_voted=%d).\n",
                       getpid(), v.name, v.has_voted);
                fflush(stdout);
                break;
            }
        }

        // --- Release lock ---
        printf("[TRACE] Cast Vote (PID: %d): Releasing SHARED lock on voters.dat.\n", getpid());
        fflush(stdout);
        flock(fileno(vfp), LOCK_UN);
        fclose(vfp);
    } else {
        printf("[TRACE] Cast Vote (PID: %d): Could not open voters.dat!\n", getpid());
        fflush(stdout);
    }

    printf("[TRACE] Cast Vote (PID: %d): Voter lookup result — found=%d, already_voted=%d\n",
           getpid(), voter_found, voter_voted);
    fflush(stdout);

    if (!voter_found) {
        strcpy(msg, "Error: Voter not registered.");
        log_event("WARN", "Vote failed: Voter '%s' not found.", vote.voter_id);
    } else if (voter_voted) {
        strcpy(msg, "Error: Voter has already cast a vote.");
        log_event("WARN", "Vote failed: Voter '%s' has already voted.", vote.voter_id);
    } else {
        // ===== STEP 2: Find candidate and increment vote count =====
        int candidate_found = 0;
        printf("[TRACE] Cast Vote (PID: %d): Opening candidates.dat to record vote...\n", getpid());
        fflush(stdout);
        FILE *cfp = fopen("candidates.dat", "r+b");
        if (cfp) {
            // --- Acquire exclusive lock for read-modify-write ---
            printf("[TRACE] Cast Vote (PID: %d): Acquiring EXCLUSIVE lock on candidates.dat...\n", getpid());
            fflush(stdout);
            flock(fileno(cfp), LOCK_EX);
            printf("[TRACE] Cast Vote (PID: %d): EXCLUSIVE lock acquired on candidates.dat.\n", getpid());
            fflush(stdout);

            // --- File read + CPU processing: find and update candidate ---
            printf("[TRACE] Cast Vote (PID: %d): File Read — scanning for candidate ID '%s'...\n",
                   getpid(), vote.candidate_id);
            fflush(stdout);
            Candidate temp;
            while (fread(&temp, sizeof(Candidate), 1, cfp)) {
                if (strcmp(temp.student_id, vote.candidate_id) == 0) {
                    candidate_found = 1;

                    printf("[TRACE] Cast Vote (PID: %d): CPU Processing — incrementing vote count for '%s' (%d -> %d)...\n",
                           getpid(), temp.name, temp.votes, temp.votes + 1);
                    fflush(stdout);
                    temp.votes++;

                    fseek(cfp, -(long)sizeof(Candidate), SEEK_CUR);

                    printf("[TRACE] Cast Vote (PID: %d): File Write — writing updated candidate record back to candidates.dat...\n", getpid());
                    fflush(stdout);
                    fwrite(&temp, sizeof(Candidate), 1, cfp);

                    snprintf(msg, sizeof(msg), "Vote cast for %s in the %s position!",
                             temp.name, SONU_POSITIONS[temp.position_index]);
                    log_event("SUCCESS", "Vote cast: Voter '%s' -> Candidate '%s' (%s). New total: %d.",
                              vote.voter_id, temp.name, SONU_POSITIONS[temp.position_index], temp.votes);
                    break;
                }
            }

            // --- Release lock ---
            printf("[TRACE] Cast Vote (PID: %d): Releasing EXCLUSIVE lock on candidates.dat.\n", getpid());
            fflush(stdout);
            flock(fileno(cfp), LOCK_UN);
            fclose(cfp);
        } else {
            printf("[TRACE] Cast Vote (PID: %d): FAILED to open candidates.dat for writing!\n", getpid());
            fflush(stdout);
        }

        // ===== STEP 3: Mark voter as has_voted =====
        if (candidate_found) {
            printf("[TRACE] Cast Vote (PID: %d): Candidate found. Now updating voter record...\n", getpid());
            fflush(stdout);
            printf("[TRACE] Cast Vote (PID: %d): Opening voters.dat for voter status update...\n", getpid());
            fflush(stdout);
            FILE *vfp2 = fopen("voters.dat", "r+b");
            if (vfp2) {
                // --- Acquire exclusive lock for read-modify-write ---
                printf("[TRACE] Cast Vote (PID: %d): Acquiring EXCLUSIVE lock on voters.dat...\n", getpid());
                fflush(stdout);
                flock(fileno(vfp2), LOCK_EX);
                printf("[TRACE] Cast Vote (PID: %d): EXCLUSIVE lock acquired on voters.dat.\n", getpid());
                fflush(stdout);

                // --- File read + CPU processing: find and update voter ---
                printf("[TRACE] Cast Vote (PID: %d): File Read — scanning for voter ID '%s'...\n",
                       getpid(), vote.voter_id);
                fflush(stdout);
                Voter temp;
                while (fread(&temp, sizeof(Voter), 1, vfp2)) {
                    if (strcmp(temp.student_id, vote.voter_id) == 0) {
                        printf("[TRACE] Cast Vote (PID: %d): CPU Processing — marking voter '%s' as has_voted=1...\n",
                               getpid(), temp.name);
                        fflush(stdout);
                        temp.has_voted = 1;

                        fseek(vfp2, -(long)sizeof(Voter), SEEK_CUR);

                        printf("[TRACE] Cast Vote (PID: %d): File Write — writing updated voter record back to voters.dat...\n", getpid());
                        fflush(stdout);
                        fwrite(&temp, sizeof(Voter), 1, vfp2);
                        break;
                    }
                }

                // --- Release lock ---
                printf("[TRACE] Cast Vote (PID: %d): Releasing EXCLUSIVE lock on voters.dat.\n", getpid());
                fflush(stdout);
                flock(fileno(vfp2), LOCK_UN);
                fclose(vfp2);
            } else {
                printf("[TRACE] Cast Vote (PID: %d): FAILED to open voters.dat for update!\n", getpid());
                fflush(stdout);
            }
        } else {
            strcpy(msg, "Error: Candidate not found.");
            log_event("WARN", "Vote failed: Candidate '%s' not found.", vote.candidate_id);
        }
    }

    // --- Send response ---
    printf("[TRACE] Cast Vote (PID: %d): Sending response to client: '%s'\n", getpid(), msg);
    fflush(stdout);
    udp_send(sock, msg, sizeof(msg), cli);
}

void server_send_large_text(int sock, struct sockaddr_in *cli, int is_tally) {
    const char *label = is_tally ? "Tally" : "Results";
    static char buffer[16384];
    buffer[0] = '\0';

    // --- Open file for reading ---
    printf("[TRACE] %s Generation (PID: %d): Opening candidates.dat for reading...\n", label, getpid());
    fflush(stdout);
    FILE *fp = fopen("candidates.dat", "rb");
    if (!fp) {
        strcpy(buffer, "\nNo candidates database found.\n");
        printf("[TRACE] %s Generation (PID: %d): candidates.dat not found. Sending error to client.\n", label, getpid());
        fflush(stdout);
        udp_send(sock, buffer, sizeof(buffer), cli);
        log_event("ERROR", "Failed to open candidates.dat for %s generation.", is_tally ? "tally" : "results");
        return;
    }

    // --- Acquire shared lock ---
    printf("[TRACE] %s Generation (PID: %d): Acquiring SHARED lock on candidates.dat...\n", label, getpid());
    fflush(stdout);
    flock(fileno(fp), LOCK_SH);
    printf("[TRACE] %s Generation (PID: %d): SHARED lock acquired.\n", label, getpid());
    fflush(stdout);

    // --- File read ---
    Candidate all[200];
    int count = 0;
    printf("[TRACE] %s Generation (PID: %d): File Read — reading all candidate records from disk...\n", label, getpid());
    fflush(stdout);
    while (fread(&all[count], sizeof(Candidate), 1, fp)) count++;

    // --- Release lock ---
    printf("[TRACE] %s Generation (PID: %d): Read %d candidate records. Releasing SHARED lock.\n", label, getpid(), count);
    fflush(stdout);
    flock(fileno(fp), LOCK_UN);
    fclose(fp);

    if (count == 0) {
        strcpy(buffer, "\nNo candidates registered.\n");
        printf("[TRACE] %s Generation (PID: %d): No candidates found. Sending empty response.\n", label, getpid());
        fflush(stdout);
        udp_send(sock, buffer, sizeof(buffer), cli);
        log_event("INFO", "%s requested but no candidates are registered.", label);
        return;
    }

    // --- CPU processing: build formatted text ---
    printf("[TRACE] %s Generation (PID: %d): CPU Processing — building formatted %s text from %d records...\n",
           label, getpid(), is_tally ? "tally" : "results", count);
    fflush(stdout);

    char temp[512];
    strcat(buffer, "\n==================================================================\n");
    strcat(buffer, is_tally
        ? "               SONU ELECTION — VOTE TALLY                         \n"
        : "            SONU ELECTION — OFFICIAL RESULTS                      \n");
    strcat(buffer, "==================================================================\n");

    for (int p = 1; p <= NUM_POSITIONS; p++) {
        int max_votes = -1;
        for (int i = 0; i < count; i++) {
            if (all[i].position_index == p) {
                if (is_tally) {
                    snprintf(temp, sizeof(temp), "  %-30s | %-15s | %d votes\n",
                             all[i].name, SONU_POSITIONS[all[i].position_index], all[i].votes);
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

    printf("[TRACE] %s Generation (PID: %d): Text built (%zu bytes). Sending to client.\n",
           label, getpid(), strlen(buffer));
    fflush(stdout);
    log_event("SUCCESS", "Generated and sent %s to client.", is_tally ? "election tally" : "official results");
    udp_send(sock, buffer, sizeof(buffer), cli);
}
