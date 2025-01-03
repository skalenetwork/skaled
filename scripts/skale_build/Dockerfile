FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get -q update && \
    apt-get -qy install \
        software-properties-common \
        curl \
        wget \
        nettle-bin \
        gnutls-bin \
        libleveldb-dev \
        libgoogle-perftools-dev \
        python3 \
        python3-pip \
        btrfs-progs

RUN wget http://archive.ubuntu.com/ubuntu/pool/main/o/openssl/libssl1.1_1.1.1f-1ubuntu2_amd64.deb && \
    dpkg -i libssl1.1_1.1.1f-1ubuntu2_amd64.deb    

RUN echo 'Acquire::Retries "3";' > /etc/apt/apt.conf.d/80-net && \
    echo 'Acquire::http::Timeout "20";' >> /etc/apt/apt.conf.d/80-net

RUN add-apt-repository ppa:ubuntu-toolchain-r/test && \
    apt-get update && \
    apt-get install -y gcc-11 g++-11 gdb

RUN mkdir /skaled
COPY ./executable /skaled
WORKDIR /skaled
RUN chmod +x skaled
RUN ldd skaled

ENTRYPOINT ["/skaled/skaled"]
