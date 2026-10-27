const {app, BrowserWindow, protocol, net} = require('electron');
const path = require('path');
const fs = require('fs');

// eigen protocol zodat fetch('drumsnd/...') en fetch('melsnd/...') lokaal werken (regel 1: geen cloud)
protocol.registerSchemesAsPrivileged([
  {scheme:'app', privileges:{standard:true, secure:true, supportFetchAPI:true, stream:true, bypassCSP:true}}
]);

app.whenReady().then(() => {
  protocol.handle('app', (req) => {
    const u = new URL(req.url);
    let p = decodeURIComponent(u.pathname);
    const fp = path.join(__dirname, 'app', p);
    if (fs.existsSync(fp) && fs.statSync(fp).isDirectory()) p = path.join(p, 'index.html');
    return net.fetch('file://' + path.join(__dirname, 'app', p));
  });
  const w = new BrowserWindow({width: 1280, height: 940, autoHideMenuBar: true, backgroundColor: '#12081e', title: 'G2B Beat Maker PRO'});
  w.loadURL('app://beat/index.html');
});
app.on('window-all-closed', () => app.quit());
