import {mkdir, readdir, writeFileSync, readFile, unlink} from "fs";
const fs = require('fs');
import {existsSync} from "fs";
import deepDiff, {diff} from 'deep-diff';
import {expect} from "chai";
import * as path from 'path';
import {int, string} from "hardhat/internal/core/params/argumentTypes";

const OWNER_ADDRESS: string = "0x907cd0881E50d359bb9Fd120B1A5A143b1C97De6";
const ZERO_ADDRESS: string = "0xO000000000000000000000000000000000000000";
const INITIAL_MINT: bigint = 10000000000000000000000000000000000000000n;
const TEST_CONTRACT_NAME = "Tracer";
const RUN_FUNCTION_NAME = "mint";
const CALL_FUNCTION_NAME = "getBalance";

const SKALE_TRACES_DIR = "/tmp/skale_traces/"
const GETH_TRACES_DIR = "scripts/geth_traces/"

let DEFAULT_TRACER = "defaultTracer";
let CALL_TRACER =  "callTracer";
let PRESTATE_TRACER = "prestateTracer";
let FOUR_BYTE_TRACER = "4byteTracer";


async function getTraceJsonOptions(_tracer : string):Promise<object> {
    if (_tracer == DEFAULT_TRACER) {
        return {};
    }

    return {"tracer" : _tracer}
}

const TEST_CONTRACT_DEPLOY_FILE_NAME = TEST_CONTRACT_NAME + ".deploy.defaultTracer.json";
const TEST_CONTRACT_RUN_FILE_NAME = TEST_CONTRACT_NAME + "." + RUN_FUNCTION_NAME + ".defaultTracer.json";
const TEST_CONTRACT_CALL_DEFAULTTRACER_FILE_NAME = TEST_CONTRACT_NAME + "." + CALL_FUNCTION_NAME + ".defaultTracer.json";
const TEST_CONTRACT_CALL_CALLTRACER_FILE_NAME = TEST_CONTRACT_NAME + "." + CALL_FUNCTION_NAME + ".callTracer.json";
const TEST_CONTRACT_CALL_PRESTATETRACER_FILE_NAME = TEST_CONTRACT_NAME + "." + CALL_FUNCTION_NAME + ".prestateTracer.json";
const TEST_CONTRACT_CALL_FOURBYTETRACER_FILE_NAME = TEST_CONTRACT_NAME + "." + CALL_FUNCTION_NAME + ".4byteTracer.json";

async function deleteAndRecreateDirectory(dirPath: string): Promise<void> {
    try {
        // Remove the directory and its contents
        await deleteDirectory(dirPath);
    } catch (error) {
    }
    try {

        // Recreate the directory

        if (!fs.existsSync(dirPath)) {
            fs.mkdirSync(dirPath);
        }
        console.log(`Directory recreated: ${dirPath}`);
    } catch (error) {
        console.error('An error occurred:', error);
    }
}

async function deleteDirectory(dirPath: string): Promise<void> {
    if (!fs.existsSync(dirPath))
        return;
    const entries = fs.readdirSync(dirPath);

    // Iterate over directory contents
    for (const entry of entries) {
        const entryPath = path.join(dirPath, entry.name);

        if (entry.isDirectory()) {
            // Recursive call for nested directories
            await deleteDirectory(entryPath);
        } else {
            // Delete file
            await fs.unlink(entryPath);
        }
    }

    // Delete the now-empty directory
    await fs.rmdir(dirPath);
}


/**
 * Replaces a string in a file.
 * @param {string} _filePath - The path to the file.
 * @param {string} _str1 - The string to be replaced.
 * @param {string} _str2 - The string to replace with.
 */
async function replaceStringInFile(_filePath, _str1, _str2) {
    // Read the file

    let data = fs.readFileSync(_filePath, 'utf8');

    console.log("Replacing ...");

    // Replace the string
    const transformedFile = data.replace(new RegExp(_str1, 'g'), _str2);

    console.log("Replaced ...");

        // Write the file
    fs.writeFileSync(_filePath, transformedFile, 'utf8');


}


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

    console.log("Calling debug_traceTransaction to generate " + _skaleFileName );

    const trace = await ethers.provider.send('debug_traceTransaction', [hash, {}]);

    const result = JSON.stringify(trace, null, 4);
    writeFileSync(SKALE_TRACES_DIR + _skaleFileName, result);

    return trace;
}

async function deployTestContract(): Promise<object> {

    console.log(`Deploying ` + TEST_CONTRACT_NAME);

    const factory = await ethers.getContractFactory(TEST_CONTRACT_NAME);
    const testContractName = await factory.deploy({
        gasLimit: 2100000, // this is just an example value; you'll need to set an appropriate gas limit for your specific function call
    });
    const deployedTestContract = await testContractName.deployed();

    const deployReceipt = await ethers.provider.getTransactionReceipt(deployedTestContract.deployTransaction.hash)
    const deployBlockNumber: number = deployReceipt.blockNumber;
    const hash = deployedTestContract.deployTransaction.hash;
    console.log(`Contract deployed to ${deployedTestContract.address} at block ${deployBlockNumber.toString(16)} tx hash ${hash}`);

    // await waitUntilNextBlock()

    //await getAndPrintBlockTrace(deployBlockNumber);
    //await getAndPrintBlockTrace(deployBlockNumber);
    await getAndPrintTrace(hash, TEST_CONTRACT_DEPLOY_FILE_NAME);

    return deployedTestContract;

}

async function callTestContractRun(deployedContract: any): Promise<void> {


    const transferReceipt = await deployedContract[RUN_FUNCTION_NAME](1000, {
        gasLimit: 2100000, // this is just an example value; you'll need to set an appropriate gas limit for your specific function call
    });

    //await getAndPrintBlockTrace(transferReceipt.blockNumber);
    //await getAndPrintBlockTrace(transferReceipt.blockNumber);
    //

    await getAndPrintTrace(transferReceipt.hash, TEST_CONTRACT_RUN_FILE_NAME);

}

async function callDebugTraceCall(_deployedContract: any, _tracer: string, _traceFileName: string): Promise<void> {

    // first call function using eth_call

    const currentBlock = await hre.ethers.provider.getBlockNumber();

    const transaction = {
        from: OWNER_ADDRESS,
        to: _deployedContract.address,
        data: _deployedContract.interface.encodeFunctionData("getBalance", [])
    };

    const returnData = await ethers.provider.call(transaction, currentBlock - 1);

    const result = _deployedContract.interface.decodeFunctionResult("getBalance", returnData);

    // Example usage

    console.log("Calling debug_traceCall to generate and verify " + _traceFileName);

    let traceOptions = await getTraceJsonOptions(_tracer);

    const trace = await ethers.provider.send('debug_traceCall', [transaction, "latest", traceOptions]);

    const traceResult = JSON.stringify(trace, null, 4);

    writeFileSync(SKALE_TRACES_DIR + _traceFileName, traceResult);

    let deployedContractAddressLowerCase  = _deployedContract.address.toString().toLowerCase();
    await replaceStringInFile(SKALE_TRACES_DIR + _traceFileName ,
        deployedContractAddressLowerCase, TEST_CONTRACT_NAME + ".address");

    let ownerAddressLowerCase  = OWNER_ADDRESS.toLowerCase();

    await replaceStringInFile(SKALE_TRACES_DIR + _traceFileName ,
        ownerAddressLowerCase,  "OWNER.address");


}


async function readJSONFile(fileName: string): Promise<object> {
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


async function verifyDefaultTraceAgainstGethTrace(_fileName: string) {

    const _expectedResultFileName = GETH_TRACES_DIR + _fileName;
    const _actualResultFileName = SKALE_TRACES_DIR + _fileName;

    let expectedResult = await readJSONFile(_expectedResultFileName)
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
            console.log(`Found difference (lhs is expected value) ${index + 1} at path:`, difference.path);
            console.log(`Difference ${index + 1}:`, difference);
        });
    }
    ;

    await expect(foundDiffs).to.be.eq(false)
}


async function verifyPrestateTraceAgainstGethTrace(_fileName: string) {

    const _expectedResultFileName = GETH_TRACES_DIR + _fileName;
    const _actualResultFileName = SKALE_TRACES_DIR + _fileName;

    let expectedResult = await readJSONFile(_expectedResultFileName)
    let actualResult = await readJSONFile(_actualResultFileName)

    const differences = deepDiff(expectedResult, actualResult)!;

    let foundDiffs = false;


    if (differences) {
        differences.forEach((difference, index) => {
            // do not print differences related to total gas in the account

            if (difference.kind == "E" && difference.path!.length == 1) {
                let key = difference.path![0];
                if (key == "to" || key == "gas" || key == "gasUsed")
                    return;
            }

            foundDiffs = true;

            console.log(`Found difference (lhs is expected value) ${index + 1} at path:`, difference.path);
            console.log(`Difference ${index + 1}:`, difference);
        });
    }


    await expect(foundDiffs).to.be.eq(false)
}



async function verifyCallTraceAgainstGethTrace(_fileName: string) {

    const _expectedResultFileName = GETH_TRACES_DIR + _fileName;
    const _actualResultFileName = SKALE_TRACES_DIR + _fileName;

    let expectedResult = await readJSONFile(_expectedResultFileName)
    let actualResult = await readJSONFile(_actualResultFileName)

    const differences = deepDiff(expectedResult, actualResult)!;

    let foundDiffs = false;


    if (differences) {
        differences.forEach((difference, index) => {
            // do not print differences related to total gas in the account

            if (difference.kind == "E" && difference.path!.length == 1) {
                let key = difference.path![0];
                if (key == "to" || key == "gas" || key == "gasUsed")
                    return;
            }

            foundDiffs = true;

            console.log(`Found difference (lhs is expected value) ${index + 1} at path:`, difference.path);
            console.log(`Difference ${index + 1}:`, difference);
        });
    }


    await expect(foundDiffs).to.be.eq(false)
}

async function verifyFourByteTraceAgainstGethTrace(_fileName: string) {

    const _expectedResultFileName = GETH_TRACES_DIR + _fileName;
    const _actualResultFileName = SKALE_TRACES_DIR + _fileName;

    let expectedResult = await readJSONFile(_expectedResultFileName)
    let actualResult = await readJSONFile(_actualResultFileName)

    const differences = deepDiff(expectedResult, actualResult)!;

    let foundDiffs = false;


    if (differences) {
        differences.forEach((difference, index) => {
            // do not print differences related to total gas in the account

            if (difference.kind == "E" && difference.path!.length == 2) {
                if ( difference.path![0] == "difference.path![0]" || key == "gas" || key == "gasUsed")
                    return;
            }

            foundDiffs = true;

            console.log(`Found difference (lhs is expected value) ${index + 1} at path:`, difference.path);
            console.log(`Difference ${index + 1}:`, difference);
        });
    }


    await expect(foundDiffs).to.be.eq(false)
}





async function verifyPrestateTraceAgainstGethTrace(_fileName: string) {

    const _expectedResultFileName = GETH_TRACES_DIR + _fileName;
    const _actualResultFileName = SKALE_TRACES_DIR + _fileName;

    let expectedResult = await readJSONFile(_expectedResultFileName)
    let actualResult = await readJSONFile(_actualResultFileName)

    const differences = deepDiff(expectedResult, actualResult)!;

    let foundDiffs = false;


    if (differences) {
        differences.forEach((difference, index) => {
            // do not print differences related to total gas in the account

            if (difference.kind == "E" && difference.path!.length == 2) {
                if ( difference.path![0] == "OWNER.address" || difference.path![1] == "nonce")
                    return;
            }


            foundDiffs = true;

            console.log(`Found difference (lhs is expected value) ${index + 1} at path:`, difference.path);
            console.log(`Difference ${index + 1}:`, difference);
        });
    }


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
}


async function main(): Promise<void> {

    expect(existsSync(GETH_TRACES_DIR + TEST_CONTRACT_DEPLOY_FILE_NAME));
    expect(existsSync(GETH_TRACES_DIR + TEST_CONTRACT_RUN_FILE_NAME));
    expect(existsSync(GETH_TRACES_DIR + TEST_CONTRACT_CALL_DEFAULTTRACER_FILE_NAME));

    await deleteAndRecreateDirectory(SKALE_TRACES_DIR);

    let deployedContract = await deployTestContract();

    await verifyDefaultTraceAgainstGethTrace(TEST_CONTRACT_DEPLOY_FILE_NAME)

    await callTestContractRun(deployedContract);

    await verifyDefaultTraceAgainstGethTrace(TEST_CONTRACT_RUN_FILE_NAME)

    await callDebugTraceCall(deployedContract, DEFAULT_TRACER, TEST_CONTRACT_CALL_DEFAULTTRACER_FILE_NAME);

    await verifyDefaultTraceAgainstGethTrace(TEST_CONTRACT_CALL_DEFAULTTRACER_FILE_NAME)

    await callDebugTraceCall(deployedContract, CALL_TRACER, TEST_CONTRACT_CALL_CALLTRACER_FILE_NAME);

    await verifyCallTraceAgainstGethTrace(TEST_CONTRACT_CALL_CALLTRACER_FILE_NAME)


    await callDebugTraceCall(deployedContract, FOUR_BYTE_TRACER, TEST_CONTRACT_CALL_FOURBYTETRACER_FILE_NAME);

    await verifyFourByteTraceAgainstGethTrace(TEST_CONTRACT_CALL_FOURBYTETRACER_FILE_NAME)

    await callDebugTraceCall(deployedContract, PRESTATE_TRACER, TEST_CONTRACT_CALL_PRESTATETRACER_FILE_NAME);

    // since deployed contract address changes from one test run to another we replace the
    // contract address with contract name. This is done to be able to compare trace files.


    await verifyPrestateTraceAgainstGethTrace(TEST_CONTRACT_CALL_PRESTATETRACER_FILE_NAME)

}


// We recommend this pattern to be able to use async/await everywhere
// and properly handle errors.
main().catch((error: any) => {
    console.error(error);
    process.exitCode = 1;
});
