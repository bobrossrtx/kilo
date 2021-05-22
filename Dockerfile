# Repo: https://hub.docker.com/repository/docker/owenboreham/kilo/

FROM ubuntu:devel

LABEL author="Bobrossrtx"
LABEL version="0.0.1"
LABEL description="Kilo is a nano like command line text editor.\
Running kilo without any command line arguments causes kilo to\
open a plain document. You can edit any kind of document with\
kilo just like its predecessors, [ Vim, Emac, Nano ]"

WORKDIR /kilo/
COPY . /kilo/

# Installs
RUN apt-get update \
    && apt-get upgrade \
    && apt-get install -y --no-install-recommends make gcc vim nano \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*



WORKDIR /kilo/

# Making new executable just in case old one is corrupt
RUN make all

# Setting path
RUN export PATH=$PATH:/kilo
