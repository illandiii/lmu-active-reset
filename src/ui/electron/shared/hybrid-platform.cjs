// Match vehicle models, not manufacturers shared with GT cars.
const normalize = (value) =>
  String(value || "")
    .toLowerCase()
    .replace(/[^a-z0-9]/g, "");
const profiles = [
  {
    name: "LMDh",
    step: 10,
    max: 50,
    models: [
      "porsche963",
      "cadillacvseriesr",
      "bmwmhybridv8",
      "alpinea424",
      "lamborghinisc63",
    ],
    short: ["963", "vseriesr", "mhybridv8", "a424", "sc63"],
  },
  {
    name: "LMH",
    step: 20,
    max: 200,
    models: [
      "ferrari499p",
      "toyotagr010",
      "peugeot9x8",
      "isottafraschinitipo6",
      "isottatipo6",
    ],
    short: ["499p", "gr010", "9x8", "tipo6", "tipo6lmh"],
  },
];
function vehicleIdentity(data) {
  return normalize(
    data?.player?.model ||
      data?.cars?.find((c) => c.id === data.player?.id)?.vehicle,
  );
}
function hybridPlatform(data) {
  const model = normalize(data?.player?.model);
  const vehicle = normalize(
    data?.cars?.find((c) => c.id === data.player?.id)?.vehicle,
  );
  return (
    profiles.find(
      (p) =>
        p.models.some((name) => model.includes(name)) ||
        p.short.includes(model),
    ) ||
    profiles.find((p) => p.models.some((name) => vehicle.includes(name))) ||
    null
  );
}
function deployMap(hybrid, profile) {
  if (
    !profile ||
    !Number.isInteger(hybrid?.map) ||
    !Number.isInteger(hybrid?.mapMax)
  )
    return null;
  const { map, mapMax } = hybrid;
  if (map < 0 || map > mapMax) return null;
  // Distinguish SDK step indices from an already scaled deployment value.
  if (mapMax === profile.max / profile.step) return map * profile.step;
  if (mapMax === profile.max && map % profile.step === 0) return map;
  return null;
}
function validDeployMap(value, profile) {
  return Boolean(
    profile &&
    Number.isInteger(value) &&
    value >= 0 &&
    value <= profile.max &&
    value % profile.step === 0,
  );
}
module.exports = { hybridPlatform, deployMap, validDeployMap, vehicleIdentity };
