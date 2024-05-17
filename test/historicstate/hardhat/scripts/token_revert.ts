import {mkdir, readdir, writeFileSync, readFile, unlink} from "fs";

const fs = require('fs');
import {existsSync} from "fs";
import deepDiff, {diff} from 'deep-diff';
import {expect} from "chai";
import * as path from 'path';
import {int, string} from "hardhat/internal/core/params/argumentTypes";
import internal from "node:stream";
import exp from "node:constants";

const OWNER_ADDRESS: string = "0x907cd0881E50d359bb9Fd120B1A5A143b1C97De6";
const CALL_ADDRESS: string = "0xCe5c7ca85F8cB94FA284a303348ef42ADD23f5e7";

const ZERO_ADDRESS: string = '0x0000000000000000000000000000000000000000';

const SKALE_TRACES_DIR = "/tmp/skale_traces/"
const GETH_TRACES_DIR = "scripts/geth_traces/"

const DEFAULT_TRACER = "defaultTracer";
const CALL_TRACER = "callTracer";
const PRESTATE_TRACER = "prestateTracer";
const PRESTATEDIFF_TRACER = "prestateDiffTracer";
const FOURBYTE_TRACER = "4byteTracer";
const REPLAY_TRACER = "replayTracer";

var DEPLOYED_CONTRACT_ADDRESS_LOWER_CASE: string = "";
var globalCallCount = 0;


async function replaceAddressesWithSymbolicNames(_traceFileName: string) {

    let callAddressLowerCase = CALL_ADDRESS.toLowerCase();

    await replaceStringInFile(SKALE_TRACES_DIR + _traceFileName,
        callAddressLowerCase, "CALL.address");

    let ownerAddressLowerCase = OWNER_ADDRESS.toLowerCase();

    await replaceStringInFile(SKALE_TRACES_DIR + _traceFileName,
        ownerAddressLowerCase, "OWNER.address");

    // if the contract has been deployed, also replace contract address

    if (DEPLOYED_CONTRACT_ADDRESS_LOWER_CASE.length > 0) {
        await replaceStringInFile(SKALE_TRACES_DIR + _traceFileName,
            DEPLOYED_CONTRACT_ADDRESS_LOWER_CASE, TEST_CONTRACT_NAME + ".address");

    }


}

async function getTraceJsonOptions(_tracer: string): Promise<object> {
    if (_tracer == DEFAULT_TRACER) {
        return {disableStack: true};
    }

    if (_tracer == PRESTATEDIFF_TRACER) {
        return {"tracer": PRESTATE_TRACER, "tracerConfig": {diffMode: true}};
    }

    return {"tracer": _tracer}
}


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

    // Replace the string
    const transformedFile = data.replace(new RegExp(_str1, 'g'), _str2);

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



const TEST_CONTRACT_NAME = "ERC20Custom";

async function deployTestContract(): Promise<object> {

    console.log(`Deploying ` + TEST_CONTRACT_NAME);

    const factory = await ethers.getContractFactory(TEST_CONTRACT_NAME);
    const testContractName = await factory.deploy(
        "RevertTest", "RevertTest", {
        gasLimit: 2100000, // this is just an example value; you'll need to set an appropriate gas limit for your specific function call
    });
    const deployedTestContract = await testContractName.deployed();

    const deployReceipt = await ethers.provider.getTransactionReceipt(deployedTestContract.deployTransaction.hash)
    const deployBlockNumber: number = deployReceipt.blockNumber;

    const hash = deployedTestContract.deployTransaction.hash;
    console.log(`Contract deployed to ${deployedTestContract.address} at block ${deployBlockNumber.toString(16)} tx hash ${hash}`);

    return deployedTestContract;

}


function generateNewWallet() {
    const wallet = hre.ethers.Wallet.createRandom();
    console.log("Address:", wallet.address);
    return wallet;
}

function sleep(ms: number) {
    return new Promise(resolve => setTimeout(resolve, ms));
}


async function writeTraceFileReplacingAddressesWithSymbolicNames(_traceFileName: string, traceResult: string) {
    writeFileSync(SKALE_TRACES_DIR + _traceFileName, traceResult);
    await replaceAddressesWithSymbolicNames(_traceFileName);
}

async function getBlockTrace(blockNumber: number): Promise<String> {

    const blockStr = "0x" + blockNumber.toString(16);
    const params  : any = [blockStr, await getTraceJsonOptions(DEFAULT_TRACER)];
    console.log(params);
    const trace : any  = await ethers.provider.send('debug_traceBlockByNumber', params);

    return trace;
}

async function getAndPrintCommittedTransactionTrace(hash: string, _tracer: string, _skaleFileName: string): Promise<String> {
    globalCallCount++;

    let traceOptions = await getTraceJsonOptions(_tracer);




    console.log("Calling debug_traceTransaction to generate " + _skaleFileName);

    let trace;


    if (_tracer == DEFAULT_TRACER) {
        // test both empty tracer and now tracer
        if (globalCallCount % 2 === 0) {
            trace = await ethers.provider.send('debug_traceTransaction', [hash]);
        } else {
            trace = await ethers.provider.send('debug_traceTransaction', [hash, traceOptions]);
        }
    } else {
        trace = await ethers.provider.send('debug_traceTransaction', [hash, traceOptions]);
    }

    const result = JSON.stringify(trace, null, 4);

    await writeTraceFileReplacingAddressesWithSymbolicNames(_skaleFileName, result);

    return trace;
}

async function callDebugTraceCall(_deployedContract: any, _tracer: string, _traceFileName: string): Promise<void> {

    // first call function using eth_call

    const currentBlock = await hre.ethers.provider.getBlockNumber();

    const transaction = {
        from: CALL_ADDRESS,
        to: _deployedContract.address,
        data: _deployedContract.interface.encodeFunctionData("getBalance", [])
    };

    const returnData = await ethers.provider.call(transaction, currentBlock - 1);

    const result = _deployedContract.interface.decodeFunctionResult("getBalance", returnData);


    console.log("Calling debug_traceCall to generate  " + _traceFileName);

    let traceOptions = await getTraceJsonOptions(_tracer);

    const trace = await ethers.provider.send('debug_traceCall', [transaction, "latest", traceOptions]);

    const traceResult = JSON.stringify(trace, null, 4);
    await writeTraceFileReplacingAddressesWithSymbolicNames(_traceFileName, traceResult);
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

const EXECUTE_FUNCTION_NAME = "transfer";

const TEST_CONTRACT_EXECUTE_CALLTRACER_FILE_NAME = TEST_CONTRACT_NAME + "." + EXECUTE_FUNCTION_NAME + ".callTracer.json";


async function sendERCTransferWithoutConfirmation(deployedContract: any): Promise<int> {
    // Generate a new wallet
    const newWallet = generateNewWallet();

    await sleep(3000);  // Sleep for 1000 milliseconds (1 second)


    const currentNonce = await signer.getTransactionCount();

    // Define the transaction
    const tx = {
        to: deployedContract.address,
        gasLimit: 2100000,
        nonce: currentNonce,
        data: deployedContract.interface.encodeFunctionData(EXECUTE_FUNCTION_NAME, [CALL_ADDRESS, 10])
    };

    // Send the transaction and wait until it is submitted ot the queue
    const txResponse = signer.sendTransaction(tx);

    if (hre.network.name == "geth") {
        await txResponse;
    }

    console.log(`Submitted a tx to send 0.1 ETH to ${newWallet.address}`);

    return currentNonce;
}



async function executeERC20Transfer(deployedContract: any): Promise<string> {

    console.log("Doing transfer revert")

    // Get the first signer from Hardhat's local network
    const [signer] = await hre.ethers.getSigners();

    const currentNonce = await signer.getTransactionCount();

    const receipt = await deployedContract[EXECUTE_FUNCTION_NAME]( CALL_ADDRESS, 1,  {
        gasLimit: 0x6239, // this is just an example value; you'll need to set an appropriate gas limit for your specific function call
        nonce: currentNonce
    });

    expect(receipt.blockNumber).not.to.be.null;


    const trace: string = await getBlockTrace(receipt.blockNumber);

    expect(Array.isArray(trace));

    expect(trace.length).equal(1);

    console.log("Trace:" + JSON.stringify(trace[0]));

    return receipt.hash!;

}

async function verifyCallTraceAgainstGethTrace(_fileName: string) {

    console.log("Verifying " + _fileName);

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

            if (difference.kind == "E" && difference.path!.length == 3 && difference.path![0] == "calls") {
                let key = difference.path![2];
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

async function main(): Promise<void> {

    await deleteAndRecreateDirectory(SKALE_TRACES_DIR);

    let deployedContract = await deployTestContract();
    const deployHash = deployedContract.deployTransaction.hash;
    DEPLOYED_CONTRACT_ADDRESS_LOWER_CASE = deployedContract.address.toString().toLowerCase();

    sleep(10000);

    console.log("Balance before:" + await deployedContract.balanceOf(OWNER_ADDRESS));
    const failedTransferHash: string = await executeERC20Transfer(deployedContract);
    await getAndPrintCommittedTransactionTrace(failedTransferHash, CALL_TRACER, TEST_CONTRACT_EXECUTE_CALLTRACER_FILE_NAME);
    console.log("Balance after:" + await deployedContract.balanceOf(OWNER_ADDRESS));
    await verifyCallTraceAgainstGethTrace(TEST_CONTRACT_EXECUTE_CALLTRACER_FILE_NAME);

}


main().catch((error: any) => {
    console.error(error);
    process.exitCode = 1;
});
