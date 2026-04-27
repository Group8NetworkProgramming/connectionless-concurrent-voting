#ifndef VOTER_H
#define VOTER_H

#include <stddef.h> 

#define MAX_NAME     50
#define MAX_ID       15
#define MAX_POSITION 50
#define NUM_POSITIONS 11

// Protocol Command Codes
#define CMD_REG_VOTER       1
#define CMD_REG_CANDIDATE   2
#define CMD_VIEW_CANDIDATES 3
#define CMD_CAST_VOTE       4
#define CMD_VIEW_TALLY      5
#define CMD_RESULTS         6
#define CMD_QUIT            7

// Notice the second 'const' and the '__attribute__((unused))'
static const char * const SONU_POSITIONS[] __attribute__((unused)) = {
    "", 
    "Chairman", 
    "Vice Chairman", 
    "Secretary General",
    "Organizing Secretary", 
    "Secretary for Finance",
    "Secretary for Academic Affairs", 
    "Secretary for Catering and Accommodation",
    "Secretary for Legal Affairs", 
    "Secretary for Gender Affairs",
    "Secretary for Special Needs", 
    "Campus/Faculty Representatives"
};

#endif