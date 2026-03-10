import { ethers } from "hardhat";
import * as fs from "fs";

async function main() {
  console.log("Starting SimpleSecret deployment...");
  console.log("=".repeat(50));
  console.log("RPC endpoint:", process.env.ENDPOINT || "(not set)");
  console.log("PRIVATE_KEY set:", !!process.env.PRIVATE_KEY);

  const [deployer] = await ethers.getSigners();
  console.log("Deployer:", deployer.address);

  const balance = await ethers.provider.getBalance(deployer.address);
  console.log("Balance:", ethers.formatEther(balance), "ETH");

  if (balance === 0n) {
    console.error("ERROR: Deployer has zero balance. Check genesis config.");
    process.exit(1);
  }
  console.log("=".repeat(50));

  const factory = await ethers.getContractFactory("SimpleSecret");
  const simpleSecret = await factory.deploy();
  await simpleSecret.waitForDeployment();
  const contractAddress = await simpleSecret.getAddress();

  console.log("SimpleSecret deployed to:", contractAddress);

  const network = await ethers.provider.getNetwork();
  const deploymentInfo = {
    network: network.name,
    chainId: Number(network.chainId),
    deployer: deployer.address,
    contracts: {
      SimpleSecret: contractAddress,
    },
    deploymentTime: new Date().toISOString(),
    blockNumber: await ethers.provider.getBlockNumber(),
  };

  if (!fs.existsSync("./deployments")) {
    fs.mkdirSync("./deployments");
  }

  const outputPath = "./deployments/simple_secret.json";
  fs.writeFileSync(outputPath, JSON.stringify(deploymentInfo, null, 2));
  console.log("Deployment info saved to:", outputPath);
  console.log("Deployment completed successfully.");
}

main()
  .then(() => process.exit(0))
  .catch((error) => {
    console.error("Deployment failed:", error);
    process.exit(1);
  });
