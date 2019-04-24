FROM ubuntu:16.04

RUN apt-get -q update && \
    apt-get -qy install \
        software-properties-common \
        curl \
        libgmp-dev \
        libleveldb-dev \
        nettle-bin \
        gnutls-bin \
        python3 \
        python3-pip

RUN add-apt-repository ppa:ubuntu-toolchain-r/test && \
    apt update && \
    apt install g++-7 gdb -y

RUN mkdir /skaled
COPY ./executable /skaled
WORKDIR /skaled
RUN chmod +x skaled

EXPOSE 30303

ENV DATA_DIR $DATA_DIR

ENTRYPOINT /skaled/skaled --no-discovery --config $CONFIG_FILE -d $DATA_DIR --ipcpath $DATA_DIR -v 4  --http-trace --aa no --ask 0 --bid 0 >> $DATA_DIR/stdout.log 2>> $DATA_DIR/stderr.log
#CMD ["bash"]

