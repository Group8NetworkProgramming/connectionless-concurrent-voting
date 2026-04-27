#ifndef CLIENT_H
#define CLIENT_H

#include <netinet/in.h>
#include "../shared/protocol.h"

#define PORT 8080

// Network Helpers
void udp_send(int sock, const void *msg, size_t len, struct sockaddr_in *dest);
void udp_recv(int sock, void *buf, size_t len, struct sockaddr_in *src);
void send_cmd(int sock, int cmd, struct sockaddr_in *serv);

// UI Handlers
void remote_register_voter(int sock, struct sockaddr_in *serv);
void remote_register_candidate(int sock, struct sockaddr_in *serv);
void remote_view_candidates(int sock, struct sockaddr_in *serv);
void remote_cast_vote(int sock, struct sockaddr_in *serv);
void remote_print_large_text(int sock, int cmd, struct sockaddr_in *serv);

#endif