<div align="center">
  <img src="https://uploads-ssl.webflow.com/5be05ae542686c4ebf192462/5be2f8beb08f6d0fbd2ea797_Skale_Logo_Blue-p-500.png"><br><br>
</div>

-----------------


# SKALED – SKALE C++ Client


[![Discord](https://img.shields.io/discord/534485763354787851.svg)](https://discord.gg/vvUtWJB)

Skaled is SKALE Proof-Of-Stake blockchain client, compatible with ETH ecocystem, including EVM, Solidity, Metamask and Truffle. It uses [SKALE BFT Consensus engine](https://github.com/skalenetwork/skale-consensus).  It is currently actively developed and maintained by SKALE Labs, and intended to be used for [SKALE blockchains](https://skale.space/technology).

The SKALE network supports an unlimited number of independent blockchains with zero gas fees, instant finality, and high transaction throughput. SKALE is the first live blockchain with Linear Scaling. As more nodes join the network, the capacity of the network also grows.

## Forklessness

Skaled is forkless, meaning that blockchain a linear chain (and not a tree of forks as with ETH 1.0). Every block is provably finalized immediately after creation.
Therefore,  finalization time for skaled is equal to block time, which is
much faster than 13 minutes for Eth main net.


## Asynchronous block production

Skaled is asynchronous, meaning that the consensus on the next block starts immediately after the previous block is finalized.  There is no set block time interval. This allows for subsecond block production in case of a fast network, enabling interactive Dapps.

## Provable security

Skaled is the only provably secure ETH compatible PoS client. Security is proven under assumption of maximum t malicious nodes, where the total number of nodes N is more or equal 3t + 1.

## Survivability

The network is assumed to bef fully asynchronous meaning that there is no upper limit for the packet delivery time. In case of a temporarily network split, the protocol can wait indefinitely long until the split is resolved and then resume normal block production.

##  Historic origins

Historically skaled started by forking [Aleth](https://github.com/ethereum/aleth) (formerly known as the [cpp-ethereum](http://www.ethdocs.org/en/latest/ethereum-clients/cpp-ethereum/) project). We are thankful to the original cpp-ethereum team for their contributions.


## Building from source


### OS requirements

Skaled builds and runs on Ubuntu 20.04 and 22.04

### Clone repository

```
git clone --recurse-submodules https://github.com/skalenetwork/skaled.git
cd skaled
```

⚠️ Note: Because this repository depends on additional submodules, it is important to pass`--recurse-submodules` to the `git clone` command.

If you have already cloned the repo and forgot to pass `--recurse-submodules`, execute `git submodule update --init --recursive`

### Install required Ubuntu packages

```
sudo apt update
sudo apt install autoconf build-essential cmake libprocps-dev libtool texinfo wget yasm flex bison btrfs-progs python3 python3-pip gawk git vim doxygen 
sudo apt install make build-essential cmake pkg-config libgnutls28-dev libssl-dev unzip zlib1g-dev libgcrypt20-dev docker.io gcc-9 g++-9 gperf clang-format-11 gnutls-dev
sudo apt install nettle-dev libhiredis-dev redis-server google-perftools libgoogle-perftools-dev lcov sudo apt-get install libv8-dev
```




NB cmake needs to be of version >=3.21, git of version >=2.18

### (for Ubuntu 20.10 or later) Set  gcc-9 as default compiler
```
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 9
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-9 9
sudo update-alternatives --install /usr/bin/gcov gcov /usr/bin/gcov-9 9
sudo update-alternatives --install /usr/bin/gcov-dump gcov-dump /usr/bin/gcov-dump-9 9
sudo update-alternatives --install /usr/bin/gcov-tool gcov-tool /usr/bin/gcov-tool-9 9
gcc --version
```

# Install latest cmake 

```
sudo apt-get purge cmake
sudo snap install cmake --classic
```


### Build dependencies

```
cd deps
./build.sh DEBUG=1
```


## Hunter fix

```
mkdir -p ~/.hunter/_Base/Download/crc32c/1.0.5/dc7fa8c/
cd ~/.hunter/_Base/Download/crc32c/1.0.5/dc7fa8c/
wget https://github.com/hunter-packages/crc32c/archive/refs/tags/hunter-1.0.5.tar.gz
```

### Configure and build skaled


```shell
# Configure the project and create a build directory.
cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Debug
# Build all default targets using all cores.
cmake --build build -- -j$(nproc)
```


## Testing

To run the tests:

```
cd build/test
./testeth -- --all
```


## Documentation

[SKALED documentation](https://docs.skale.network/skaled/3.16.x-beta/)

## Contributing

We are actively looking for contributors and have great bounties!

**Please read [CONTRIBUTING](CONTRIBUTING.md) and [CODING_STYLE](CODING_STYLE.md) thoroughly before making alterations to the code base. This project adheres to SKALE's code of conduct. By participating, you are expected to uphold this code.**

**We use GitHub issues for tracking requests and bugs, so please see our general development questions and discussion on [Discord](https://discord.gg/vvUtWJB).**

All contributions are welcome! We try to keep a list of tasks that are suitable for newcomers under the tag [help wanted](https://github.com/skalenetwork/skaled/labels/help%20wanted). If you have any questions, please just ask.

[![Discord](https://img.shields.io/discord/534485763354787851.svg)](https://discord.gg/vvUtWJB)


All development goes in develop branch. 


## For more information
* [SKALE Labs Website](https://skalelabs.com)
* [SKALE Labs Twitter](https://twitter.com/skalelabs)
* [SKALE Labs Blog](https://medium.com/skale)

Learn more about the SKALE community over on [Discord](https://discord.gg/vvUtWJB).


## License

[![License](https://img.shields.io/github/license/skalenetwork/skaled.svg)](LICENSE)

All contributions are made under the [GNU General Public License v3](https://www.gnu.org/licenses/gpl-3.0.en.html). See [LICENSE](LICENSE).

All original cpp-ethereum code Copyright (C) Aleth Authors.  
All cpp-ethereum modifications Copyright (C) SKALE Labs.  
All skaled code Copyright (C) SKALE Labs.
