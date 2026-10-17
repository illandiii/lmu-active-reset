const compounds = {
  S: ["Soft", "#F2F4F6"],
  M: ["Medium", "#F5C518"],
  H: ["Hard", "#E5322F"],
  W: ["Wet", "#2D9CDB"],
};
function compound(name) {
  const code = /soft/i.test(name || "")
    ? "S"
    : /medium/i.test(name || "")
      ? "M"
      : /hard/i.test(name || "")
        ? "H"
        : /wet|rain/i.test(name || "")
          ? "W"
          : "-";
  return {
    code,
    name: compounds[code]?.[0] || "Unavailable",
    color: compounds[code]?.[1] || "#677383",
  };
}
function resourceValues(resources) {
  const tyres = [0, 1, 2, 3].map((i) => compound(resources?.compounds?.[i]));
  const mixed = tyres.some((t) => t.code !== tyres[0].code);
  const energy = resources?.energy;
  const hasEnergy = Number.isFinite(energy) && energy >= 0 && energy <= 1;
  return {
    uniform: !mixed,
    mixed,
    tc: tyres[0].color,
    tl: tyres[0].code,
    wheels: tyres.map((t) => ({ c: t.color })),
    tyreTitle: tyres
      .map((t, i) => `${["FL", "FR", "RL", "RR"][i]} ${t.name}`)
      .join(", "),
    veTxt: hasEnergy ? `${Math.round(energy * 100)}%` : "-",
    veFg: hasEnergy && energy < 0.15 ? "#FF8A80" : "#C2C9D0",
  };
}
module.exports = { compound, resourceValues };
