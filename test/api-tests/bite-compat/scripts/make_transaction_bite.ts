import { BITE } from "@skalenetwork/bite";
import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { randomBytes } from "node:crypto";
import { spawnSync } from "node:child_process";

type BiteCompatConfig = {
  httpPort: number;
  toAddress: string;
  valueWei: string;
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
  const toAddress = values.to_address ?? `0x${randomBytes(20).toString("hex")}`;
  const valueWei = values.value_wei ?? "0x1";

  if (!Number.isFinite(httpPort) || httpPort <= 0) {
    throw new Error(`Invalid bite_compat.http_port: ${values.http_port}`);
  }

  return { httpPort, toAddress, valueWei };
}

function compileSolidityContracts(): void {
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
}

const tomlPath =
  process.env.BITE_COMPAT_TOML || resolve(__dirname, "..", "bite-compat.toml");
const cfg = parseBiteCompatConfig(tomlPath);
const providerUrl = process.env.BITE_PROVIDER_URL || `http://127.0.0.1:${cfg.httpPort}`;
const to = process.env.BITE_TX_TO || cfg.toAddress;
const value = process.env.BITE_TX_VALUE || cfg.valueWei;
const data = process.env.BITE_TX_DATA || "0x";
const outputJsonOnly = process.env.BITE_OUTPUT_JSON === "1";

const transaction = {
  to,
  value,
  data,
};

(async () => {
  try {
    compileSolidityContracts();
    const bite = new BITE(providerUrl);
    const encryptedTx = await bite.encryptTransaction(transaction);
    const committeesInfo = await bite.getCommitteesInfo();

    if (outputJsonOnly) {
      console.log(JSON.stringify({
        providerUrl,
        transaction,
        encryptedTx,
        committeesInfo,
      }));
      return;
    }

    console.log("Provider URL:", providerUrl);
    console.log("Encrypted Transaction:", encryptedTx);
    console.log("Committees Info:", committeesInfo);
    console.log("Current BLS Public Key:", committeesInfo[0]?.commonBLSPublicKey);
    console.log("Current Epoch ID:", committeesInfo[0]?.epochId);
  } catch (error) {
    console.error("Encryption Error:", error);
    process.exitCode = 1;
  }
})();
