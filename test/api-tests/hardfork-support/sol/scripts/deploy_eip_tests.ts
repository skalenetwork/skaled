import { ethers, run } from "hardhat";
import * as fs from "fs";

async function main() {
  console.log("Compiling contracts...");
  await run("compile");

  console.log("Starting EIP test contracts deployment...");
  console.log("=".repeat(50));
  console.log("RPC endpoint:", process.env.ENDPOINT || "(not set)");
  console.log("PRIVATE_KEY set:", !!process.env.PRIVATE_KEY);

  const [deployer] = await ethers.getSigners();
  console.log("Deployer:", deployer.address);

  const balance = await ethers.provider.getBalance(deployer.address);
  console.log("Balance:", ethers.formatEther(balance), "ETH");
  
  if (balance === 0n) {
    console.error("ERROR: Deployer has zero balance! Check genesis config.");
    process.exit(1);
  }
  console.log("=".repeat(50));

  // Deploy EIP2929Test
  console.log("\nDeploying EIP2929Test...");
  const EIP2929 = await ethers.getContractFactory("EIP2929Test");
  const eip2929 = await EIP2929.deploy();
  await eip2929.waitForDeployment();
  const addr2929 = await eip2929.getAddress();
  console.log("EIP2929Test deployed to:", addr2929);

  // Deploy EIP2930Test
  console.log("\nDeploying EIP2930Test...");
  const EIP2930 = await ethers.getContractFactory("EIP2930Test");
  const eip2930 = await EIP2930.deploy();
  await eip2930.waitForDeployment();
  const addr2930 = await eip2930.getAddress();
  console.log("EIP2930Test deployed to:", addr2930);

  // Deploy EIP2565Test
  console.log("\nDeploying EIP2565Test...");
  const EIP2565 = await ethers.getContractFactory("EIP2565Test");
  const eip2565 = await EIP2565.deploy();
  await eip2565.waitForDeployment();
  const addr2565 = await eip2565.getAddress();
  console.log("EIP2565Test deployed to:", addr2565);

  // Deploy EIP2929RevertTest
  console.log("\nDeploying EIP2929RevertTest...");
  const EIP2929Revert = await ethers.getContractFactory("EIP2929RevertTest");
  const eip2929Revert = await EIP2929Revert.deploy();
  await eip2929Revert.waitForDeployment();
  const addr2929Revert = await eip2929Revert.getAddress();
  console.log("EIP2929RevertTest deployed to:", addr2929Revert);

  // Deploy EIP2929ExtendedTest
  console.log("\nDeploying EIP2929ExtendedTest...");
  const EIP2929Extended = await ethers.getContractFactory("EIP2929ExtendedTest");
  const eip2929Extended = await EIP2929Extended.deploy();
  await eip2929Extended.waitForDeployment();
  const addr2929Extended = await eip2929Extended.getAddress();
  console.log("EIP2929ExtendedTest deployed to:", addr2929Extended);

  // Deploy EIP2565GasTest
  console.log("\nDeploying EIP2565GasTest...");
  const EIP2565Gas = await ethers.getContractFactory("EIP2565GasTest");
  const eip2565Gas = await EIP2565Gas.deploy();
  await eip2565Gas.waitForDeployment();
  const addr2565Gas = await eip2565Gas.getAddress();
  console.log("EIP2565GasTest deployed to:", addr2565Gas);

  // Deploy EIP3198Test
  console.log("\nDeploying EIP3198Test...");
  const EIP3198 = await ethers.getContractFactory("EIP3198Test");
  const eip3198 = await EIP3198.deploy();
  await eip3198.waitForDeployment();
  const addr3198 = await eip3198.getAddress();
  console.log("EIP3198Test deployed to:", addr3198);

  // Deploy EIP3529Test
  console.log("\nDeploying EIP3529Test...");
  const EIP3529 = await ethers.getContractFactory("EIP3529Test");
  const eip3529 = await EIP3529.deploy();
  await eip3529.waitForDeployment();
  const addr3529 = await eip3529.getAddress();
  console.log("EIP3529Test deployed to:", addr3529);

  // Deploy EIP3541Test
  console.log("\nDeploying EIP3541Test...");
  const EIP3541 = await ethers.getContractFactory("EIP3541Test");
  const eip3541 = await EIP3541.deploy();
  await eip3541.waitForDeployment();
  const addr3541 = await eip3541.getAddress();
  console.log("EIP3541Test deployed to:", addr3541);

  // Deploy EIP1559EffectiveGasPrice
  console.log("\nDeploying EIP1559EffectiveGasPrice...");
  const EIP1559EffectiveGasPrice = await ethers.getContractFactory("EIP1559EffectiveGasPrice");
  const eip1559EffectiveGasPrice = await EIP1559EffectiveGasPrice.deploy();
  await eip1559EffectiveGasPrice.waitForDeployment();
  const addr1559EffectiveGasPrice = await eip1559EffectiveGasPrice.getAddress();
  console.log("EIP1559EffectiveGasPrice deployed to:", addr1559EffectiveGasPrice);

  console.log("\n" + "=".repeat(50));

  const deploymentInfo = {
    network: (await ethers.provider.getNetwork()).name,
    chainId: Number((await ethers.provider.getNetwork()).chainId),
    deployer: deployer.address,
    contracts: {
      EIP2929Test: addr2929,
      EIP2930Test: addr2930,
      EIP2565Test: addr2565,
      EIP2929RevertTest: addr2929Revert,
      EIP2929ExtendedTest: addr2929Extended,
      EIP2565GasTest: addr2565Gas,
      EIP3198Test: addr3198,
      EIP3529Test: addr3529,
      EIP3541Test: addr3541,
      EIP1559EffectiveGasPrice: addr1559EffectiveGasPrice,
    },
    deploymentTime: new Date().toISOString(),
    blockNumber: await ethers.provider.getBlockNumber(),
  };

  if (!fs.existsSync("./deployments")) {
    fs.mkdirSync("./deployments");
  }

  const outputPath = "./deployments/eip_tests.json";
  fs.writeFileSync(outputPath, JSON.stringify(deploymentInfo, null, 2));
  console.log("Deployment info saved to:", outputPath);
  console.log("Deployment completed successfully!");
}

main()
  .then(() => process.exit(0))
  .catch((error) => {
    console.error("Deployment failed:", error);
    process.exit(1);
  });
