FROM alpine:latest

# gcc, g++, gdb, make, readline, wget, openssl, and cmake
RUN apk update
RUN apk add gcc g++ gdb make readline-dev wget openssl cmake bash

# Run in dir "/yapl/docker":
# docker build -t gdb-alpine .

# Run in dir "/yapl"
# docker run --rm -it --cap-add=SYS_PTRACE --security-opt seccomp=unconfined -v $(pwd):/usr/local/yapl gdb-alpine
