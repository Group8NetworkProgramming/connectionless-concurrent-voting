#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>
#include "../shared/protocol.h"
#include "../shared/voter.h"      
#include "../shared/candidate.h"

#define PORT 8080

// Network Helpers
void udp_send(int sock, const void *msg, size_t len, struct sockaddr_in *dest);
void udp_recv(int sock, void *buf, size_t len, struct sockaddr_in *src);

void log_event(const char *level, const char *format, ...);

// Database Helpers
int is_id_registered(const char* id, const char* filename);
int read_record(const char *filename, long index, void *out, size_t record_size);
int append_record(const char *filename, void *record, size_t record_size);

// Route Handlers
void server_register_voter(int sock, struct sockaddr_in *cli);
void server_register_candidate(int sock, struct sockaddr_in *cli);
void server_view_candidates(int sock, struct sockaddr_in *cli);
void server_cast_vote(int sock, struct sockaddr_in *cli);
void server_send_large_text(int sock, struct sockaddr_in *cli, int is_tally);

#endif