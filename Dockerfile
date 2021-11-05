FROM ubuntu:bionic 

RUN apt-get install -yq software-properties-common
RUN add-apt-repository ppa:ubuntu-toolchain-r/test
RUN apt-get update
RUN apt-get install -yq gcc-9 g++-9 libprocps-dev ccache \
    flex bison yasm python python-pip texinfo clang-format-6.0 btrfs-progs \
    cmake libtool build-essential pkg-config autoconf wget git  libargtable2-dev \
    libmicrohttpd-dev libhiredis-dev redis-server openssl libssl-dev doxygen vim

RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 9
RUN update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-9 9
RUN update-alternatives --install /usr/bin/gcov gcov /usr/bin/gcov-9 9
RUN update-alternatives --install /usr/bin/gcov-dump gcov-dump /usr/bin/gcov-dump-9 9
RUN update-alternatives --install /usr/bin/gcov-tool gcov-tool /usr/bin/gcov-tool-9 9

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
RUN cd /skaled && cd deps && ./build.sh PARALLEL_COUNT=$(nproc) && cd .. && mkdir build && cmake . -Bbuild -DCMAKE_BUILD_TYPE=Release
RUN bash -c "cmake --build build  -- -j$(nproc)"
#RUN bash -c "cd /skaled/build/test && ./testeth -- --express"
#RUN bash -c "cd /skaled/build/test && ./testeth -t BtrfsTestSuite -- --all"
#RUN bash -c "cd /skaled/build/test && ./testeth -t HashSnapshotTestSuite -- --all"
#RUN bash -c "cd /skaled/build/test && ./testeth -t ClientSnapshotsSuite -- --all"
