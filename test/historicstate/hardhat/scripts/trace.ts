import {ethers} from "hardhat";
import {readFile} from 'fs';
import {writeFileSync} from "fs";
import deepDiff, {diff} from 'deep-diff';
import {expect} from "chai";

const OWNER_ADDRESS: string = "0x907cd0881E50d359bb9Fd120B1A5A143b1C97De6";
const ZERO_ADDRESS: string = "0xO000000000000000000000000000000000000000";
const INITIAL_MINT: bigint = 10000000000000000000000000000000000000000n;
const SKALED_TRACE_FILE_NAME: string = "/tmp/skaled.trace.json"

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

async function getAndPrintBlockTrace(blockNumber: number): Promise<String> {

    const blockStr = "0x" + blockNumber.toString(16);
    const trace = await ethers.provider.send('debug_traceBlockByNumber', [blockStr, {}]);

    console.log(JSON.stringify(trace, null, 4));
    return trace;
}

async function getAndPrintTrace(hash: string): Promise<String> {
//    const trace = await ethers.provider.send('debug_traceTransaction', [hash, {"tracer":"prestateTracer",
//        "tracerConfig": {"diffMode":true}}]);

//    const trace = await ethers.provider.send('debug_traceTransaction', [hash, {"tracer": "callTracer",
//        "tracerConfig": {"withLog":true}}]);

    const trace = await ethers.provider.send('debug_traceTransaction', [hash, {}]);

    const result = JSON.stringify(trace, null, 4);
    writeFileSync(SKALED_TRACE_FILE_NAME, result);

    return trace;
}

async function deployAndMint(): Promise<void> {

    console.log(`Deploying ...`);

    const factory = await ethers.getContractFactory("Tracer");
    const tracer = await factory.deploy({
        gasLimit: 2100000, // this is just an example value; you'll need to set an appropriate gas limit for your specific function call
    });
    const deployedTracer = await tracer.deployed();
    const deployReceipt = await ethers.provider.getTransactionReceipt(deployedTracer.deployTransaction.hash)
    const deployBlockNumber: number = deployReceipt.blockNumber;
    const hash = deployedTracer.deployTransaction.hash;
    console.log(`Gas limit ${deployedTracer.deployTransaction.gasLimit}`);
    console.log(`Contract deployed to ${deployedTracer.address} at block ${deployBlockNumber.toString(16)} tx hash ${hash}`);


    // await waitUntilNextBlock()

    //await getAndPrintBlockTrace(deployBlockNumber);
    //await getAndPrintBlockTrace(deployBlockNumber);
    await getAndPrintTrace(hash)


    /* console.log(`Now minting`);

    const transferReceipt = await deployedTracer.mint(1000, {
        gasLimit: 2100000, // this is just an example value; you'll need to set an appropriate gas limit for your specific function call
    });
    console.log(`Gas limit ${transferReceipt.gasLimit}`);
*/
    //await getAndPrintBlockTrace(transferReceipt.blockNumber);
    //await getAndPrintBlockTrace(transferReceipt.blockNumber);
    //
    //  await getAndPrintTrace(transferReceipt.hash);

    /*

    console.log(`Now testing self-destruct`);

    const transferReceipt2 = await lockContract.die("0x690b9a9e9aa1c9db991c7721a92d351db4fac990");
    await transferReceipt2.wait();

    console.log(`Successfully self destructed`);

    console.log(`PASSED!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!`);


     */
}


function readJSONFile<T>(fileName: string): Promise<T> {
    return new Promise((resolve, reject) => {
        readFile(fileName, 'utf8', (err, data) => {
            if (err) {
                reject(`An error occurred while reading the file: ${err.message}`);
                return;
            }
            try {
                const obj: T = JSON.parse(data);
                resolve(obj);
            } catch (parseError) {
                reject(`Error parsing JSON: ${parseError.message}`);
            }
        });
    });
}

async function checkDiffs(_skaleTrace: any, _gethTrace: any): Promise<void> {

}

async function main(): Promise<void> {

    await deployAndMint();

    let expectedResult = await readJSONFile("scripts/tracer_contract_geth_trace.json")
    let actualResult = await readJSONFile(SKALED_TRACE_FILE_NAME)
    const differences = deepDiff(expectedResult, actualResult);

//    console.log('Differences:', differences);

    let foundDiffs = false;

    if (differences) {
        differences.forEach((difference, index) => {
            // do not print differences related to total gas
            if (difference.kind == "E" && difference.path.length == 3 && difference.path[2] == "gas") {
                return;
            }

            if (difference.kind == "E" && difference.path.length == 3 && difference.path[2] == "gasCost" &&
                expectedResult["structLogs"][difference.path[1]]["op"]  == "SLOAD") {
                return;
            }

            if (difference.kind == "E" && difference.path.length == 3 && difference.path[2] == "gasCost" &&
                expectedResult["structLogs"][difference.path[1]]["op"]  == "SSTORE") {
                return;
            }

            foundDiffs = true;
            if (difference.kind == "E") {
                console.log(`Difference op:`, expectedResult["structLogs"][difference.path[1]]);
            }
            console.log(`Difference ${index + 1}:`, difference.path);
            console.log(`Difference ${index + 1}:`, difference);
        });
    }
    ;

    await expect(foundDiffs).to.be.eq(false)


}


// We recommend this pattern to be able to use async/await everywhere
// and properly handle errors.
main().catch((error: any) => {
    console.error(error);
    process.exitCode = 1;
});
