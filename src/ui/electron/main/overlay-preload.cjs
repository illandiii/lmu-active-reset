const { contextBridge, ipcRenderer } = require("electron");
contextBridge.exposeInMainWorld("sectorOverlay", {
  initialize: () => ipcRenderer.invoke("sector:overlay-ready"),
  scale: (value, commit = true) =>
    ipcRenderer.invoke("sector:overlay-scale", value, commit),
  lock: () => ipcRenderer.invoke("sector:overlay-lock"),
  onFrame: (callback) => {
    const handler = (_event, data) => callback(data);
    ipcRenderer.on("sector:overlay-frame", handler);
    return () => ipcRenderer.removeListener("sector:overlay-frame", handler);
  },
});
