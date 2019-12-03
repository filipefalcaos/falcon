# Run in dir "/falcon/docker":
# docker build -t gdb-alpine .

# Run in dir "/falcon":
# docker run --rm -it --cap-add=SYS_PTRACE --security-opt seccomp=unconfined -v $(pwd):/usr/local/falcon gdb-alpine

FROM alpine:latest

# gcc, g++, gdb, openssl, make, cmake, and bash
RUN apk update
RUN apk add gcc g++ gdb openssl make cmake bash
