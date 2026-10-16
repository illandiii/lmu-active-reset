const { contextBridge, ipcRenderer } = require("electron");
contextBridge.exposeInMainWorld("sectorOverlay", {
  initialize: () => ipcRenderer.invoke("sector:overlay-ready"),
  scale: (value) => ipcRenderer.invoke("sector:overlay-scale", value),
  lock: () => ipcRenderer.invoke("sector:overlay-lock"),
  onFrame: (callback) => {
    const handler = (_event, data) => callback(data);
    ipcRenderer.on("sector:overlay-frame", handler);
    return () => ipcRenderer.removeListener("sector:overlay-frame", handler);
  },
});
