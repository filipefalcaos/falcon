# Run in dir "/yapl/docker/gdb-alpine":
# docker build -t gdb-alpine .

# Run in dir "/yapl":
# docker run --rm -it --cap-add=SYS_PTRACE --security-opt seccomp=unconfined -v $(pwd):/usr/local/yapl gdb-alpine

FROM alpine:latest

# gcc, g++, gdb, make, readline, openssl, cmake, bash, and vim
RUN apk update
RUN apk add gcc g++ gdb make readline-dev openssl cmake
RUN apk add bash vim
