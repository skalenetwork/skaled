const OWNER_ADDRESS: string = "0x907cd0881E50d359bb9Fd120B1A5A143b1C97De6";
const ZERO_ADDRESS: string = "0xO000000000000000000000000000000000000000";
const INITIAL_MINT: bigint = 10000000000000000000000000000000000000000n;

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
    console.log(trace);
    return trace;
}

async function deployWriteAndDestroy(): Promise<void> {

    console.log(`Deploying ...`);

    const Lock = await ethers.getContractFactory("Lock");
    const lock = await Lock.deploy();
    const lockContract = await lock.deployed();


    const deployBn = await ethers.provider.getBlockNumber();

    const hash = lockContract.deployTransaction.hash;
    console.log(`Gas limit ${lockContract.deployTransaction.gasLimit}`);
    console.log(`Contract deployed to ${lockContract.address} at block ${deployBn} tx hash ${hash}`);


    // await waitUntilNextBlock()

    await getAndPrintTrace(hash)


    console.log(`Now minting`);

    const transferReceipt = await lockContract.mint(1000);
    console.log(`Gas limit ${transferReceipt.gasLimit}`);

    await getAndPrintTrace(transferReceipt.hash);

    /*

console.log(`Now testing self-destruct`);

const transferReceipt2 = await lockContract.die("0x690b9a9e9aa1c9db991c7721a92d351db4fac990");
await transferReceipt2.wait();

console.log(`Successfully self destructed`);

console.log(`PASSED!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!`);


 */
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