import { BITE } from "@skalenetwork/bite";
import { existsSync, readFileSync } from "node:fs";
import { resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { ethers } from "ethers";

// ---------------------------------------------------------------------------
// Mode — selected via CLI flag; defaults to --all
// ---------------------------------------------------------------------------

type Mode = "deploy" | "simulate" | "transaction" | "all";

function parseMode(): Mode {
  const args = process.argv.slice(2);
  if (args.includes("--deploy"))      return "deploy";
  if (args.includes("--simulate"))    return "simulate";
  if (args.includes("--transaction")) return "transaction";
  return "all";
}

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

type BiteCompatConfig = {
  httpPort: number;
};

function parseBiteCompatConfig(configPath: string): BiteCompatConfig {
  const text = readFileSync(configPath, "utf8");
  let section = "";
  const values: Record<string, string> = {};

  for (const rawLine of text.split("\n")) {
    const noComment = rawLine.split("#", 1)[0].trim();
    if (!noComment) continue;
    const sec = noComment.match(/^\[([^\]]+)\]$/);
    if (sec) { section = sec[1]; continue; }
    if (section !== "bite_compat") continue;
    const kv = noComment.match(/^([A-Za-z0-9_]+)\s*=\s*(.+)$/);
    if (!kv) continue;
    let value = kv[2].trim();
    if (
      (value.startsWith('"') && value.endsWith('"')) ||
      (value.startsWith("'") && value.endsWith("'"))
    ) {
      value = value.slice(1, -1);
    }
    values[kv[1]] = value;
  }

  const httpPort = Number(values.http_port ?? "4234");
  if (!Number.isFinite(httpPort) || httpPort <= 0)
    throw new Error(`Invalid bite_compat.http_port: ${values.http_port}`);
  return { httpPort };
}

// ---------------------------------------------------------------------------
// Artifact helpers
// ---------------------------------------------------------------------------

function ensureSimpleSecretArtifact(artifactPath: string): void {
  const solDir = resolve(__dirname, "..", "sol");
  const compileCmd =
    process.env.BITE_SOL_COMPILE_COMMAND || "bun run hardhat compile";

  console.log(`Compiling Solidity contracts: ${compileCmd}`);
  const cp = spawnSync(compileCmd, {
    cwd: solDir,
    shell: true,
    encoding: "utf8",
  });

  if (cp.status !== 0) {
    const stdout = (cp.stdout || "").trim();
    const stderr = (cp.stderr || "").trim();
    throw new Error(
      `Solidity compilation failed (rc=${cp.status}) for ${compileCmd}. ` +
        `stdout=${stdout.slice(-500)} stderr=${stderr.slice(-500)}`
    );
  }

  if (!existsSync(artifactPath))
    throw new Error(`SimpleSecret artifact not found after compile: ${artifactPath}`);
}

// ---------------------------------------------------------------------------
// Steps
// ---------------------------------------------------------------------------

async function deploy(
  signer: ethers.Wallet,
  artifactPath: string
): Promise<{ contractAddress: string; contractInstance: ethers.Contract }> {
  const artifact = JSON.parse(readFileSync(artifactPath, "utf8"));
  const factory = new ethers.ContractFactory(artifact.abi, artifact.bytecode, signer);
  const contract = await factory.deploy();
  await contract.waitForDeployment();
  const contractAddress = await contract.getAddress();
  const contractInstance = new ethers.Contract(contractAddress, artifact.abi, signer);
  console.log(`Deployed SimpleSecret at ${contractAddress}`);
  return { contractAddress, contractInstance };
}

type SimulateResult = {
  callSucceeded: boolean;
  gasEstimate: string | null;
  revertReason: string | null;
};

async function simulate(
  contractInstance: ethers.Contract,
  encrypted: string,
  value: bigint
): Promise<SimulateResult> {
  try {
    const gasEstimate = await contractInstance.revealSecret.estimateGas(encrypted, { value });
    console.log(`estimateGas succeeded: ${gasEstimate}`);
    return { callSucceeded: true, gasEstimate: gasEstimate.toString(), revertReason: null };
  } catch (e: any) {
    const reason = e?.shortMessage ?? e?.message ?? String(e);
    console.log(`estimateGas reverted: ${reason}`);
    return { callSucceeded: false, gasEstimate: null, revertReason: reason };
  }
}

type TransactionResult = {
  txHash: string;
  gasLimit: string;
};

async function sendTransaction(
  contractInstance: ethers.Contract,
  encrypted: string,
  value: bigint,
  gasEstimate: bigint | null
): Promise<TransactionResult> {
  const gasLimit = gasEstimate ?? BigInt(500_000);
  const tx = await contractInstance.revealSecret(encrypted, { gasLimit, value });
  console.log(`Submitted revealSecret tx: ${tx.hash}`);
  return { txHash: tx.hash, gasLimit: gasLimit.toString() };
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

async function main() {
  const mode = parseMode();

  const tomlPath =
    process.env.BITE_COMPAT_TOML || resolve(__dirname, "..", "bite-compat.toml");
  const cfg = parseBiteCompatConfig(tomlPath);
  const providerUrl =
    process.env.BITE_PROVIDER_URL || `http://127.0.0.1:${cfg.httpPort}`;
  const outputJsonOnly = process.env.BITE_OUTPUT_JSON === "1";

  const privateKey = process.env.BITE_PRIVATE_KEY || process.env.PRIVATE_KEY;
  if (!privateKey) throw new Error("BITE_PRIVATE_KEY (or PRIVATE_KEY) is required");

  const provider = new ethers.JsonRpcProvider(providerUrl);
  const signer = new ethers.Wallet(privateKey, provider);
  const bite = new BITE(providerUrl);

  const artifactPath =
    process.env.BITE_SIMPLE_SECRET_ARTIFACT ||
    resolve(__dirname, "..", "sol/artifacts/contracts/SimpleSecret.sol/SimpleSecret.json");
  ensureSimpleSecretArtifact(artifactPath);

  const CTX_GAS_PAYMENT = BigInt(60_000_000_000_000_000);

  // -- deploy ----------------------------------------------------------------
  let contractAddress = process.env.BITE_CONTRACT_ADDRESS ?? "";
  let contractInstance: ethers.Contract;

  if (mode === "deploy" || mode === "all") {
    const result = await deploy(signer, artifactPath);
    contractAddress = result.contractAddress;
    contractInstance = result.contractInstance;

    if (mode === "deploy") {
      const out = { contractAddress };
      if (outputJsonOnly) { console.log(JSON.stringify(out)); return; }
      console.log("SimpleSecret:", contractAddress);
      return;
    }
  } else {
    if (!contractAddress)
      throw new Error("BITE_CONTRACT_ADDRESS must be set for --simulate / --transaction");
    const artifact = JSON.parse(readFileSync(artifactPath, "utf8"));
    contractInstance = new ethers.Contract(contractAddress, artifact.abi, signer);
  }

  // Encrypt once — shared by both simulate and transaction steps
  const secret = "0x" + Buffer.from("Hello BITE!").toString("hex");
  console.log(`Encrypting: "${secret}" (aad=${contractAddress})`);
  const encrypted = await bite.encryptMessageForCTX(secret, contractAddress);
  console.log(`Encrypted: ${encrypted}`);

  // -- simulate --------------------------------------------------------------
  let gasEstimate: bigint | null = null;

  if (mode === "simulate" || mode === "all") {
    const simResult = await simulate(contractInstance, encrypted, CTX_GAS_PAYMENT);
    if (simResult.gasEstimate !== null)
      gasEstimate = BigInt(simResult.gasEstimate);

    if (mode === "simulate") {
      if (outputJsonOnly) { console.log(JSON.stringify(simResult)); return; }
      console.log("Simulate result:", simResult);
      return;
    }
  }

  // -- transaction -----------------------------------------------------------
  if (mode === "transaction" || mode === "all") {
    const txResult = await sendTransaction(
      contractInstance, encrypted, CTX_GAS_PAYMENT, gasEstimate
    );
    const out = { ...txResult, contractAddress };
    if (outputJsonOnly) { console.log(JSON.stringify(out)); return; }
    console.log("Transaction result:", out);
  }
}

main().catch((error) => {
  console.error("make_transaction_bite2.ts failed:", error);
  process.exitCode = 1;
});
