import React from "react";
import releases from "../shared/changelog.json";
import "./changelog.css";

export function Changelog() {
  return (
    <div className="changelog-list" tabIndex={0} aria-label="Release history">
      {releases.map(({ version, changes }, index) => (
        <section key={version} aria-labelledby={`release-${version}`}>
          <h3 id={`release-${version}`}>
            {version}
            {index === 0 && <span>Latest</span>}
          </h3>
          <ul>
            {changes.map((change) => (
              <li key={change}>{change}</li>
            ))}
          </ul>
        </section>
      ))}
    </div>
  );
}
