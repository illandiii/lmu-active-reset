import assert from "node:assert/strict";
import path from "node:path";
import { execFileSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import model from "../shared/overlay-model.cjs";
import config from "../shared/overlay-config.cjs";

const root = path.resolve(
  fileURLToPath(new URL("../../../..", import.meta.url)),
);
const raw = JSON.parse(
  execFileSync(path.join(root, "bin/overlay-reader.exe"), ["--test-frame"], {
    encoding: "utf8",
    windowsHide: true,
  }),
);
assert.equal(raw.player.wheels.length, 4);
assert.deepEqual(raw.player.wheels[0].temperatures, [98, 93, 88]);
assert.equal(raw.player.wheels[0].radius, 0.31);
assert.equal(raw.player.wheels[0].wear, 0.71);
assert.deepEqual(raw.player.wheels[1].temperatures, [null, null, null]);
const data = new model.TelemetryModel().update(raw);
const settings = config.defaults().widgets;
assert.equal(
  model.valuesFor("tyres", data, settings.tyres).tyres[0].lifePct,
  71,
);
assert.equal(
  model.valuesFor("brake-lock", data, settings["brake-lock"]).tyres[0].locked,
  true,
);
assert.equal(
  model.valuesFor("brake-lock", data, settings["brake-lock"]).tyres[1].locked,
  false,
);
assert.equal(
  model.valuesFor("stint-planner", data, settings["stint-planner"]).boxLaps,
  "-",
);
console.log(
  "PASS native SDK serialization through the production wheel and stint models.",
);
