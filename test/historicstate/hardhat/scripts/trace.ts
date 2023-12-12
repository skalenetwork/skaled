import {ethers} from "hardhat";
import {readFile} from 'fs';
import {writeFileSync} from "fs";
import deepDiff, {diff} from 'deep-diff';
import {expect} from "chai";
import {int} from "hardhat/internal/core/params/argumentTypes";

const OWNER_ADDRESS: string = "0x907cd0881E50d359bb9Fd120B1A5A143b1C97De6";
const ZERO_ADDRESS: string = "0xO000000000000000000000000000000000000000";
const INITIAL_MINT: bigint = 10000000000000000000000000000000000000000n;
const TEST_CONTRACT_NAME = "Tracer"
const RUN_FUNCTION_NAME = "mint"

const SKALED_TEST_CONTRACT_DEPLOY_FILE_NAME: string = "/tmp/" + TEST_CONTRACT_NAME + ".deploy.skaled.trace.json"
const SKALED_TEST_CONTRACT_RUN_FILE_NAME: string = "/tmp/"+ TEST_CONTRACT_NAME+ "." + RUN_FUNCTION_NAME + ".skaled.trace.json"
const GETH_TEST_CONTRACT_DEPLOY_FILE_NAME: string = "scripts/" + TEST_CONTRACT_NAME + ".deploy.geth.trace.json"
const GETH_TEST_CONTRACT_RUN_FILE_NAME: string = "scripts/"+ TEST_CONTRACT_NAME+ "." + RUN_FUNCTION_NAME + ".geth.trace.json"

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

async function getAndPrintTrace(hash: string, _skaleFileName: string): Promise<String> {
//    const trace = await ethers.provider.send('debug_traceTransaction', [hash, {"tracer":"prestateTracer",
//        "tracerConfig": {"diffMode":true}}]);

//    const trace = await ethers.provider.send('debug_traceTransaction', [hash, {"tracer": "callTracer",
//        "tracerConfig": {"withLog":true}}]);

    const trace = await ethers.provider.send('debug_traceTransaction', [hash, {}]);

    const result = JSON.stringify(trace, null, 4);
    writeFileSync(_skaleFileName, result);

    return trace;
}

async function deployTestContract(): Promise<object> {

    console.log(`Deploying ...`);

    const factory = await ethers.getContractFactory(TEST_CONTRACT_NAME);
    const testContractName = await factory.deploy({
        gasLimit: 2100000, // this is just an example value; you'll need to set an appropriate gas limit for your specific function call
    });
    const deployedTestContract = await testContractName.deployed();

    const deployReceipt = await ethers.provider.getTransactionReceipt(deployedTestContract.deployTransaction.hash)
    const deployBlockNumber: number = deployReceipt.blockNumber;
    const hash = deployedTestContract.deployTransaction.hash;
    console.log(`Gas limit ${deployedTestContract.deployTransaction.gasLimit}`);
    console.log(`Contract deployed to ${deployedTestContract.address} at block ${deployBlockNumber.toString(16)} tx hash ${hash}`);

    // await waitUntilNextBlock()

    //await getAndPrintBlockTrace(deployBlockNumber);
    //await getAndPrintBlockTrace(deployBlockNumber);
    await getAndPrintTrace(hash, SKALED_TEST_CONTRACT_DEPLOY_FILE_NAME);

    return deployedTestContract;

}

async function callTestContractRun(deployedContract: any): Promise<void> {

    console.log(`Minting ...`);

    const transferReceipt = await deployedContract[RUN_FUNCTION_NAME](1000, {
        gasLimit: 2100000, // this is just an example value; you'll need to set an appropriate gas limit for your specific function call
    });
    console.log(`Gas limit ${transferReceipt.gasLimit}`);

    //await getAndPrintBlockTrace(transferReceipt.blockNumber);
    //await getAndPrintBlockTrace(transferReceipt.blockNumber);
    //

    await getAndPrintTrace(transferReceipt.hash, SKALED_TEST_CONTRACT_RUN_FILE_NAME);

}

async function callDebugTraceCall(deployedContract: any): Promise<void> {


    // Example usage
    const transaction = {
        from: OWNER_ADDRESS,
        to: deployedContract.address,
        data: '0x0'   // Replace with the encoded contract method call
    };


    console.log(`Calling debug trace call ...`);
    console.log(transaction);


    const trace = await ethers.provider.send('debug_traceCall', [transaction, "latest", {}]);

    console.log(trace);

}



function readJSONFile(fileName: string): Promise<object> {
    return new Promise((resolve, reject) => {
        readFile(fileName, 'utf8', (err, data) => {
            if (err) {
                reject(`An error occurred while reading the file: ${err.message}`);
                return;
            }
            try {
                const obj: object = JSON.parse(data);
                resolve(obj);
            } catch (parseError: any) {
                reject(`Error parsing JSON: ${parseError.message}`);
            }
        });
    });
}


async function verifyTransactionTraceAgainstGethTrace(_expectedResultFileName: string, _actualResultFileName: string) {

    let expectedResult  = await readJSONFile(_expectedResultFileName)
    let actualResult = await readJSONFile(_actualResultFileName)

    verifyGasCalculations(actualResult);

    const differences = deepDiff(expectedResult, actualResult)!;

    let foundDiffs = false;

    if (differences) {
        differences.forEach((difference, index) => {
            // do not print differences related to total gas in the account
            if (difference.kind == "E" && difference.path!.length == 3 && difference.path![2] == "gas") {
                return;
            }

            if (difference.kind == "E" && difference.path!.length == 1 && difference.path![0] == "gas") {
                return;
            }


            if (difference.kind == "E" && difference.path!.length == 3 && difference.path![2] == "gasCost") {
                let op = expectedResult.structLogs[difference.path![1]]["op"];
                if (op == "SLOAD" || op == "SSTORE" || op == "EXTCODESIZE") {
                    return;
                }
            }


            foundDiffs = true;
            if (difference.kind == "E") {
                console.log(`Difference op:`, expectedResult["structLogs"][difference.path![1]]);
            }
            console.log(`Found difference (lhs is expected value) ${index + 1}:`, difference.path);
            console.log(`Difference ${index + 1}:`, difference);
        });
    }
    ;

    await expect(foundDiffs).to.be.eq(false)
}


async function verifyGasCalculations(_actualResult: any): Promise<void> {
    let structLogs: object[] = _actualResult.structLogs;
    expect(structLogs.length > 0)
    let gasRemaining: bigint = structLogs[0].gas
    let gasCost = structLogs[0].gasCost
    let totalGasUsedInOps: bigint = gasCost;

    for (let index = 1; index < structLogs.length; index++) {
        let newGasRemaining: bigint = structLogs[index].gas
        expect(gasRemaining - newGasRemaining).eq(gasCost);
        gasRemaining = newGasRemaining;
        gasCost = structLogs[index].gasCost
        totalGasUsedInOps += gasCost;
    }

    console.log("Total gas used in ops ", totalGasUsedInOps)
}


async function main(): Promise<void> {

    let deployedContract = await deployTestContract();

    await verifyTransactionTraceAgainstGethTrace(GETH_TEST_CONTRACT_DEPLOY_FILE_NAME,
        SKALED_TEST_CONTRACT_DEPLOY_FILE_NAME)

    await callTestContractRun(deployedContract);

    await verifyTransactionTraceAgainstGethTrace(GETH_TEST_CONTRACT_RUN_FILE_NAME,
        SKALED_TEST_CONTRACT_RUN_FILE_NAME)

    await callDebugTraceCall(deployedContract);



}


// We recommend this pattern to be able to use async/await everywhere
// and properly handle errors.
main().catch((error: any) => {
    console.error(error);
    process.exitCode = 1;
});
