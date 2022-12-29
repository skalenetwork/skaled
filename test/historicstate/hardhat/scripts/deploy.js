// We require the Hardhat Runtime Environment explicitly here. This is optional
// but useful for running the script in a standalone fashion through `node <script>`.
//
// You can also run a script with `npx hardhat run <script>`. If you do that, Hardhat
// will compile your contracts, add the Hardhat Runtime Environment's members to the
// global scope, and execute the script.


WALLETS_COUNT = 10;
OWNER_ADDRESS = "0x907cd0881E50d359bb9Fd120B1A5A143b1C97De6";

require("hardhat");
const ethers2 = require('ethers')
const fs = require('fs')
const waitForUserInput = require("wait-for-user-input");
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

        const tmp = await new ethers2.Wallet(privateKeys[i], ethers.provider);

        wallets.push(tmp);
        addresses.push(tmp.address)
        amounts.push(ethers.utils.parseEther("1"))
    }

}


async function deployContracts() {



    console.log(`Deploying ...`);

    const currentTimestampInSeconds = Math.round(Date.now() / 1000);

    const lockedAmount = ethers.utils.parseEther("10");

    const Lock = await ethers.getContractFactory("Lock");
    const lock = await Lock.deploy({value: lockedAmount});
    lockContract = await lock.deployed();
    deployBn = await ethers.provider.getBlockNumber();

    console.log(`Lock deployed to ${lockContract.address} at block ${deployBn}`);

   //b = await lockContract.balanceOf(OWNER_ADDRESS);
   //console.log(`Contract balance before transfer: ${b}`);

   transferReceipt = await lockContract.transfer(
        "0x690b9a9e9aa1c9db991c7721a92d351db4fac990", 0x02);
    await transferReceipt.wait();
    transferBn = await hre.ethers.provider.getBlockNumber();

    console.log('Transferred: ${b} at ${transferBn}');

    await delay(30);

    b = await lockContract.balanceOf(OWNER_ADDRESS, {blockTag : transferBn});

    console.log(`Contract balance after transform: ${b}`);


    b = await lockContract.balanceOf(OWNER_ADDRESS);

    b = await lockContract.balanceOf(OWNER_ADDRESS, {blockTag : transferBn - 1});
    console.log(`Contract balance before transform: ${b}`);

    const MultiSend = await hre.ethers.getContractFactory("MultiSend");
    const multiSend = await MultiSend.deploy({value: ethers.utils.parseEther("100000")});
    multiSendContract = await multiSend.deployed();
    console.log(`Multisend deployed to ${multiSend.address}`);


}

async function deployContractsProxy() {

    console.log(`Deploying ...`);

    const currentTimestampInSeconds = Math.round(Date.now() / 1000);

    const lockedAmount = hre.ethers.utils.parseEther("10");

    const Lock = await hre.ethers.getContractFactory("Lock");

    const box = await hre.upgrades.deployProxy(Lock);

    const lock = await Lock.deploy();
    lockContract = await lock.deployed();
    deployBn = await hre.ethers.provider.getBlockNumber();

    console.log(`Lock deployed to ${lockContract.address} at block ${deployBn}`);

    //b = await lockContract.balanceOf(OWNER_ADDRESS);
    //console.log(`Contract balance before transfer: ${b}`);

    transferReceipt = await lockContract.transfer(
        "0x690b9a9e9aa1c9db991c7721a92d351db4fac990", 0x02);
    await transferReceipt.wait();
    transferBn = await hre.ethers.provider.getBlockNumber();

    console.log('Transferred: ${b} at ${transferBn}');

    await delay(30);

    b = await lockContract.balanceOf(OWNER_ADDRESS, {blockTag : transferBn});

    console.log(`Contract balance after transform: ${b}`);


    b = await lockContract.balanceOf(OWNER_ADDRESS);

    b = await lockContract.balanceOf(OWNER_ADDRESS, {blockTag : transferBn - 1});
    console.log(`Contract balance before transform: ${b}`);

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


    await deployContractsProxy();


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
        for (let j = 0; j < 1; j++) {
            //await hre.ethers.provider.getBlockNumber();
            balance = await wallets[0].getBalance();
            //await lockContract.blockNumber();
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
