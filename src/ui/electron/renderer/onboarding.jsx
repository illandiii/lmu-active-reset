import React, { useEffect, useRef, useState } from "react";
import { Dialog } from "@base-ui/react/dialog";
import { Icon } from "@iconify/react";
import folderIcon from "@iconify-icons/solar/folder-open-linear";
import helpIcon from "@iconify-icons/solar/question-circle-linear";
import closeIcon from "@iconify-icons/solar/close-circle-linear";

export function Onboarding({ open, initialFolder, onFinish, onQuit }) {
  const [step, setStep] = useState("folder");
  const [folder, setFolder] = useState(initialFolder || "");
  const [searching, setSearching] = useState(!initialFolder);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState("");
  const popup = useRef(null);
  const api = window.sector;

  useEffect(() => {
    if (initialFolder) return;
    let live = true;
    api
      .detectInstallations()
      .then((folders) => {
        if (live) setFolder(folders[0] || "");
      })
      .catch(() => {
        if (live) setError("Select the LMU folder to continue.");
      })
      .finally(() => {
        if (live) setSearching(false);
      });
    return () => {
      live = false;
    };
  }, [initialFolder]);

  async function run(action) {
    setBusy(true);
    setError("");
    try {
      await action();
    } catch (e) {
      setError(e.message || "Could not save setup. Please try again.");
    } finally {
      setBusy(false);
    }
  }

  function browse() {
    void run(async () => {
      const result = await api.browseInstallation();
      if (result.canceled) return;
      if (!result.ok) throw new Error(result.error);
      setFolder(result.folder);
    });
  }

  function confirm() {
    void run(async () => {
      const result = await api.confirmInstallation(folder);
      if (!result.ok) throw new Error(result.error);
      setStep("tutorial");
      requestAnimationFrame(() => popup.current?.focus());
    });
  }

  function finish(tutorial) {
    void run(async () => {
      const result = await api.completeOnboarding();
      if (!result.ok) throw new Error(result.error);
      onFinish(tutorial);
    });
  }

  return (
    <Dialog.Root open={open} disablePointerDismissal>
      <Dialog.Portal>
        <Dialog.Backdrop className="modal-backdrop" />
        <Dialog.Viewport className="modal-viewport">
          <Dialog.Popup
            className="modal onboarding"
            ref={popup}
            initialFocus={popup}
          >
            <button
              className="modal-dismiss"
              aria-label="Quit setup"
              onClick={onQuit}
              disabled={busy}
            >
              <Icon icon={closeIcon} aria-hidden="true" />
            </button>
            <div className="modal-icon">
              <Icon
                icon={step === "folder" ? folderIcon : helpIcon}
                aria-hidden="true"
              />
            </div>
            <Dialog.Title>
              {step === "folder"
                ? "Connect Le Mans Ultimate"
                : "Ready for a quick tour?"}
            </Dialog.Title>
            <Dialog.Description>
              {step === "folder"
                ? "Confirm your LMU folder, or choose another."
                : "Learn how to save, restore and customize your overlays."}
            </Dialog.Description>
            {step === "folder" ? (
              <>
                <div className="installation-folder" role="status">
                  <Icon icon={folderIcon} aria-hidden="true" />
                  <span>
                    {searching
                      ? "Looking for LMU…"
                      : folder || "Choose your LMU folder"}
                  </span>
                </div>
                <div className="onboarding-actions">
                  <button
                    className="secondary"
                    onClick={browse}
                    disabled={busy || searching}
                  >
                    Browse…
                  </button>
                  <button
                    className="primary"
                    onClick={confirm}
                    disabled={busy || searching || !folder}
                  >
                    Confirm folder
                  </button>
                </div>
              </>
            ) : (
              <div className="onboarding-actions">
                <button
                  className="secondary"
                  onClick={() => finish(false)}
                  disabled={busy}
                >
                  Not now
                </button>
                <button
                  className="primary"
                  onClick={() => finish(true)}
                  disabled={busy}
                >
                  Start guide
                </button>
              </div>
            )}
            {error && (
              <p className="modal-error" role="alert">
                {error}
              </p>
            )}
          </Dialog.Popup>
        </Dialog.Viewport>
      </Dialog.Portal>
    </Dialog.Root>
  );
}
