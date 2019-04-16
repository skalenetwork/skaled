<div align="center">
  <img src="https://uploads-ssl.webflow.com/5be05ae542686c4ebf192462/5be2f8beb08f6d0fbd2ea797_Skale_Logo_Blue-p-500.png"><br><br>
</div>

-----------------

# SKALED – SKALE C++ Client

[![Discord](https://img.shields.io/discord/534485763354787851.svg)](https://discord.gg/vvUtWJB)

The collection of C++ libraries and tools for [SKALE Network](https://skalelabs.com). This EVM-compatible client is forked from [Aleth](https://github.com/ethereum/aleth) (formerly known as the [cpp-ethereum](http://www.ethdocs.org/en/latest/ethereum-clients/cpp-ethereum/) project). It has been modified to work with the SKALE network.

This respository is maintained by SKALE Labs, and intended to be used for SKALE chains (elastic sidechains).

## Getting Started

## Building from source

GitHub is used to maintain this source code. Clone this repository by:

```
git clone --recurse-submodules https://github.com/skalenetwork/skaled.git
cd skaled
```

⚠️ Note: Because this repository depends on additional submodules, it is important to pass`--recurse-submodules` to the `git clone` command to automatically initialize and update each submodule.

If you have already cloned the repo and forgot to pass `--recurse-submodules`, then just execute `git submodule update --init`.

### Install dependencies (Ubuntu)

```
sudo apt-get update
sudo apt-get install autoconf build-essential cmake libboost-all-dev texinfo wget
```

### Install internal dependencies

```
cd SkaleDeps
./build.sh
```

### Build

Configure the project build with the following command to create the
`build` directory with the configuration.

```shell
cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Debug    # Configure the project and create a build directory.
cmake --build build -- -j$(nproc)             # Build all default targets.
```

## Contributing

**Please read [CONTRIBUTING](CONTRIBUTING.md) and [CODING_STYLE](CODING_STYLE.md) thoroughly before making alterations to the code base. This project adheres to SKALE's code of conduct. By participating, you are expected to uphold this code.**

**We use GitHub issues for tracking requests and bugs, so please see our general development questions and discussion on [Discord](https://discord.gg/vvUtWJB).**

All contributions are welcome! We try to keep a list of tasks that are suitable for newcomers under the tag [help wanted](https://github.com/skalenetwork/skaled/labels/help%20wanted). If you have any questions, please just ask.

[![Discord](https://img.shields.io/discord/534485763354787851.svg)](https://discord.gg/vvUtWJB)


All development goes in develop branch.

## Note on mining

The SKALE Network uses Proof-of-Stake, therefore this project is **not suitable for Ethereum mining**.

## Testing

To run the tests:

```
cd build/test
./testeth -- --all
```

## Documentation

_in process_

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