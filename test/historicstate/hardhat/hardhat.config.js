require("@nomicfoundation/hardhat-toolbox");
require('@openzeppelin/hardhat-upgrades');

/** @type import('hardhat/config').HardhatUserConfig */
module.exports = {
  solidity: "0.8.25",
};

/* Address 0x907cd0881E50d359bb9Fd120B1A5A143b1C97De6 */
const INSECURE_PRIVATE_KEY = "bd200f4e7f597f3c2c77fb405ee7fabeb249f63f03f43d5927b4fa0c43cfe85e";

module.exports = {
  solidity: {
    compilers: [
      {
        version: `0.8.25`,
        settings: {
          optimizer: {
            enabled: true,
            runs: 15000
          },
          evmVersion: `shanghai`
        }
      },
    ],
  },
  networks: {
    skaled: {
      url: `http://localhost:1234`,
      accounts: [INSECURE_PRIVATE_KEY],
      chainId: 74565,
      gas: 0x10000000
    },
    geth: {
      url: `http://localhost:8545`,
      accounts: [INSECURE_PRIVATE_KEY],
      chainId: 1337,
      gas: 0x10000000
    }

  }
};
