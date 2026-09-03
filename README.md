<div align="center">
  <img src="https://uploads-ssl.webflow.com/5be05ae542686c4ebf192462/5be2f8beb08f6d0fbd2ea797_Skale_Logo_Blue-p-500.png"><br><br>
</div>

-----------------


# SKALED – SKALE C++ Client


[![Discord](https://img.shields.io/discord/534485763354787851.svg)](https://discord.gg/YYHy7Ekc8)

Skaled is SKALE Proof-Of-Stake blockchain client, compatible with ETH ecocystem, including EVM, Solidity, Metamask and Truffle. It uses [SKALE BFT Consensus engine](https://github.com/skalenetwork/skale-consensus).  It is currently actively developed and maintained by SKALE Labs, and intended to be used for [SKALE blockchains](https://docs.skale.space/skale-chain/introduction).

The SKALE network supports an unlimited number of independent blockchains with zero gas fees, instant finality, and high transaction throughput. SKALE is the first live blockchain with Linear Scaling. As more nodes join the network, the capacity of the network also grows.

## Forklessness

Skaled is forkless, meaning that blockchain a linear chain (and not a tree of forks as with ETH 1.0). Every block is provably finalized within finite time.


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

Skaled officially builds, runs, and is tested on Ubuntu 24.04. You may build and run it on other Ubuntu versions at your own risk.

### Clone repository

```
git clone --recurse-submodules https://github.com/skalenetwork/skaled.git
cd skaled
```

⚠️ Note: Because this repository depends on additional submodules, it is important to pass`--recurse-submodules` to the `git clone` command.

If you have already cloned the repo and forgot to pass `--recurse-submodules`, execute `git submodule update --init --recursive`

### Install required Ubuntu 24.04 packages

```
sudo apt-get update
sudo apt-get install -y libunwind-dev autoconf build-essential cmake libtool texinfo wget yasm flex bison btrfs-progs python3 python3-pip gawk git vim doxygen
sudo apt-get install -y make pkg-config libgnutls28-dev libssl-dev unzip zlib1g-dev libgcrypt20-dev docker.io gcc-11 g++-11 gperf
sudo apt-get install -y nettle-dev libhiredis-dev redis-server google-perftools libgoogle-perftools-dev lcov
sudo apt-get install -y gettext
```

The older setup listed `libprocps-dev`, `gnutls-dev`, `libv8-dev`, and `clang-format-11`; they are not required for this build and are unavailable from Ubuntu 24.04's standard repositories. `libgnutls28-dev` provides the required GnuTLS development files.

### Set GCC 11 as the default compiler

Ubuntu 24.04 uses a newer GCC release by default, while skaled is built with GCC 11.

```
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 11
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 11
sudo update-alternatives --install /usr/bin/gcov gcov /usr/bin/gcov-11 11
sudo update-alternatives --install /usr/bin/gcov-dump gcov-dump /usr/bin/gcov-dump-11 11
sudo update-alternatives --install /usr/bin/gcov-tool gcov-tool /usr/bin/gcov-tool-11 11
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

## Next steps: Run `skaled` with SGX


This page only covers **building and testing**.


To **run `skaled` with Intel SGX** see: [Run `skaled` with SGX](./docs/getting-started/one-node.md)

## Documentation

* [SKALED documentation](./docs/README.md)
* [SKALE Network documentation](https://docs.skale.space)

## Contributing

We are actively looking for contributors and have great bounties!

**Please read [CONTRIBUTING](https://github.com/skalenetwork/skale-network/blob/master/CONTRIBUTING.md) and [CODING_STYLE](CODING_STYLE.md) thoroughly before making alterations to the code base. This project adheres to SKALE's code of conduct. By participating, you are expected to uphold this code.**

**We use GitHub issues for tracking requests and bugs, so please see our general development questions and discussion on [Discord](https://discord.gg/YYHy7Ekc8).**

All contributions are welcome! We try to keep a list of tasks that are suitable for newcomers under the tag [help wanted](https://github.com/skalenetwork/skaled/labels/help%20wanted). If you have any questions, please just ask.

[![Discord](https://img.shields.io/discord/534485763354787851.svg)](https://discord.gg/YYHy7Ekc8)


All development goes in develop branch. 


## For more information
* [SKALE Labs Website](https://www.skale.space/)
* [SKALE Labs Twitter](https://x.com/SkaleNetwork)
* [SKALE Labs Blog](https://medium.com/skale)

Learn more about the SKALE community over on [Discord](https://discord.gg/YYHy7Ekc8).


## License

[![License](https://img.shields.io/github/license/skalenetwork/skaled.svg)](LICENSE)

All contributions are made under the [GNU General Public License v3](https://www.gnu.org/licenses/gpl-3.0.en.html). See [LICENSE](LICENSE).

All original cpp-ethereum code Copyright (C) Aleth Authors.  
All cpp-ethereum modifications Copyright (C) SKALE Labs.  
All skaled code Copyright (C) SKALE Labs.
