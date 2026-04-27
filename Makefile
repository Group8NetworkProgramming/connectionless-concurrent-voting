CC = gcc
CFLAGS = -Wall -I.

SERVER_SRCS = server/server_main.c server/server_candidates.c server/server_voters.c server/server_votes.c server/utils.c

CLIENT_SRCS = client/client_main.c 
CLIENT_BIN = client_app
SERVER_BIN = server_app

# .PHONY prevents conflicts if you ever create a file named "clean" or "server"
.PHONY: all server client clean wipe

# Default target when you just type 'make'
all: server client

# Target to build JUST the server
server: $(SERVER_BIN)

# Target to build JUST the client
client: $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_SRCS)
	$(CC) $(CFLAGS) $(SERVER_SRCS) -o $(SERVER_BIN)
	@echo "--> Server compiled successfully as '$(SERVER_BIN)'"
	
$(CLIENT_BIN): $(CLIENT_SRCS)
	$(CC) $(CFLAGS) $(CLIENT_SRCS) -o $(CLIENT_BIN)
	@echo "--> Client compiled successfully as '$(CLIENT_BIN)'"

clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN) 
	@echo "--> Binaries removed."


wipe: clean
	rm -f *.dat *.log
	@echo "--> Databases and logs wiped clean."