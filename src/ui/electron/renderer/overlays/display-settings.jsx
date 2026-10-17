import React, { useEffect, useState } from "react";
import { Select } from "@base-ui/react/select";
import { Icon } from "@iconify/react";
import monitor from "@iconify-icons/solar/monitor-linear";
import down from "@iconify-icons/solar/alt-arrow-down-linear";
import check from "@iconify-icons/solar/check-read-linear";

export function DisplaySettings() {
  const [state, setState] = useState(null);
  const [error, setError] = useState("");
  const [pending, setPending] = useState(false);
  useEffect(() => {
    let live = true;
    let received = false;
    const off = window.sector.onOverlays((value) => {
      received = true;
      if (live) setState(value);
    });
    window.sector
      .initializeOverlays()
      .then((value) => {
        if (live && !received) setState(value);
      })
      .catch((e) => {
        if (live) setError(e.message);
      });
    return () => {
      live = false;
      off();
    };
  }, []);
  async function choose(value) {
    if (!state || value === null) return;
    setPending(true);
    try {
      const result = await window.sector.saveOverlays({
        ...state.config,
        displayId: value === "primary" ? null : Number(value),
      });
      if (!result.ok) throw new Error(result.error);
      setState(result);
      setError("");
    } catch (e) {
      setError(e.message);
    } finally {
      setPending(false);
    }
  }
  const items = [
    { value: "primary", label: "Primary display" },
    ...(state?.displays || []).map((d) => ({
      value: String(d.id),
      label: d.label,
    })),
  ];
  const selected = items.some(
    (item) => item.value === String(state?.config.displayId),
  )
    ? String(state.config.displayId)
    : "primary";
  return (
    <section
      className="display-settings"
      aria-labelledby="overlay-settings-title"
    >
      <h3 id="overlay-settings-title" className="preference-category">
        Overlay
      </h3>
      <span id="overlay-display-label" className="preference-label">
        Display
      </span>
      <Select.Root
        items={items}
        value={selected}
        onValueChange={choose}
        disabled={!state || pending}
      >
        <Select.Trigger
          className="display-select"
          aria-labelledby="overlay-display-label"
        >
          <Icon icon={monitor} aria-hidden="true" />
          <Select.Value />
          <Select.Icon className="display-chevron">
            <Icon icon={down} aria-hidden="true" />
          </Select.Icon>
        </Select.Trigger>
        <Select.Portal>
          <Select.Positioner
            className="display-positioner"
            sideOffset={6}
            alignItemWithTrigger={false}
          >
            <Select.Popup className="display-popup">
              <Select.List>
                {items.map((item) => (
                  <Select.Item
                    key={item.value}
                    value={item.value}
                    className="display-option"
                  >
                    <Select.ItemText>{item.label}</Select.ItemText>
                    <Select.ItemIndicator>
                      <Icon icon={check} aria-hidden="true" />
                    </Select.ItemIndicator>
                  </Select.Item>
                ))}
              </Select.List>
            </Select.Popup>
          </Select.Positioner>
        </Select.Portal>
      </Select.Root>
      {error && (
        <p role="alert" className="modal-error">
          {error}
        </p>
      )}
    </section>
  );
}
