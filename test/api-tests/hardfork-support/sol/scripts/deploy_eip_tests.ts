import { ethers } from "hardhat";
import * as fs from "fs";

async function main() {
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

  console.log("\n" + "=".repeat(50));

  const deploymentInfo = {
    network: (await ethers.provider.getNetwork()).name,
    chainId: Number((await ethers.provider.getNetwork()).chainId),
    deployer: deployer.address,
    contracts: {
      EIP2929Test: addr2929,
      EIP2930Test: addr2930,
      EIP2565Test: addr2565,
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
