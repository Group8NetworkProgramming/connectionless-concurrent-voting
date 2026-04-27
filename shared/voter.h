#ifndef VOTER_H
#define VOTER_H

typedef struct {
    char student_id[15];
    char name[50];
    int  has_voted; 
    int  votes_cast;
} Voter;

#endif