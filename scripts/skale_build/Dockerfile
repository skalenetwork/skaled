FROM ubuntu:20.04

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

RUN echo 'Acquire::Retries "3";' > /etc/apt/apt.conf.d/80-net && \
    echo 'Acquire::http::Timeout "20";' >> /etc/apt/apt.conf.d/80-net

RUN add-apt-repository ppa:ubuntu-toolchain-r/test && \
    apt-get update && \
    apt-get install -y gcc-9 g++-9 gdb

RUN mkdir /skaled
COPY ./executable /skaled
WORKDIR /skaled
RUN chmod +x skaled

ENTRYPOINT ["/skaled/skaled"]
