const fs = require("fs");

const names = [
  "ACTIONS_RUNTIME_TOKEN",
  "ACTIONS_CACHE_URL",
  "ACTIONS_RESULTS_URL",
  "ACTIONS_CACHE_SERVICE_V2",
];

const token = process.env.ACTIONS_RUNTIME_TOKEN || "";
const serviceV2 = process.env.ACTIONS_CACHE_SERVICE_V2 || "";
const url = serviceV2
  ? process.env.ACTIONS_RESULTS_URL || ""
  : process.env.ACTIONS_CACHE_URL || process.env.ACTIONS_RESULTS_URL || "";

if (!token || !url) {
  process.stderr.write(
    "GitHub Actions cache runtime credentials are not available to this JavaScript action\n"
  );
  process.exit(1);
}

const envFile = process.env.GITHUB_ENV;
if (!envFile) {
  process.stderr.write("GITHUB_ENV is unset\n");
  process.exit(1);
}

process.stdout.write(`::add-mask::${token}\n`);

for (const name of names) {
  const value = process.env[name];
  if (!value) {
    continue;
  }
  const marker = `ORLIX_${name}_${Date.now().toString(16)}`;
  fs.appendFileSync(envFile, `${name}<<${marker}\n${value}\n${marker}\n`);
}
