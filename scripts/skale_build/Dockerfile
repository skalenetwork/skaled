FROM ubuntu:18.04

RUN apt-get -q update && \
    apt-get -qy install \
        software-properties-common \
        curl \
        libgmp-dev \
        libleveldb-dev \
        nettle-bin \
        gnutls-bin \
        python3 \
        python3-pip \
        btrfs-progs

RUN add-apt-repository ppa:ubuntu-toolchain-r/test && \
    apt update && \
    apt install g++-7 gdb -y

RUN mkdir /skaled
COPY ./executable /skaled
WORKDIR /skaled
RUN chmod +x skaled

ENTRYPOINT /skaled/skaled $OPTIONS
