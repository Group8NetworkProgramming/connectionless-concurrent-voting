
FROM gcc:latest AS builder
WORKDIR /app
COPY . .

# Use the new Makefile target to build ONLY the server
RUN make clean && make server

# -----------------------------------------
# STAGE 2: The Runner 
# -----------------------------------------
FROM debian:bookworm-slim
WORKDIR /app

# Pull only the compiled server binary from Stage 1
COPY --from=builder /app/server_app .

EXPOSE 8080/udp
CMD ["./server_app"]