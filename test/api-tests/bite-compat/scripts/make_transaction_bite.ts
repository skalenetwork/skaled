import { BITE } from "@skalenetwork/bite";
import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { randomBytes } from "node:crypto";

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

const tomlPath =
  process.env.BITE_COMPAT_TOML || resolve(__dirname, "..", "bite-compat.toml");
const cfg = parseBiteCompatConfig(tomlPath);
const providerUrl = `http://127.0.0.1:${cfg.httpPort}`;
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
