const fs = require("node:fs");
const path = require("node:path");
const { randomUUID } = require("node:crypto");
const { execFile } = require("node:child_process");
const { promisify } = require("node:util");

const executable = "Le Mans Ultimate.exe";

function validateFolder(folder) {
  if (typeof folder !== "string" || !path.isAbsolute(folder))
    throw new Error("Select the Le Mans Ultimate installation folder.");
  const resolved = path.resolve(folder);
  try {
    if (fs.statSync(path.join(resolved, executable)).isFile()) return resolved;
  } catch {}
  throw new Error("Le Mans Ultimate.exe was not found in this folder.");
}

function readSetup(filename) {
  try {
    const value = JSON.parse(fs.readFileSync(filename, "utf8"));
    return {
      lmuPath: validateFolder(value.lmuPath),
      completed: value.completed === true,
    };
  } catch {
    // A removed installation or interrupted setup must show onboarding again.
    return { lmuPath: "", completed: false };
  }
}

function writeSetup(filename, value) {
  const state = {
    lmuPath: validateFolder(value.lmuPath),
    completed: value.completed === true,
  };
  fs.mkdirSync(path.dirname(filename), { recursive: true });
  const temporary = `${filename}.${randomUUID()}.tmp`;
  try {
    fs.writeFileSync(temporary, JSON.stringify(state), "utf8");
    fs.renameSync(temporary, filename);
  } finally {
    fs.rmSync(temporary, { force: true });
  }
  return state;
}

function readText(filename) {
  try {
    return fs.readFileSync(filename, "utf8");
  } catch {
    return "";
  }
}

function steamCandidates(roots) {
  const libraries = new Set(roots.filter(Boolean));
  for (const root of [...libraries]) {
    const vdf = readText(path.join(root, "steamapps/libraryfolders.vdf"));
    // Steam has used both numbered string entries and nested "path" entries.
    for (const match of vdf.matchAll(/"(?:path|\d+)"\s+"((?:\\.|[^"\\])*)"/g)) {
      const library = match[1].replace(/\\\\/g, "\\").replace(/\\"/g, '"');
      if (path.isAbsolute(library)) libraries.add(library);
    }
  }
  const found = new Set();
  for (const library of libraries) {
    const manifest = readText(
      path.join(library, "steamapps/appmanifest_2399420.acf"),
    );
    const directory = /"installdir"\s+"([^"\r\n]+)"/.exec(manifest)?.[1];
    for (const name of [directory, "Le Mans Ultimate"].filter(Boolean)) {
      try {
        found.add(validateFolder(path.join(library, "steamapps/common", name)));
      } catch {}
    }
  }
  return [...found];
}

async function detectInstallations() {
  const roots = [process.env["ProgramFiles(x86)"], process.env.ProgramFiles]
    .filter(Boolean)
    .map((root) => path.join(root, "Steam"));
  const entries = [
    ["HKCU\\Software\\Valve\\Steam", "SteamPath"],
    ["HKLM\\SOFTWARE\\WOW6432Node\\Valve\\Steam", "InstallPath"],
    ["HKLM\\SOFTWARE\\Valve\\Steam", "InstallPath"],
  ];
  await Promise.all(
    entries.map(async ([key, name]) => {
      try {
        const { stdout } = await promisify(execFile)(
          "reg.exe",
          ["query", key, "/v", name],
          { windowsHide: true, timeout: 3000 },
        );
        const value = /REG_SZ\s+(.+)/.exec(stdout)?.[1]?.trim();
        if (value) roots.push(value);
      } catch {}
    }),
  );
  return steamCandidates(roots);
}

module.exports = {
  validateFolder,
  readSetup,
  writeSetup,
  steamCandidates,
  detectInstallations,
};
