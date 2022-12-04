// We require the Hardhat Runtime Environment explicitly here. This is optional
// but useful for running the script in a standalone fashion through `node <script>`.
//
// You can also run a script with `npx hardhat run <script>`. If you do that, Hardhat
// will compile your contracts, add the Hardhat Runtime Environment's members to the
// global scope, and execute the script.


WALLETS_COUNT = 1000;

const hre = require("hardhat");
const ethers = require('ethers')
const fs = require('fs')
const KEY_DIR = "/tmp/tmp_test_keys"

// GLOBAL VARS

let wallets = [];
let addresses = [];
let privateKeys = [];
let amounts = [];
let lockContract;
let multiSendContract;


function delay(time) {
    return new Promise(resolve => setTimeout(resolve, time));
}

async function readKeys() {
    if (!fs.existsSync(KEY_DIR)) {
        console.log(`No pre-generated keys found.`);
        return false;
    }

    await console.log(`Reading keys ...`);
    privateKeys =  fs.readdirSync(KEY_DIR);

    console.log(`Found ${privateKeys.length} keys`)

    return (privateKeys.length == WALLETS_COUNT);
}

async function generateKeys() {
    console.log(`Generating keys ...`);
    if (fs.existsSync(KEY_DIR))
        fs.rmdirSync(KEY_DIR, { recursive: true });

    fs.mkdirSync(KEY_DIR);

    for (i = 0; i < WALLETS_COUNT; i++) {

        wallet = ethers.Wallet.createRandom()
        privateKeys.push(wallet.privateKey);
        const filePath = KEY_DIR + "/" + wallet.privateKey.toString();
        fs.closeSync(fs.openSync(filePath, 'w'));
    }

}

async function generateOrReadKeys() {

    haveKeys = await readKeys();

    if (!haveKeys) {
        await generateKeys()
    }


    console.log(`Initing wallets ...`);

    for (i = 0; i < WALLETS_COUNT; i++) {

        const tmp = await new ethers.Wallet(privateKeys[i], hre.ethers.provider);

        wallets.push(tmp);
        addresses.push(tmp.address)
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
    const lock = await Lock.deploy(unlockTime, {value: lockedAmount});
    lockContract = await lock.deployed();

    console.log(`Lock deployed to ${lockContract.address}`);

    const MultiSend = await hre.ethers.getContractFactory("MultiSend");
    const multiSend = await MultiSend.deploy({value: ethers.utils.parseEther("100000")});
    multiSendContract = await multiSend.deployed();
    console.log(`Multisend deployed to ${multiSend.address}`);


}


async function multisend() {


    console.log(`Multisending ...`);

    await multiSendContract.withdrawls(addresses, amounts);
    console.log(`Multisend done to ${addresses.length} addresses`);
}

async function main() {

    await generateOrReadKeys();


    await deployContracts();


    await multisend();


    currentTime = Date.now();
    for (let i = 0; i < 10000000; i++) {
        console.log("Sending batch of transactions  ...");

        promises = [];

        for (let k = 0; k < WALLETS_COUNT; k++) {
            promises.push(lockContract.connect(wallets[k]).store());
        }

        console.log("Sent");


        for (let k = 0; k < WALLETS_COUNT; k++) {
            await promises[k];
        }

        NUMBER_OF_READS = 100;

        console.log(`Calling block number ${NUMBER_OF_READS} times  ...`);
        for (let j = 0; j < NUMBER_OF_READS; j++) {
            //await hre.ethers.provider.getBlockNumber();
            //await wallets[0].getBalance();
            await lockContract.blockNumber();
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
