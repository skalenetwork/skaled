// We require the Hardhat Runtime Environment explicitly here. This is optional
// but useful for running the script in a standalone fashion through `node <script>`.
//
// You can also run a script with `npx hardhat run <script>`. If you do that, Hardhat
// will compile your contracts, add the Hardhat Runtime Environment's members to the
// global scope, and execute the script.


WALLETS_COUNT = 1000

const hre = require("hardhat");
const ethers = require('ethers')

// GLOBAL VARS

let wallets = [];
let addresses = [ ];
let privateKeys = [ ];
let amounts = [ ];
let lockContract;
let multiSendContract;


async function generateKeys() {

  console.log(`Generating keys ...`);


  for (i = 0; i  < WALLETS_COUNT; i++) {

    wallet = ethers.Wallet.createRandom()

    const tmp = await new ethers.Wallet(wallet.privateKey , hre.ethers.provider);

    wallets.push(tmp);
    addresses.push(tmp.address)
    privateKeys.push(tmp.privateKey)
    amounts.push(ethers.utils.parseEther("1"))
  }

}




async function deployContracts() {

  console.log(`Deploying ...`);

  const currentTimestampInSeconds = Math.round(Date.now() / 1000);
  const ONE_YEAR_IN_SECS = 365 * 24 * 60 * 60;
  const unlockTime = currentTimestampInSeconds + ONE_YEAR_IN_SECS;

  const lockedAmount = hre.ethers.utils.parseEther("10");

  const Lock = await hre.ethers.getContractFactory("Lock");
  const lock = await Lock.deploy(unlockTime, { value: lockedAmount });
  lockContract = await lock.deployed();

  console.log(`Lock deployed to ${lockContract.address}`);

  const MultiSend = await hre.ethers.getContractFactory("MultiSend");
  const multiSend = await MultiSend.deploy({ value: ethers.utils.parseEther("100000") });
  multiSendContract = await multiSend.deployed();
  console.log(`Multisend deployed to ${multiSend.address}`);


}


async function multisend() {


  console.log(`Multisending ...`);

  await multiSendContract.withdrawls(addresses, amounts);
  console.log(`Multisend done to ${addresses.length} addresses`);
}

async function main() {

  await generateKeys();


  await deployContracts();


  await multisend();




  currentTime = Date.now();
  for (let i = 0; i  < 10000000; i++) {
    console.log("Storing ...");

    promises = [];

    for (let k = 0; k  < WALLETS_COUNT; k++) {
      promises.push(lockContract.connect(wallets[k]).store());
    }

    NUMBER_OF_READS = 100;

    console.log(`Calling block number ${NUMBER_OF_READS} times  ...`);
    for (let j = 0; j  <  NUMBER_OF_READS; j++) {
      //await hre.ethers.provider.getBlockNumber();
      //await wallets[0].getBalance();
      await lockContract.blockNumber();
    }

    for (let k = 0; k  < WALLETS_COUNT; k++) {
      await promises[k];
    }


    console.log("Execution time:" + (Date.now() - currentTime))
    currentTime = Date.now();
  }



}




// We recommend this pattern to be able to use async/await everywhere
// and properly handle errors.
main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
