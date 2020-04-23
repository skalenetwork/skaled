FROM skalenetwork/consensust_base:latest

RUN apt-get update
RUN apt-get install -yq  libprocps-dev g++-7   ccache  \
    flex bison yasm python python-pip texinfo clang-format-6.0 btrfs-progs \
    cmake libtool build-essential pkg-config autoconf wget git  libargtable2-dev \
    libmicrohttpd-dev libhiredis-dev redis-server openssl libssl-dev doxygen vim



ENV CC gcc-7
ENV CXX g++-7
ENV TARGET all
ENV TRAVIS_BUILD_TYPE Debug

COPY libconsensus /skaled/libconsensus
COPY SkaleDeps /skaled/SkaleDeps
WORKDIR /consensust
RUN cd libconsensus/scripts && ./build_deps.sh 
RUN cd SkaleDeps && ./build.sh
COPY . /skaled

RUN cmake -Bbuild -DCMAKE_BUILD_TYPE=Debug .
RUN bash -c "cmake --build build  --" ##; true;


