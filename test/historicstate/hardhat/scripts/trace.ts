import {mkdir, readdir, writeFileSync, readFile, unlink} from "fs";

const fs = require('fs');
import {existsSync} from "fs";
import deepDiff, {diff} from 'deep-diff';
import {expect} from "chai";
import * as path from 'path';
import {int, string} from "hardhat/internal/core/params/argumentTypes";
import internal from "node:stream";

const OWNER_ADDRESS: string = "0x907cd0881E50d359bb9Fd120B1A5A143b1C97De6";
const CALL_ADDRESS: string = "0xCe5c7ca85F8cB94FA284a303348ef42ADD23f5e7";

const ZERO_ADDRESS: string = '0x0000000000000000000000000000000000000000';
const INITIAL_MINT: bigint = 10000000000000000000000000000000000000000n;
const TEST_CONTRACT_NAME = "Tracer";
const EXECUTE_FUNCTION_NAME = "mint";
const EXECUTE2_FUNCTION_NAME = "mint2";
const EXECUTE3_FUNCTION_NAME = "readableRevert";
const CALL_FUNCTION_NAME = "getBalance";

const SKALE_TRACES_DIR = "/tmp/skale_traces/"
const GETH_TRACES_DIR = "scripts/geth_traces/"

const DEFAULT_TRACER = "defaultTracer";
const CALL_TRACER = "callTracer";
const PRESTATE_TRACER = "prestateTracer";
const PRESTATEDIFF_TRACER = "prestateDiffTracer";
const FOURBYTE_TRACER = "4byteTracer";
const REPLAY_TRACER = "replayTracer";


const TEST_DEPLOY_DEFAULTTRACER_FILE_NAME = TEST_CONTRACT_NAME + ".deploy.defaultTracer.json";
const TEST_DEPLOY_CALLTRACER_FILE_NAME = TEST_CONTRACT_NAME + ".deploy.callTracer.json";
const TEST_DEPLOY_PRESTATETRACER_FILE_NAME = TEST_CONTRACT_NAME + ".deploy.prestateTracer.json";
const TEST_DEPLOY_PRESTATEDIFFTRACER_FILE_NAME = TEST_CONTRACT_NAME + ".deploy.prestateDiffTracer.json";
const TEST_DEPLOY_FOURBYTETRACER_FILE_NAME = TEST_CONTRACT_NAME + ".deploy.4byteTracer.json";


const TEST_CONTRACT_EXECUTE_DEFAULTTRACER_FILE_NAME = TEST_CONTRACT_NAME + "." + EXECUTE_FUNCTION_NAME + ".defaultTracer.json";
const TEST_CONTRACT_EXECUTE_CALLTRACER_FILE_NAME = TEST_CONTRACT_NAME + "." + EXECUTE_FUNCTION_NAME + ".callTracer.json";
const TEST_CONTRACT_EXECUTE_PRESTATETRACER_FILE_NAME = TEST_CONTRACT_NAME + "." + EXECUTE_FUNCTION_NAME + ".prestateTracer.json";
const TEST_CONTRACT_EXECUTE_PRESTATEDIFFTRACER_FILE_NAME = TEST_CONTRACT_NAME + "." + EXECUTE_FUNCTION_NAME + ".prestateDiffTracer.json";
const TEST_CONTRACT_EXECUTE_FOURBYTETRACER_FILE_NAME = TEST_CONTRACT_NAME + "." + EXECUTE_FUNCTION_NAME + ".4byteTracer.json";


const TEST_CONTRACT_EXECUTE2_DEFAULTTRACER_FILE_NAME = TEST_CONTRACT_NAME + "." + EXECUTE2_FUNCTION_NAME + ".defaultTracer.json";
const TEST_CONTRACT_EXECUTE2_CALLTRACER_FILE_NAME = TEST_CONTRACT_NAME + "." + EXECUTE2_FUNCTION_NAME + ".callTracer.json";
const TEST_CONTRACT_EXECUTE2_PRESTATETRACER_FILE_NAME = TEST_CONTRACT_NAME + "." + EXECUTE2_FUNCTION_NAME + ".prestateTracer.json";
const TEST_CONTRACT_EXECUTE2_PRESTATEDIFFTRACER_FILE_NAME = TEST_CONTRACT_NAME + "." + EXECUTE2_FUNCTION_NAME + ".prestateDiffTracer.json";
const TEST_CONTRACT_EXECUTE2_FOURBYTETRACER_FILE_NAME = TEST_CONTRACT_NAME + "." + EXECUTE2_FUNCTION_NAME + ".4byteTracer.json";

const TEST_CONTRACT_EXECUTE3_DEFAULTTRACER_FILE_NAME = TEST_CONTRACT_NAME + "." + EXECUTE3_FUNCTION_NAME + ".defaultTracer.json";
const TEST_CONTRACT_EXECUTE3_CALLTRACER_FILE_NAME = TEST_CONTRACT_NAME + "." + EXECUTE3_FUNCTION_NAME + ".callTracer.json";
const TEST_CONTRACT_EXECUTE3_PRESTATETRACER_FILE_NAME = TEST_CONTRACT_NAME + "." + EXECUTE3_FUNCTION_NAME + ".prestateTracer.json";
const TEST_CONTRACT_EXECUTE3_PRESTATEDIFFTRACER_FILE_NAME = TEST_CONTRACT_NAME + "." + EXECUTE3_FUNCTION_NAME + ".prestateDiffTracer.json";
const TEST_CONTRACT_EXECUTE3_FOURBYTETRACER_FILE_NAME = TEST_CONTRACT_NAME + "." + EXECUTE3_FUNCTION_NAME + ".4byteTracer.json";


const TEST_TRANSFER_DEFAULTTRACER_FILE_NAME = TEST_CONTRACT_NAME + ".transfer.defaultTracer.json";
const TEST_TRANSFER_CALLTRACER_FILE_NAME = TEST_CONTRACT_NAME + ".transfer.callTracer.json";
const TEST_TRANSFER_PRESTATETRACER_FILE_NAME = TEST_CONTRACT_NAME + ".transfer.prestateTracer.json";
const TEST_TRANSFER_PRESTATEDIFFTRACER_FILE_NAME = TEST_CONTRACT_NAME + ".transfer.prestateDiffTracer.json";
const TEST_TRANSFER_FOURBYTETRACER_FILE_NAME = TEST_CONTRACT_NAME + ".transfer.4byteTracer.json";


const TEST_CONTRACT_CALL_DEFAULTTRACER_FILE_NAME = TEST_CONTRACT_NAME + "." + CALL_FUNCTION_NAME + ".defaultTracer.json";
const TEST_CONTRACT_CALL_CALLTRACER_FILE_NAME = TEST_CONTRACT_NAME + "." + CALL_FUNCTION_NAME + ".callTracer.json";
const TEST_CONTRACT_CALL_PRESTATETRACER_FILE_NAME = TEST_CONTRACT_NAME + "." + CALL_FUNCTION_NAME + ".prestateTracer.json";
const TEST_CONTRACT_CALL_PRESTATEDIFFTRACER_FILE_NAME = TEST_CONTRACT_NAME + "." + CALL_FUNCTION_NAME + ".prestateDiffTracer.json";
const TEST_CONTRACT_CALL_FOURBYTETRACER_FILE_NAME = TEST_CONTRACT_NAME + "." + CALL_FUNCTION_NAME + ".4byteTracer.json";
const TEST_CONTRACT_CALL_REPLAYTRACER_FILE_NAME = TEST_CONTRACT_NAME + "." + CALL_FUNCTION_NAME + ".replayTracer.json";

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
        return {};
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

function CHECK(result: any): void {
    if (!result) {
        const message: string = `Check failed ${result}`
        console.log(message);
        throw message;
    }


}

async function getBlockTrace(blockNumber: number): Promise<String> {

    const blockStr = "0x" + blockNumber.toString(16);
    //trace both empty tracer and no tracer
    let trace = await ethers.provider.send('debug_traceBlockByNumber', [blockStr]);
    trace = await ethers.provider.send('debug_traceBlockByNumber', [blockStr, {}]);

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


async function sendMoneyWithoutConfirmation(): Promise<int> {
    // Generate a new wallet
    const newWallet = generateNewWallet();

    await sleep(3000);  // Sleep for 1000 milliseconds (1 second)

    // Get the first signer from Hardhat's local network
    const [signer] = await hre.ethers.getSigners();

    const currentNonce = await signer.getTransactionCount();

    // Define the transaction
    const tx = {
        to: newWallet.address,
        value: hre.ethers.utils.parseEther("0.1"),
        nonce: currentNonce
    };

    // Send the transaction and wait until it is submitted ot the queue
    const txResponse = signer.sendTransaction(tx);

    if (hre.network.name == "geth") {
        await txResponse;
    }

    console.log(`Submitted a tx to send 0.1 ETH to ${newWallet.address}`);

    return currentNonce;
}

async function sendTransferWithConfirmation(): Promise<string> {
    // Generate a new wallet
    const newWallet = generateNewWallet();

    await sleep(3000);  // Sleep for 1000 milliseconds (1 second)

    // Get the first signer from Hardhat's local network
    const [signer] = await hre.ethers.getSigners();

    const currentNonce = await signer.getTransactionCount();

    // Define the transaction
    const tx = {
        to: "0x388C818CA8B9251b393131C08a736A67ccB19297",
        value: hre.ethers.utils.parseEther("0.1"),
    };

    // Send the transaction and wait until it is submitted ot the queue
    const txResponse = await signer.sendTransaction(tx);
    const txReceipt = await txResponse.wait();

    console.log(`Submitted a tx to send 0.1 ETH to ${newWallet.address}`);

    return txReceipt.transactionHash!;
}


async function executeTransferAndThenTestContractMintInSingleBlock(deployedContract: any): Promise<string> {

    let currentNonce: int = await sendMoneyWithoutConfirmation();

    const mintReceipt = await deployedContract[EXECUTE_FUNCTION_NAME](1000, {
        gasLimit: 2100000, // this is just an example value; you'll need to set an appropriate gas limit for your specific function call
        nonce: currentNonce + 1,
    });

    expect(mintReceipt.blockNumber).not.to.be.null;


    const trace: string = await getBlockTrace(mintReceipt.blockNumber);


    expect(Array.isArray(trace));

    // the array should have two elements
    if (hre.network.name != "geth") {
        expect(trace.length == 2);
    }

    return mintReceipt.hash!;

}

async function executeMint2(deployedContract: any): Promise<string> {


    const mint2Receipt = await deployedContract[EXECUTE2_FUNCTION_NAME](1000, {
        gasLimit: 2100000, // this is just an example value; you'll need to set an appropriate gas limit for your specific function call,
    });

    expect(mint2Receipt.blockNumber).not.to.be.null;


    return mint2Receipt.hash!;

}


async function executeRevert(deployedContract: any): Promise<string> {

    const revertReceipt = await deployedContract[EXECUTE3_FUNCTION_NAME](1000, {
        gasLimit: 2100000, // this is just an example value; you'll need to set an appropriate gas limit for your specific function call,
    });

    expect(revertReceipt.blockNumber).not.to.be.null;


    return revertReceipt.hash!;

}


async function writeTraceFileReplacingAddressesWithSymbolicNames(_traceFileName: string, traceResult: string) {
    writeFileSync(SKALE_TRACES_DIR + _traceFileName, traceResult);
    await replaceAddressesWithSymbolicNames(_traceFileName);
}

async function callDebugTraceCall(_deployedContract: any, _tracer: string, _traceFileName: string): Promise<void> {

    // first call function using eth_call

    const currentBlock = await hre.ethers.provider.getBlockNumber();

    const transaction = {
        from: CALL_ADDRESS,
        to: _deployedContract.address,
        data: _deployedContract.interface.encodeFunctionData("getBalance", [])
    };

    let returnData = await ethers.provider.call(transaction, currentBlock - 1);

    console.log("Calling debug_traceCall to generate  " + _traceFileName);

    let traceOptions = await getTraceJsonOptions(_tracer);

    const trace = await ethers.provider.send('debug_traceCall', [transaction, "latest", traceOptions]);

    const traceResult = JSON.stringify(trace, null, 4);
    await writeTraceFileReplacingAddressesWithSymbolicNames(_traceFileName, traceResult);

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

    console.log("Verifying " + _fileName);

    const _expectedResultFileName = GETH_TRACES_DIR + _fileName;
    const _actualResultFileName = SKALE_TRACES_DIR + _fileName;

    let expectedResult = await readJSONFile(_expectedResultFileName)
    let actualResult = await readJSONFile(_actualResultFileName)

    await verifyGasCalculations(actualResult);

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
                if (op == "SLOAD" || op == "SSTORE" || op == "EXTCODESIZE" || op == "CALL" ||
                    op == "STATICCALL" || op == "CREATE") {
                    return;
                }
            }

            if (difference.kind == "E" && difference.path!.length == 4 && difference.path![2] == "storage") {
                return;
            }

            if (difference.kind == "E" && difference.path!.length == 4 && difference.path![2] == "stack") {
                return;
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

async function verifyTransferTraceAgainstGethTrace(_fileName: string) {

    console.log("Verifying " + _fileName);

    const _expectedResultFileName = GETH_TRACES_DIR + _fileName;
    const _actualResultFileName = SKALE_TRACES_DIR + _fileName;

    let expectedResult = await readJSONFile(_expectedResultFileName)
    let actualResult = await readJSONFile(_actualResultFileName)

    const differences = deepDiff(expectedResult, actualResult)!;

    let foundDiffs = false;


    if (differences) {
        differences.forEach((difference, index) => {
            foundDiffs = true;
        });
    }
    ;

    await expect(foundDiffs).to.be.eq(false)
}

async function verifyPrestateTransferTraceAgainstGethTrace(_fileName: string) {

    console.log("Verifying " + _fileName);

    const _expectedResultFileName = GETH_TRACES_DIR + _fileName;
    const _actualResultFileName = SKALE_TRACES_DIR + _fileName;

    let expectedResult = await readJSONFile(_expectedResultFileName)
    let actualResult = await readJSONFile(_actualResultFileName)

    const differences = deepDiff(expectedResult, actualResult)!;

    let foundDiffs = false;


    if (differences) {
        differences.forEach((difference, index) => {

            if (difference.kind == "E" && difference.path!.length == 2) {
                if (difference.path![1] == "balance" || difference.path![1] == "nonce") {
                    return;
                }
            }

            if (difference.kind == "E" && difference.path!.length == 2) {
                let address = difference.path![0];
                if (address == ZERO_ADDRESS && difference.path![1] == "balance") {
                    return;
                }
            }

            console.log(`Found difference (lhs is expected value) ${index + 1} at path:`, difference.path);
            console.log(`Difference ${index + 1}:`, difference);

            foundDiffs = true;
        });
    }
    ;

    await expect(foundDiffs).to.be.eq(false)
}

async function verifyPrestateDiffTransferTraceAgainstGethTrace(_fileName: string) {

    console.log("Verifying " + _fileName);

    const _expectedResultFileName = GETH_TRACES_DIR + _fileName;
    const _actualResultFileName = SKALE_TRACES_DIR + _fileName;

    let expectedResult = await readJSONFile(_expectedResultFileName)
    let actualResult = await readJSONFile(_actualResultFileName)

    const differences = deepDiff(expectedResult, actualResult)!;

    let foundDiffs = false;


    if (differences) {
        differences.forEach((difference, index) => {

            if (difference.kind == "E" && difference.path!.length == 3) {
                if (difference.path![2] == "balance" || difference.path![2] == "nonce") {
                    return;
                }
            }

            if (difference.kind == "E" && difference.path!.length == 3) {
                let address = difference.path![1];
                if (address == ZERO_ADDRESS && difference.path![2] == "balance") {
                    return;
                }
            }

            console.log(`Found difference (lhs is expected value) ${index + 1} at path:`, difference.path);
            console.log(`Difference ${index + 1}:`, difference);

            foundDiffs = true;
        });
    }
    ;

    await expect(foundDiffs).to.be.eq(false)
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

async function verifyFourByteTraceAgainstGethTrace(_fileName: string) {

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

            foundDiffs = true;

            console.log(`Found difference (lhs is expected value) ${index + 1} at path:`, difference.path);
            console.log(`Difference ${index + 1}:`, difference);
        });
    }


    await expect(foundDiffs).to.be.eq(false)
}


async function verifyPrestateTraceAgainstGethTrace(_fileName: string) {

    console.log("Verifying " + _fileName);

    const _expectedResultFileName = GETH_TRACES_DIR + _fileName;
    const _actualResultFileName = SKALE_TRACES_DIR + _fileName;

    let expectedResult = await readJSONFile(_expectedResultFileName)
    let actualResult = await readJSONFile(_actualResultFileName)

    const differences = deepDiff(expectedResult, actualResult)!;

    let foundDiffs = false;


    if (differences) {
        differences.forEach((difference, index) => {
            if (difference.kind == "E" && difference.path!.length == 2) {
                let address = difference.path![0];
                if (address == ZERO_ADDRESS && difference.path![1] == "balance") {
                    return;
                }

                if (address == "OWNER.address" && difference.path![1] == "balance") {
                    return;
                }

                if (address == "OWNER.address" && difference.path![1] == "nonce") {
                    return;
                }
            }

            if (difference.kind != "E" && difference.path!.length == 1) {
                // ignore everything related to newly deployed contract
                const key = difference.path![0];
                if (key != "Tracer.address" && key != ZERO_ADDRESS && key != "OWNER.address") {
                    return;
                }
            }


            foundDiffs = true;

            console.log(`Found difference (lhs is expected value) ${index + 1} at path:`, difference.path);
            console.log(`Difference ${index + 1}:`, difference);
        });
    }


    await expect(foundDiffs).to.be.eq(false)
}

async function verifyPrestateDiffTraceAgainstGethTrace(_fileName: string) {

    console.log("Verifying " + _fileName);

    const _expectedResultFileName = GETH_TRACES_DIR + _fileName;
    const _actualResultFileName = SKALE_TRACES_DIR + _fileName;

    let expectedResult = await readJSONFile(_expectedResultFileName)
    let actualResult = await readJSONFile(_actualResultFileName)

    const differences = deepDiff(expectedResult, actualResult)!;

    let foundDiffs = false;


    if (differences) {
        differences.forEach((difference, index) => {

            if (difference.kind == "E" && difference.path!.length == 2) {
                let address = difference.path![0];
                if (address == ZERO_ADDRESS && difference.path![1] == "balance") {
                    return;
                }

                if (address == "OWNER.address" && difference.path![1] == "balance") {
                    return;
                }

                if (address == "OWNER.address" && difference.path![1] == "nonce") {
                    return;
                }
            }

            if (difference.kind == "E" && difference.path!.length == 3) {
                let address = difference.path![1];
                if (address == ZERO_ADDRESS && difference.path![2] == "balance") {
                    return;
                }

                if (address == "OWNER.address" && difference.path![2] == "balance") {
                    return;
                }

                if (address == "OWNER.address" && difference.path![2] == "nonce") {
                    return;
                }
            }

            if (difference.kind == "E" && difference.path!.length == 4) {
                if (difference.path![1] == "Tracer.address" && difference.path![2] == "storage") {
                    return;
                }
            }

            if (difference.kind != "E" && difference.path!.length == 2 &&
                difference.path![0] == "post") {
                // ignore everything related to newly deployed contract
                const key = difference.path![1];
                if (key != "Tracer.address" && key != ZERO_ADDRESS && key != "OWNER.address") {
                    return;
                }
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
    let currentOpGasCost = structLogs[0].gasCost
    let totalGasUsedInOps: bigint = currentOpGasCost;


    for (let index = 1; index < structLogs.length; index++) {
        const currentOpLog = structLogs[index]
        const previousOpLog = structLogs[index - 1]

        let newGasRemaining: bigint = currentOpLog.gas
        // geth does not correctly report gas cost of create operation
        if (previousOpLog.op !== "CREATE" && previousOpLog.op !== "RETURN"
            && previousOpLog.op !== "CALL" && previousOpLog.op !== "REVERT" && previousOpLog.op !== "STATICCALL") {
            expect(gasRemaining - newGasRemaining).eq(currentOpGasCost,
                // in case of failure we print current and previous op log
                "\n" + JSON.stringify(structLogs[index - 1]) +
                "\n" + JSON.stringify(currentOpLog) + "\n");
        }
        gasRemaining = newGasRemaining;
        currentOpGasCost = structLogs[index].gasCost
        totalGasUsedInOps += currentOpGasCost;
    }
}


async function main(): Promise<void> {

    expect(existsSync(GETH_TRACES_DIR + TEST_DEPLOY_DEFAULTTRACER_FILE_NAME));
    expect(existsSync(GETH_TRACES_DIR + TEST_CONTRACT_EXECUTE_DEFAULTTRACER_FILE_NAME));
    expect(existsSync(GETH_TRACES_DIR + TEST_CONTRACT_CALL_DEFAULTTRACER_FILE_NAME));

    await deleteAndRecreateDirectory(SKALE_TRACES_DIR);

    let deployedContract = await deployTestContract();
    const deployHash = deployedContract.deployTransaction.hash;
    DEPLOYED_CONTRACT_ADDRESS_LOWER_CASE = deployedContract.address.toString().toLowerCase();


    const firstMintHash: string = await executeTransferAndThenTestContractMintInSingleBlock(deployedContract);

    await getAndPrintCommittedTransactionTrace(deployHash, DEFAULT_TRACER, TEST_DEPLOY_DEFAULTTRACER_FILE_NAME);
    await getAndPrintCommittedTransactionTrace(deployHash, CALL_TRACER, TEST_DEPLOY_CALLTRACER_FILE_NAME);
    await getAndPrintCommittedTransactionTrace(deployHash, FOURBYTE_TRACER, TEST_DEPLOY_FOURBYTETRACER_FILE_NAME);
    await getAndPrintCommittedTransactionTrace(deployHash, PRESTATEDIFF_TRACER, TEST_DEPLOY_PRESTATEDIFFTRACER_FILE_NAME);
    await getAndPrintCommittedTransactionTrace(deployHash, PRESTATE_TRACER, TEST_DEPLOY_PRESTATETRACER_FILE_NAME);


    await getAndPrintCommittedTransactionTrace(firstMintHash, DEFAULT_TRACER, TEST_CONTRACT_EXECUTE_DEFAULTTRACER_FILE_NAME);
    await getAndPrintCommittedTransactionTrace(firstMintHash, CALL_TRACER, TEST_CONTRACT_EXECUTE_CALLTRACER_FILE_NAME);
    await getAndPrintCommittedTransactionTrace(firstMintHash, PRESTATE_TRACER, TEST_CONTRACT_EXECUTE_PRESTATETRACER_FILE_NAME);
    await getAndPrintCommittedTransactionTrace(firstMintHash, PRESTATEDIFF_TRACER, TEST_CONTRACT_EXECUTE_PRESTATEDIFFTRACER_FILE_NAME);
    await getAndPrintCommittedTransactionTrace(firstMintHash, FOURBYTE_TRACER, TEST_CONTRACT_EXECUTE_FOURBYTETRACER_FILE_NAME);


    const secondTransferHash: string = await sendTransferWithConfirmation();

    await getAndPrintCommittedTransactionTrace(secondTransferHash, DEFAULT_TRACER, TEST_TRANSFER_DEFAULTTRACER_FILE_NAME);
    await getAndPrintCommittedTransactionTrace(secondTransferHash, CALL_TRACER, TEST_TRANSFER_CALLTRACER_FILE_NAME);
    await getAndPrintCommittedTransactionTrace(secondTransferHash, PRESTATE_TRACER, TEST_TRANSFER_PRESTATETRACER_FILE_NAME);
    await getAndPrintCommittedTransactionTrace(secondTransferHash, PRESTATEDIFF_TRACER, TEST_TRANSFER_PRESTATEDIFFTRACER_FILE_NAME);
    await getAndPrintCommittedTransactionTrace(secondTransferHash, FOURBYTE_TRACER, TEST_TRANSFER_FOURBYTETRACER_FILE_NAME);


    const secondMintHash: string = await executeMint2(deployedContract);
    await getAndPrintCommittedTransactionTrace(secondMintHash, DEFAULT_TRACER, TEST_CONTRACT_EXECUTE2_DEFAULTTRACER_FILE_NAME);
    await getAndPrintCommittedTransactionTrace(secondMintHash, CALL_TRACER, TEST_CONTRACT_EXECUTE2_CALLTRACER_FILE_NAME);
    await getAndPrintCommittedTransactionTrace(secondMintHash, PRESTATE_TRACER, TEST_CONTRACT_EXECUTE2_PRESTATETRACER_FILE_NAME);
    await getAndPrintCommittedTransactionTrace(secondMintHash, PRESTATEDIFF_TRACER, TEST_CONTRACT_EXECUTE2_PRESTATEDIFFTRACER_FILE_NAME);
    await getAndPrintCommittedTransactionTrace(secondMintHash, FOURBYTE_TRACER, TEST_CONTRACT_EXECUTE2_FOURBYTETRACER_FILE_NAME);


    const revertHash: string = await executeRevert(deployedContract);
    await getAndPrintCommittedTransactionTrace(revertHash, DEFAULT_TRACER, TEST_CONTRACT_EXECUTE3_DEFAULTTRACER_FILE_NAME);
    await getAndPrintCommittedTransactionTrace(revertHash, CALL_TRACER, TEST_CONTRACT_EXECUTE3_CALLTRACER_FILE_NAME);
    await getAndPrintCommittedTransactionTrace(revertHash, PRESTATE_TRACER, TEST_CONTRACT_EXECUTE3_PRESTATETRACER_FILE_NAME);
    await getAndPrintCommittedTransactionTrace(revertHash, PRESTATEDIFF_TRACER, TEST_CONTRACT_EXECUTE3_PRESTATEDIFFTRACER_FILE_NAME);
    await getAndPrintCommittedTransactionTrace(revertHash, FOURBYTE_TRACER, TEST_CONTRACT_EXECUTE3_FOURBYTETRACER_FILE_NAME);


    await callDebugTraceCall(deployedContract, DEFAULT_TRACER, TEST_CONTRACT_CALL_DEFAULTTRACER_FILE_NAME);
    await callDebugTraceCall(deployedContract, CALL_TRACER, TEST_CONTRACT_CALL_CALLTRACER_FILE_NAME);
    await callDebugTraceCall(deployedContract, FOURBYTE_TRACER, TEST_CONTRACT_CALL_FOURBYTETRACER_FILE_NAME);
    await callDebugTraceCall(deployedContract, PRESTATE_TRACER, TEST_CONTRACT_CALL_PRESTATETRACER_FILE_NAME);
    await callDebugTraceCall(deployedContract, PRESTATEDIFF_TRACER, TEST_CONTRACT_CALL_PRESTATEDIFFTRACER_FILE_NAME);


    // geth does not have replay trace
    if (hre.network.name != "geth") {
        await callDebugTraceCall(deployedContract, REPLAY_TRACER, TEST_CONTRACT_CALL_REPLAYTRACER_FILE_NAME);
    }


    await verifyTransferTraceAgainstGethTrace(TEST_TRANSFER_DEFAULTTRACER_FILE_NAME);
    await verifyTransferTraceAgainstGethTrace(TEST_TRANSFER_CALLTRACER_FILE_NAME);
    await verifyPrestateDiffTransferTraceAgainstGethTrace(TEST_TRANSFER_PRESTATEDIFFTRACER_FILE_NAME);
    await verifyPrestateTransferTraceAgainstGethTrace(TEST_TRANSFER_PRESTATETRACER_FILE_NAME);
    await verifyTransferTraceAgainstGethTrace(TEST_TRANSFER_FOURBYTETRACER_FILE_NAME);


    await verifyDefaultTraceAgainstGethTrace(TEST_CONTRACT_EXECUTE_DEFAULTTRACER_FILE_NAME);
    await verifyCallTraceAgainstGethTrace(TEST_CONTRACT_EXECUTE_CALLTRACER_FILE_NAME);
    await verifyPrestateTraceAgainstGethTrace(TEST_CONTRACT_EXECUTE_PRESTATETRACER_FILE_NAME);
    await verifyPrestateDiffTraceAgainstGethTrace(TEST_CONTRACT_EXECUTE_PRESTATEDIFFTRACER_FILE_NAME);
    await verifyFourByteTraceAgainstGethTrace(TEST_CONTRACT_EXECUTE_FOURBYTETRACER_FILE_NAME);


    await verifyDefaultTraceAgainstGethTrace(TEST_CONTRACT_CALL_DEFAULTTRACER_FILE_NAME);
    await verifyCallTraceAgainstGethTrace(TEST_CONTRACT_CALL_CALLTRACER_FILE_NAME);
    await verifyFourByteTraceAgainstGethTrace(TEST_CONTRACT_CALL_FOURBYTETRACER_FILE_NAME);
    await verifyPrestateTraceAgainstGethTrace(TEST_CONTRACT_CALL_PRESTATETRACER_FILE_NAME);
    await verifyPrestateDiffTraceAgainstGethTrace(TEST_CONTRACT_CALL_PRESTATEDIFFTRACER_FILE_NAME);

    await verifyDefaultTraceAgainstGethTrace(TEST_DEPLOY_DEFAULTTRACER_FILE_NAME);
    await verifyCallTraceAgainstGethTrace(TEST_DEPLOY_CALLTRACER_FILE_NAME);
    await verifyFourByteTraceAgainstGethTrace(TEST_DEPLOY_FOURBYTETRACER_FILE_NAME);
    await verifyPrestateTraceAgainstGethTrace(TEST_DEPLOY_PRESTATETRACER_FILE_NAME);
    await verifyPrestateDiffTraceAgainstGethTrace(TEST_DEPLOY_PRESTATEDIFFTRACER_FILE_NAME);


    await verifyCallTraceAgainstGethTrace(TEST_CONTRACT_EXECUTE2_CALLTRACER_FILE_NAME);
    await verifyFourByteTraceAgainstGethTrace(TEST_CONTRACT_EXECUTE2_FOURBYTETRACER_FILE_NAME);
    await verifyDefaultTraceAgainstGethTrace(TEST_CONTRACT_EXECUTE2_DEFAULTTRACER_FILE_NAME);
    await verifyPrestateTraceAgainstGethTrace(TEST_CONTRACT_EXECUTE2_PRESTATETRACER_FILE_NAME);
    await verifyPrestateDiffTraceAgainstGethTrace(TEST_CONTRACT_EXECUTE2_PRESTATEDIFFTRACER_FILE_NAME);

    await verifyCallTraceAgainstGethTrace(TEST_CONTRACT_EXECUTE3_CALLTRACER_FILE_NAME);
    await verifyFourByteTraceAgainstGethTrace(TEST_CONTRACT_EXECUTE3_FOURBYTETRACER_FILE_NAME);
    await verifyDefaultTraceAgainstGethTrace(TEST_CONTRACT_EXECUTE3_DEFAULTTRACER_FILE_NAME);
    await verifyPrestateTraceAgainstGethTrace(TEST_CONTRACT_EXECUTE3_PRESTATETRACER_FILE_NAME);
    await verifyPrestateDiffTraceAgainstGethTrace(TEST_CONTRACT_EXECUTE3_PRESTATEDIFFTRACER_FILE_NAME);


}


// We recommend this pattern to be able to use async/await everywhere
// and properly handle errors.
main().catch((error: any) => {
    console.error(error);
    process.exitCode = 1;
});
