FROM ubuntu:18.04 as build

RUN apt-get update && \
apt-get install -y libevent-dev make g++

COPY src src
COPY Makefile ./
RUN make CC=g++ build

FROM ubuntu:18.04

RUN apt-get update && \
apt-get install -y libevent-2.1-6

COPY --from=build ./src/newserv ./
COPY system system

ENTRYPOINT ["./newserv"]
