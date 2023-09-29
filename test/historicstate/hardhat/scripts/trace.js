// We require the Hardhat Runtime Environment explicitly here. This is optional
// but useful for running the script in a standalone fashion through `node <script>`.
//
// You can also run a script with `npx hardhat run <script>`. If you do that, Hardhat
// will compile your contracts, add the Hardhat Runtime Environment's members to the
// global scope, and execute the script.


OWNER_ADDRESS = "0x907cd0881E50d359bb9Fd120B1A5A143b1C97De6";
ZERO_ADDRESS =  "0xO000000000000000000000000000000000000000";
INITIAL_MINT = 10000000000000000000000000000000000000000;

require("hardhat");

let lockContract;


function CHECK(result) {
    if (!result) {
        message = `Check failed ${result}`
        console.log(message);
        throw message;
    }
}

function getTrace(hash) {
    const trace = hre.ethers.provider.send('debug_traceTransaction', [hash, {}]);
    console.log(trace);
}

async function deployWriteAndDestroy() {

    console.log(`Deploying ...`);

    const Lock = await hre.ethers.getContractFactory("Lock");

    const lock = await Lock.deploy();

    lockContract = await lock.deployed();


    // Print the transaction hash
    console.log(lockContract.hash)

    deployBn = await hre.ethers.provider.getBlockNumber();

    console.log(`Contract deployed to ${lockContract.address} at block ${deployBn}`);

    try {
        const trace = getTrace()
        console.log(trace)
    } catch(e) {
        console.error(e)
    }

    console.log(`Now minting`);

    transferReceipt = await lockContract.mint(1000);
    await transferReceipt.wait();

    console.log(`Now testing self-destruct`);

    transferReceipt2 = await lockContract.die("0x690b9a9e9aa1c9db991c7721a92d351db4fac990");
    await transferReceipt2.wait();

    console.log(`Successfully self destructed`);

    console.log(`PASSED!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!`);


}

async function main() {
    await deployWriteAndDestroy();
}


// We recommend this pattern to be able to use async/await everywhere
// and properly handle errors.
main().catch((error) => {
    console.error(error);
    process.exitCode = 1;
});
