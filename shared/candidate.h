#ifndef CANDIDATE_H
#define CANDIDATE_H

typedef struct {
    char student_id[50];            // Changed to string
    char name[50];
    int position_index;     // 0 to 10
    int votes;
} Candidate;

#endif
