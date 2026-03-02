import { BITE } from "@skalenetwork/bite";
import { existsSync, readFileSync } from "node:fs";
import { resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { ethers } from "ethers";

type BiteCompatConfig = {
  httpPort: number;
};

function parseBiteCompatConfig(configPath: string): BiteCompatConfig {
  const text = readFileSync(configPath, "utf8");
  let section = "";
  const values: Record<string, string> = {};

  for (const rawLine of text.split("\n")) {
    const noComment = rawLine.split("#", 1)[0].trim();
    if (!noComment) {
      continue;
    }
    const sec = noComment.match(/^\[([^\]]+)\]$/);
    if (sec) {
      section = sec[1];
      continue;
    }
    if (section !== "bite_compat") {
      continue;
    }
    const kv = noComment.match(/^([A-Za-z0-9_]+)\s*=\s*(.+)$/);
    if (!kv) {
      continue;
    }
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
  if (!Number.isFinite(httpPort) || httpPort <= 0) {
    throw new Error(`Invalid bite_compat.http_port: ${values.http_port}`);
  }
  return { httpPort };
}

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

  if (!existsSync(artifactPath)) {
    throw new Error(
      `SimpleSecret artifact not found after compile: ${artifactPath}`
    );
  }
}

async function main() {
  const tomlPath =
    process.env.BITE_COMPAT_TOML || resolve(__dirname, "..", "bite-compat.toml");
  const cfg = parseBiteCompatConfig(tomlPath);
  const providerUrl =
    process.env.BITE_PROVIDER_URL || `http://127.0.0.1:${cfg.httpPort}`;
  const outputJsonOnly = process.env.BITE_OUTPUT_JSON === "1";

  const privateKey = process.env.BITE_PRIVATE_KEY || process.env.PRIVATE_KEY;
  if (!privateKey) {
    throw new Error("BITE_PRIVATE_KEY (or PRIVATE_KEY) is required");
  }

  const provider = new ethers.JsonRpcProvider(providerUrl);
  const signer = new ethers.Wallet(privateKey, provider);
  const bite = new BITE(providerUrl);

  const artifactPath =
    process.env.BITE_SIMPLE_SECRET_ARTIFACT ||
    resolve(
      __dirname,
      "..",
      "sol/artifacts/contracts/SimpleSecret.sol/SimpleSecret.json"
    );
  ensureSimpleSecretArtifact(artifactPath);
  const artifact = JSON.parse(readFileSync(artifactPath, "utf8"));
  const factory = new ethers.ContractFactory(
    artifact.abi,
    artifact.bytecode,
    signer
  );
  const contract = await factory.deploy();
  await contract.waitForDeployment();
  const simpleSecretAddress = await contract.getAddress();
  const contractInstance = new ethers.Contract(
    simpleSecretAddress,
    artifact.abi,
    signer
  );
  const CTX_GAS_PAYMENT = BigInt(60000000000000000);
  const committeesInfo = await bite.getCommitteesInfo();

  console.log(`Contract: ${simpleSecretAddress}`);
  console.log(`Committees: ${JSON.stringify(committeesInfo)}`);
  const secret = "0x" + Buffer.from("Hello BITE!").toString("hex");
  console.log(`Encrypting: "${secret}"`);
  const aadTE = simpleSecretAddress;
  console.log(`aadTE: ${aadTE}`);
  const encrypted = await bite.encryptMessageForCTX(secret, aadTE);
  console.log(`Encrypted: ${encrypted}`);

  console.log("Submitting encrypted secret...");
  const tx = await contractInstance.revealSecret(encrypted, {
    gasLimit: 500000,
    value: CTX_GAS_PAYMENT,
  });

  const out = {
    txHash: tx.hash,
    simpleSecretAddress,
  };

  if (outputJsonOnly) {
    console.log(JSON.stringify(out));
    return;
  }

  console.log("Provider URL:", providerUrl);
  console.log("SimpleSecret:", simpleSecretAddress);
  console.log("Submitted revealSecret tx:", tx.hash);
}

main().catch((error) => {
  console.error("make_transaction_bite2.ts failed:", error);
  process.exitCode = 1;
});
