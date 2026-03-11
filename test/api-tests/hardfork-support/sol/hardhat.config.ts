import * as dotenv from "dotenv";
import { HardhatUserConfig } from "hardhat/config";
import "@nomicfoundation/hardhat-toolbox";
import "@nomicfoundation/hardhat-ethers";
import { ethers } from "ethers";
import { HardhatNetworkAccountUserConfig } from "hardhat/types";

dotenv.config();

function getAccounts() {
  const accounts: HardhatNetworkAccountUserConfig[] = [];
  const defaultBalance = ethers.parseEther("2000000").toString();

  if (process.env.PRIVATE_KEY) {
    const plainKey = new ethers.Wallet(process.env.PRIVATE_KEY).privateKey;
    accounts.push({
      privateKey: plainKey,
      balance: defaultBalance,
    });
  } else {
    const n = 10;
    for (let i = 0; i < n; ++i) {
      accounts.push({
        privateKey: ethers.Wallet.createRandom().privateKey,
        balance: defaultBalance,
      });
    }
  }

  return accounts;
}

const config: HardhatUserConfig = {
  networks: {
    hardhat: {
      accounts: getAccounts(),
      chainId: 31337,
      blockGasLimit: 50000000,
      gasPrice: 1000000000,
      mining: {
        auto: true,
        interval: 1000,
      },
    },
    custom: {
      url: process.env.ENDPOINT,
      accounts: process.env.PRIVATE_KEY ? [process.env.PRIVATE_KEY] : [],
    },
  },
  solidity: {
    version: "0.8.20",
    settings: {
      optimizer: {
        enabled: true,
        runs: 200,
      },
    },
  },
  paths: {
    sources: "./contracts",
    tests: "./test",
    cache: "./cache",
    artifacts: "./artifacts",
  },
};

export default config;
