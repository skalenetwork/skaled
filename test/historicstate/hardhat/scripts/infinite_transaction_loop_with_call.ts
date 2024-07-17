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





var DEPLOYED_CONTRACT_ADDRESS_LOWER_CASE: string = "";
var globalCallCount = 0;

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


    const Tracer = await ethers.getContractFactory("Tracer");
    const tracer = await Tracer.attach(deployedTestContract.address);
    let result = await tracer.mint2(1000);

    console.log("Got result");

    console.log(result);

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


    return mintReceipt.hash!;

}

async function executeMintCall(deployedContract: any): Promise<string> {

    const result = await deployedContract.mint2(1000);

    console.log("Executed mint2 call");

}


async function executeRevert(deployedContract: any): Promise<string> {

    const revertReceipt = await deployedContract[EXECUTE3_FUNCTION_NAME](1000, {
        gasLimit: 2100000, // this is just an example value; you'll need to set an appropriate gas limit for your specific function call,
    });

    expect(revertReceipt.blockNumber).not.to.be.null;


    return revertReceipt.hash!;

}

async function main(): Promise<void> {

    let deployedContract = await deployTestContract();





    DEPLOYED_CONTRACT_ADDRESS_LOWER_CASE = deployedContract.address.toString().toLowerCase();



    while (true) {

        const firstMintHash: string = await executeTransferAndThenTestContractMintInSingleBlock(deployedContract);

        const secondTransferHash: string = await sendTransferWithConfirmation();


        const secondMintHash: string = await executeMintCall(deployedContract);


        const revertHash: string = await executeRevert(deployedContract);
    }

}


// We recommend this pattern to be able to use async/await everywhere
// and properly handle errors.
main().catch((error: any) => {
    console.error(error);
    process.exitCode = 1;
});
