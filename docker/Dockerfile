# Run in dir "/falcon/docker"
# With readline lib:    docker build -t falcon-alpine . --build-arg use_readline=true
# Without readline lib: docker build -t falcon-alpine .

# Run in dir "/falcon"
# docker run --rm -it --cap-add=SYS_PTRACE --security-opt seccomp=unconfined -v $(pwd):/usr/local/falcon falcon-alpine

FROM alpine:latest
ARG use_readline

# gcc, g++, gdb, openssl, make, cmake, and bash
RUN apk update
RUN apk add gcc g++ gdb openssl make cmake bash
RUN if [ "$use_readline" = "true" ] ; then apk add readline-dev ; else echo Readline not set ; fi
