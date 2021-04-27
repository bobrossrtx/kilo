FROM ubuntu:20.04

LABEL author="Bobrossrtx"
LABEL version="0.0.1"
LABEL description="Kilo is a nano like command line text editor.\
Running kilo without any command line arguments causes kilo to\
open a plain document. You can edit any kind of document with\
kilo just like its predecessors, [ Vim, Emac, Nano ]"

WORKDIR /kilo
COPY ./* /kilo

# Installs
RUN apt update
RUN apt install -y make gcc

# Making executable
RUN make

CMD ["export" "PATH=$PATH:/kilo"]