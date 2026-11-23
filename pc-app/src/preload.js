const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('opendws', {
  onSerialPortList(handler) {
    if (typeof handler !== 'function') return () => {};
    const listener = (_event, ports) => handler(ports);
    ipcRenderer.on('serial:port-list', listener);
    return () => ipcRenderer.removeListener('serial:port-list', listener);
  },
  selectSerialPort(portId) {
    return ipcRenderer.invoke('serial:select-port', portId);
  },
  saveCsv(csvContent) {
    return ipcRenderer.invoke('csv:save', csvContent);
  },
  detectBootloader() {
    return ipcRenderer.invoke('fw:detect-bootloader');
  },
  pickLocalBin() {
    return ipcRenderer.invoke('fw:pick-local-bin');
  },
  flashLocal(payload) {
    return ipcRenderer.invoke('fw:flash-local', payload);
  },
  checkGithubRelease(token) {
    return ipcRenderer.invoke('fw:check-github', { token });
  },
  downloadAndFlash(payload) {
    return ipcRenderer.invoke('fw:download-and-flash', payload);
  },
  openExternal(url) {
    return ipcRenderer.invoke('app:open-external', url);
  },
});
