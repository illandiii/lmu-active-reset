import React from "react";
import { Icon } from "@iconify/react";
import play from "@iconify-icons/solar/play-linear";
import { DisplaySettings } from "./overlays/display-settings.jsx";

export function Preferences({ settings, pending, updateSettings }) {
  return (
    <div className="preferences">
      <h3 className="preference-category">Appearance</h3>
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
      <DisplaySettings />
    </div>
  );
}

export function AudioFeedback({ settings, pending, updateSettings, sound }) {
  return (
    <section
      className="audio-section"
      data-tour="audio"
      aria-labelledby="audio-title"
    >
      <div className="section-heading">
        <h2 id="audio-title">Audio feedback</h2>
      </div>
      <div className="setting-row">
        <h3 id="sound-label">Confirmation sounds</h3>
        <button
          role="switch"
          aria-labelledby="sound-label"
          aria-checked={settings?.sound}
          className="switch"
          disabled={!settings || pending}
          onClick={() =>
            updateSettings({ ...settings, sound: !settings?.sound })
          }
        >
          <span />
        </button>
      </div>
      <div className="sound-previews">
        <button
          disabled={!settings?.sound || pending}
          onClick={() => sound("save")}
        >
          <Icon icon={play} aria-hidden="true" />
          Play save sound
        </button>
        <button
          disabled={!settings?.sound || pending}
          onClick={() => sound("restore")}
        >
          <Icon icon={play} aria-hidden="true" />
          Play restore sound
        </button>
      </div>
    </section>
  );
}
