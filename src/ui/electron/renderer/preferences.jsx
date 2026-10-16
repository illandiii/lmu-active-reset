import React from "react";
import { Icon } from "@iconify/react";
import play from "@iconify-icons/solar/play-linear";

export function Preferences({ settings, pending, updateSettings, sound }) {
  return (
    <div className="preferences">
      <div className="setting-row">
        <h3 id="sound-label">Confirmation sounds</h3>
        <button
          role="switch"
          aria-labelledby="sound-label"
          aria-checked={settings.sound}
          className="switch"
          disabled={pending}
          onClick={() =>
            updateSettings({ ...settings, sound: !settings.sound })
          }
        >
          <span />
        </button>
      </div>
      <div className="sound-previews">
        <button
          disabled={!settings.sound || pending}
          onClick={() => sound("save")}
        >
          <Icon icon={play} aria-hidden="true" />
          Preview save
        </button>
        <button
          disabled={!settings.sound || pending}
          onClick={() => sound("restore")}
        >
          <Icon icon={play} aria-hidden="true" />
          Preview restore
        </button>
      </div>
      <div className="setting-row motion-setting">
        <h3 id="motion-label">Reduce motion</h3>
        <button
          role="switch"
          aria-labelledby="motion-label"
          aria-checked={settings.reduce_motion}
          className="switch"
          disabled={pending}
          onClick={() =>
            updateSettings({
              ...settings,
              reduce_motion: !settings.reduce_motion,
            })
          }
        >
          <span />
        </button>
      </div>
    </div>
  );
}
