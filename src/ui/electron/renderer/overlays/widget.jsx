import React from "react";
import logoSheet from "./logos.png";
import "./fonts.css";
import "./widgets.css";
const sources = import.meta.glob("./designs/*.html", {
  eager: true,
  query: "?raw",
  import: "default",
});
const templates = Object.fromEntries(
  Object.entries(sources).map(([name, source]) => {
    const doc = new DOMParser().parseFromString(source, "text/html");
    return [
      name.split("/").at(-1).replace(".html", ""),
      doc.body.firstElementChild,
    ];
  }),
);
const makes = [
  "BMW",
  "MER",
  "FOR",
  "COR",
  "FER",
  "LAM",
  "MCL",
  "POR",
  "AST",
  "TOY",
  "CAD",
  "LEX",
  "ALP",
  "PEU",
  "ORE",
  "IF",
  "LIG",
  "DUQ",
];
export function Logo({ code, badge = false }) {
  const index = (badge ? ["HY", "P2", "GT3", "P3"] : makes).indexOf(code);
  if (index < 0) return null;
  const x = 45 + (index % 6) * 150,
    y = 30 + Math.floor(index / 6) * 140;
  return (
    <svg
      className="manufacturer-logo"
      role="img"
      aria-label={code}
      viewBox={badge ? `${60 + index * 150} 473 71 39` : `${x} ${y} 100 92`}
      style={{ width: badge ? 32 : 28, height: badge ? 18 : 22 }}
    >
      <image href={logoSheet} width="940" height="600" />
    </svg>
  );
}
const lookup = (context, path) =>
  path
    .trim()
    .split(".")
    .reduce((v, k) => v?.[k], context);
function bind(value, context) {
  return value.replace(/{{\s*([\w.]+)\s*}}/g, (_, key) =>
    String(lookup(context, key) ?? ""),
  );
}
const camel = (s) => s.replace(/-([a-z])/g, (_, c) => c.toUpperCase());
function renderNode(node, context, key) {
  if (node.nodeType === 3) return bind(node.textContent, context);
  if (node.nodeType !== 1) return null;
  const tag = node.localName;
  if (tag === "sc-if") {
    const name = node.getAttribute("value").replace(/[{}]/g, "");
    return lookup(context, name)
      ? [...node.childNodes].map((n, i) =>
          renderNode(n, context, `${key}-${i}`),
        )
      : null;
  }
  if (tag === "sc-for") {
    const list =
      lookup(context, node.getAttribute("list").replace(/[{}]/g, "")) || [];
    return list.map((entry, i) =>
      [...node.childNodes].map((n, j) =>
        renderNode(
          n,
          { ...context, [node.getAttribute("as")]: entry },
          `${key}-${i}-${j}`,
        ),
      ),
    );
  }
  if (tag === "img")
    return context.showLogos ? <Logo key={key} code={context.r?.make} /> : null;
  if (
    ["{{cl.badge}}", "{{r.cls}}", "{{classBadge}}"].includes(
      node.textContent.trim(),
    )
  ) {
    return (
      <Logo
        key={key}
        code={lookup(context, node.textContent.replace(/[{}]/g, ""))}
        badge
      />
    );
  }
  const props = { key };
  for (const { name, value } of node.attributes) {
    if (name.startsWith("hint-")) continue;
    if (name === "style") {
      props.style = Object.fromEntries(
        value
          .split(";")
          .map((s) => s.split(/:(.*)/s).slice(0, 2))
          .filter(([k, v]) => k?.trim() && v !== undefined)
          .map(([k, v]) => [camel(k.trim()), bind(v.trim(), context)]),
      );
    } else {
      const attr =
        name === "class"
          ? "className"
          : name === "sc-camel-view-box"
            ? "viewBox"
            : name === "sc-camel-preserve-aspect-ratio"
              ? "preserveAspectRatio"
              : name.startsWith("aria-")
                ? name
                : camel(name);
      props[attr] = bind(value, context);
    }
  }
  if (tag === "svg")
    props.style = {
      width: Number(props.width) || undefined,
      height: Number(props.height) || undefined,
      ...props.style,
    };
  return React.createElement(
    tag,
    props,
    ...[...node.childNodes].map((n, i) =>
      renderNode(n, context, `${key}-${i}`),
    ),
  );
}
export function Widget({ id, values, settings }) {
  return (
    <div
      className={`lmu-widget widget-${id}`}
      data-clutch={settings.showClutch}
      data-trace={settings.showTrace}
      data-wheel={settings.showWheel}
      data-gear={settings.showGear}
    >
      {renderNode(
        templates[id],
        {
          ...values,
          showLogos: settings.showLogos,
          rootClass: "clear",
          panelAlpha: settings.opacity,
          bgAlpha: settings.opacity,
          boxAlpha: 0.45,
          accent: settings.accent,
        },
        id,
      )}
    </div>
  );
}
