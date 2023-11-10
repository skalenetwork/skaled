require("@nomicfoundation/hardhat-toolbox");
require('@openzeppelin/hardhat-upgrades');

/** @type import('hardhat/config').HardhatUserConfig */
module.exports = {
  solidity: {

    compilers: [
      {
        version: `0.8.19`,
        settings: {
          optimizer: {
            enabled: true,
            runs: 1000
          },
          evmVersion: `shanghai`, // downgrade to `paris` if you encounter 'invalid opcode' error
        }
      }
    ]
  }
};

/* Address 0x907cd0881E50d359bb9Fd120B1A5A143b1C97De6 */
const INSECURE_PRIVATE_KEY = "0x8b8cf9982c4b034152c3661e105c221ce9c08fed2ed32942dc69214915941842";

module.exports = {
  solidity: "0.8.19",
  settings: {
    optimizer: {
      enabled: true,
      runs: 200
    },
    evmVersion: "shanghai" // Set the EVM target version here
  },
  networks: {
    skaled: {
      url: `http://10.3.155.174:10003`,
      accounts: [INSECURE_PRIVATE_KEY],
      chainId: 4353165712451818,
      gas: 0x10000000,
      hardfork: 'shanghai'
    }
  }
};
