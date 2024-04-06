const OWNER_ADDRESS: string = "0x907cd0881E50d359bb9Fd120B1A5A143b1C97De6";
const ZERO_ADDRESS: string = "0xO000000000000000000000000000000000000000";
const INITIAL_MINT: bigint = 10000000000000000000000000000000000000000;

import {ethers} from "hardhat";

async function waitUntilNextBlock() {

    const current = await hre.ethers.provider.getBlockNumber();
    let newBlock = current;
    console.log(`BLOCK_NUMBER ${current}`);

    while (newBlock == current) {
        newBlock = await hre.ethers.provider.getBlockNumber();
    }

    console.log(`BLOCK_NUMBER ${newBlock}`);

    return current;

}

function CHECK(result: any): void {
    if (!result) {
        const message: string = `Check failed ${result}`
        console.log(message);
        throw message;
    }
}

async function getAndPrintTrace(hash: string): Promise<String> {

    const trace = await ethers.provider.send('debug_traceTransaction', [hash, {}]);

    console.log(JSON.stringify(trace, null, 4));
    return trace;
}

async function deployWriteAndDestroy(): Promise<void> {

    console.log(`Deploying ...`);

    const Push0Test = await ethers.getContractFactory("Push0");
    const test = await Push0Test.deploy();
    const testContract = await test.deployed();


    const deployBn = await ethers.provider.getBlockNumber();

    const hash = testContract.deployTransaction.hash;
    console.log(`Gas limit ${testContract.deployTransaction.gasLimit}`);
    console.log(`Contract deployed to ${testContract.address} at block ${deployBn} tx hash ${hash}`);

    console.log(`Now testing`);


    for (let i = 0; i < 1000; i++) {

        console.log("Current Linux Time in Seconds: ", Math.floor(new Date().getTime() / 1000));

        const historicBlockNumber = '0x' + parseInt(deployBn).toString(16);

        // Encode the call to the `getZero` method
        const data2 = testContract.interface.encodeFunctionData("getZero", []);

        await waitUntilNextBlock();

        // Create the call transaction object
        const callTransaction = {
            to: testContract.address, // The address of the contract
            data: data2, // The encoded function call
        };

        // Use provider.call to execute the call at the specific historic block number
        await ethers.provider.call(callTransaction);
        await ethers.provider.call(callTransaction, historicBlockNumber);

    }
}



async function main(): Promise<void> {
    await deployWriteAndDestroy();
}

// We recommend this pattern to be able to use async/await everywhere
// and properly handle errors.
main().catch((error: any) => {
    console.error(error);
    process.exitCode = 1;
});