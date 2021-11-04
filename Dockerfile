FROM ubuntu:bionic 

RUN add-apt-repository ppa:ubuntu-toolchain-r/test
RUN apt-get update
RUN apt-get install -y libprocps-dev gcc-9 g++-9 ccache \
    flex bison yasm python python-pip texinfo clang-format-6.0 btrfs-progs \
    cmake libtool build-essential pkg-config autoconf wget git  libargtable2-dev \
    libmicrohttpd-dev libhiredis-dev redis-server openssl libssl-dev doxygen vim



ENV CC gcc-9
ENV CXX g++-9
ENV TARGET all
ENV TRAVIS_BUILD_TYPE Debug

#COPY libconsensus /skaled/libconsensus
#COPY deps /skaled/deps
WORKDIR /skaled
#RUN cd libconsensus/scripts && ./build_deps.sh 
#RUN cd deps && ./build.sh
COPY . /skaled
RUN cd /skaled && cd deps && ./build.sh && cd .. && mkdir build && cmake . -Bbuild -DCMAKE_BUILD_TYPE=Debug
RUN bash -c "cmake --build build  -- -j$(nproc)"
#RUN bash -c "cd /skaled/build/test && ./testeth -- --express"
#RUN bash -c "cd /skaled/build/test && ./testeth -t BtrfsTestSuite -- --all"
#RUN bash -c "cd /skaled/build/test && ./testeth -t HashSnapshotTestSuite -- --all"
#RUN bash -c "cd /skaled/build/test && ./testeth -t ClientSnapshotsSuite -- --all"
