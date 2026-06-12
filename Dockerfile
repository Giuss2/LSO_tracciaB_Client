FROM ubuntu:22.04

RUN apt update && apt install -y gcc


COPY . .

RUN gcc client.c -o client

CMD ["./client", "host.docker.internal", "5201"]
