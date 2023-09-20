// We require the Hardhat Runtime Environment explicitly here. This is optional
// but useful for running the script in a standalone fashion through `node <script>`.
//
// You can also run a script with `npx hardhat run <script>`. If you do that, Hardhat
// will compile your contracts, add the Hardhat Runtime Environment's members to the
// global scope, and execute the script.


WALLETS_COUNT = 10;
OWNER_ADDRESS = "0x907cd0881E50d359bb9Fd120B1A5A143b1C97De6";
ZERO_ADDRESS =  "0xO000000000000000000000000000000000000000";
INITIAL_MINT = 10000000000000000000000000000000000000000;

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



function CHECK(result) {
    if (!result) {
        message = `Check failed ${result}`
        console.log(message);
        throw message;
    }
}

async function waitUntilNextBlock() {

    current = await hre.ethers.provider.getBlockNumber();
    newBlock = current;
    console.log(`BLOCK_NUMBER ${current}`);

    while (newBlock == current) {
        newBlock = await hre.ethers.provider.getBlockNumber();
    }

    console.log(`BLOCK_NUMBER ${newBlock}`);

    return current;


}


async function deployContractsProxy() {

    console.log(`Deploying ...`);

    const currentTimestampInSeconds = Math.round(Date.now() / 1000);

    //const lockedAmount = hre.ethers.utils.parseEther("10");

    console.log(`Contract deploy`);

    const Lock = await hre.ethers.getContractFactory("Lock");
    const lock = await Lock.deploy();

    lockContract = await lock.deployed();

    deployBn = await hre.ethers.provider.getBlockNumber();

    console.log(`Contract deployed to ${lockContract.address} at block ${deployBn}`);


    previousBlock =  await waitUntilNextBlock();

    console.log(`Now testing self-destruct`);

    transferReceipt2 = await lockContract.die("0x690b9a9e9aa1c9db991c7721a92d351db4fac991");
    await transferReceipt2.wait();

    console.log(`Successfully self destructed`);

    previousBlock =  await waitUntilNextBlock();


    console.log(`PASSED!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!`);

}



async function multisend() {


    console.log(`Multisending ...`);

    await multiSendContract.withdrawls(addresses, amounts);
    console.log(`Multisend done to ${addresses.length} addresses`);
}

async function main() {

    await generateOrReadKeys();


    await deployContractsProxy();


}


// We recommend this pattern to be able to use async/await everywhere
// and properly handle errors.
main().catch((error) => {
    console.error(error);
    process.exitCode = 1;
});
