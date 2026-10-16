import fs from "node:fs";
import path from "node:path";
import crypto from "node:crypto";
import { fileURLToPath } from "node:url";
import { execFileSync } from "node:child_process";
import { rcedit } from "rcedit";

const ui = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const root = path.resolve(ui, "../../..");
const version = JSON.parse(
  fs.readFileSync(path.join(ui, "package.json")),
).version;
const output = path.join(root, "releases", `Sector-${version}-win-x64`);
if (fs.existsSync(output))
  throw new Error(
    `Output already exists: ${output}. Move it before rebuilding.`,
  );
const copy = (from, to) => {
  fs.mkdirSync(path.dirname(to), { recursive: true });
  fs.cpSync(from, to, {
    recursive: true,
    filter: (p) => !p.includes("__pycache__"),
  });
};
copy(path.join(ui, "node_modules/electron/dist"), output);
fs.renameSync(
  path.join(output, "electron.exe"),
  path.join(output, "Sector.exe"),
);
fs.rmSync(path.join(output, "resources/default_app.asar"), { force: true });
const app = path.join(output, "resources/app");
for (const name of ["main", "shared", "dist", "THIRD_PARTY_NOTICES.md"])
  copy(path.join(ui, name), path.join(app, name));
fs.writeFileSync(
  path.join(app, "package.json"),
  JSON.stringify({
    name: "sector",
    productName: "Sector",
    version,
    main: "main/index.cjs",
  }),
);
copy(
  path.join(root, "src/python/sector/desktop/reset_keymap.json"),
  path.join(app, "shared/reset_keymap.json"),
);
copy(path.join(ui, "licenses"), path.join(app, "licenses"));
copy(
  path.join(ui, "renderer/overlays/fonts/LICENSE.txt"),
  path.join(app, "licenses/overlay-fonts.txt"),
);
copy(
  path.join(root, "third_party/minhook/LICENSE.txt"),
  path.join(app, "licenses/minhook.txt"),
);
function copyLicenses(directory) {
  for (const entry of fs.readdirSync(directory, { withFileTypes: true })) {
    if (entry.name === ".bin") continue;
    const file = path.join(directory, entry.name);
    if (entry.isDirectory()) copyLicenses(file);
    else if (/^(license|copying|notice)(\.|$)/i.test(entry.name))
      copy(
        file,
        path.join(
          app,
          "licenses/npm",
          path.relative(path.join(ui, "node_modules"), file),
        ),
      );
  }
}
copyLicenses(path.join(ui, "node_modules"));
const runtime = path.join(output, "resources/runtime");
for (const name of ["src/python", "assets/sector.ico", "scripts/sector.py"])
  copy(path.join(root, name), path.join(runtime, name));
for (const name of [
  "diagnostic-14200.exe",
  "overlay-reader.exe",
  "lmu-physical-reset-v15-25661166.dll",
])
  copy(path.join(root, "bin", name), path.join(runtime, "bin", name));
fs.mkdirSync(path.join(runtime, "compatibility"), { recursive: true });
for (const name of fs
  .readdirSync(path.join(root, "compatibility"))
  .filter((n) => /^steam-25661166-.*\.json$/.test(n))) {
  const profile = JSON.parse(
    fs
      .readFileSync(path.join(root, "compatibility", name), "utf8")
      .replace(/^\uFEFF/, ""),
  );
  delete profile.install_path;
  delete profile.previous_dll_path;
  fs.writeFileSync(
    path.join(runtime, "compatibility", name),
    JSON.stringify(profile),
  );
}
// Verify the shipped host and reader against the same profiles used at runtime.
const sha = (file) =>
  crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex");
const telemetry = JSON.parse(
  fs.readFileSync(
    path.join(runtime, "compatibility/steam-25661166-telemetry.json"),
  ),
);
const host = JSON.parse(
  fs.readFileSync(
    path.join(runtime, "compatibility/steam-25661166-physical-reset-v15.json"),
  ),
);
if (
  sha(path.join(runtime, "bin/diagnostic-14200.exe")) !==
    telemetry.diagnostic_sha256.toLowerCase() ||
  sha(path.join(runtime, "bin/lmu-physical-reset-v15-25661166.dll")) !==
    host.dll_sha256.toLowerCase()
)
  throw new Error("Runtime binary/profile mismatch");
const pythonArchive = path.join(
  root,
  "work/release-tools/python-3.13.12-embed-amd64.zip",
);
if (!fs.existsSync(pythonArchive))
  throw new Error(
    `Download the official Python embedded archive to ${pythonArchive}`,
  );
if (
  sha(pythonArchive) !==
  "76f238f606250c87c6beac75dccd35ee99070a13490555936abb6cb64ecce3d0"
)
  throw new Error("Embedded Python archive checksum mismatch");
execFileSync(
  "powershell.exe",
  [
    "-NoProfile",
    "-Command",
    `$env:PSModulePath = (Join-Path $PSHOME 'Modules'); Expand-Archive -LiteralPath '${pythonArchive.replaceAll("'", "''")}' -DestinationPath '${path.join(runtime, "python").replaceAll("'", "''")}'`,
  ],
  { stdio: "inherit" },
);
await rcedit(path.join(output, "Sector.exe"), {
  icon: path.join(root, "assets/sector.ico"),
  "file-version": version,
  "product-version": version,
  "version-string": {
    ProductName: "Sector",
    FileDescription: "Sector",
    CompanyName: "Sector",
    OriginalFilename: "Sector.exe",
  },
  "requested-execution-level": "asInvoker",
});
const manifest = {};
function inventory(dir) {
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const file = path.join(dir, entry.name);
    if (entry.isDirectory()) inventory(file);
    else
      manifest[path.relative(runtime, file).replaceAll("\\", "/")] = sha(file);
  }
}
inventory(runtime);
fs.writeFileSync(
  path.join(runtime, "release-manifest.json"),
  JSON.stringify(manifest, null, 2),
);
fs.writeFileSync(
  path.join(output, "resources/release-id"),
  sha(path.join(runtime, "release-manifest.json")).slice(0, 16),
);
fs.writeFileSync(
  path.join(output, "README.txt"),
  `Sector ${version}\r\nOpen Sector.exe. Keep this folder together. No Node or Python installation required.\r\nUse the verified LMU build in offline Practice without EAC.\r\nOverlays require windowed or borderless mode.\r\n`,
);
console.log(output);
