#pragma once

#include <pgmspace.h>

void webHandlers_registerSetup();
void webHandlers_registerNormal();

static const char SETUP_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>VORIX</title>
<style>
*{box-sizing:border-box}
body{margin:0;background:#050708;color:#e0e8ff;font-family:Consolas,monospace;padding:18px;max-width:440px;margin:0 auto}
h1{color:#e0e8ff;letter-spacing:.2em;margin-bottom:2px;font-size:1.3rem}
h1 span{color:#ab03ff}
.s{color:#4a5a7a;font-size:.8rem;margin:0 0 18px}
.g{color:#ab03ff;font-size:.68rem;letter-spacing:.12em;border-bottom:1px solid #1a2030;padding-bottom:3px;margin:14px 0 8px}
label{display:block;color:#4a5a7a;font-size:.7rem;margin-bottom:2px}
input{width:100%;background:#080c10;border:1px solid #1a2030;color:#00e5ff;padding:8px 10px;margin-bottom:10px;font:inherit;font-size:.85rem}
input:focus{outline:none;border-color:#ab03ff}
button{width:100%;background:transparent;color:#ab03ff;border:1px solid #ab03ff;padding:11px;font:inherit;cursor:pointer;margin-top:6px}
button:hover{background:rgba(171,3,255,.1)}
</style></head><body>
<h1>VOR<span>IX</span></h1><p class="s">ADS-B aviation companion — setup</p>
<form action="/save" method="POST">
<div class="g">NETWORK</div>
<label>WIFI SSID</label><input name="ssid" id="ssid" required autocomplete="off">
<label>WIFI PASSWORD</label><input type="password" name="pass">
<div class="g">LOCATION</div>
<label>LATITUDE</label><input name="lat" id="lat" inputmode="decimal" placeholder="e.g. 51.4775" required>
<label>LONGITUDE</label><input name="lon" id="lon" inputmode="decimal" placeholder="e.g. -0.4614" required>
<label>SCAN RADIUS (km)</label><input name="radius" id="radius" value="100" inputmode="numeric" min="10" max="250">
<button type="submit">&#9658; SAVE &amp; REBOOT</button>
</form><script>
['ssid','lat','lon','radius'].forEach(function(id){
  var el=document.getElementById(id), key='vorix_'+id;
  if(sessionStorage.getItem(key)) el.value=sessionStorage.getItem(key);
  el.addEventListener('input',function(){sessionStorage.setItem(key,el.value);});
});
</script></body></html>)HTML";

static const char DASH_HTML[] PROGMEM = R"RAWHTML(<!DOCTYPE html>
<html><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>VORIX</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
:root{--bg:#050708;--panel:#0c0f10;--line:#1a2030;--accent:#ab03ff;--cyan:#00e5ff;--text:#e0e8ff;--dim:#4a5a7a;--pos:#00e676;--neg:#ff1744}
body{background:var(--bg);color:var(--text);font-family:Consolas,monospace;min-height:100vh}
header{display:flex;justify-content:space-between;align-items:center;padding:10px 16px;border-bottom:1px solid var(--line)}
.logo{color:var(--text);font-size:1.1rem;font-weight:700;letter-spacing:.3em}
.logo span{color:var(--accent)}
.st{font-size:.7rem;color:var(--dim)}
.dot{display:inline-block;width:7px;height:7px;border-radius:50%;background:var(--pos);margin-right:5px;animation:p 2s infinite}
@keyframes p{0%,100%{opacity:1}50%{opacity:.3}}
main{max-width:480px;margin:0 auto;padding:14px;display:flex;flex-direction:column;gap:12px}
.card{background:var(--panel);border:1px solid var(--line);padding:14px}
h2{font-size:.65rem;letter-spacing:.15em;color:var(--accent);margin-bottom:10px;text-transform:uppercase}
.row{display:flex;justify-content:space-between;padding:7px 0;border-bottom:1px solid var(--line)}
.row:last-child{border:none}
.lbl{font-size:.72rem;color:var(--dim)}
.val{font-size:.72rem;color:var(--cyan)}
label{display:block;font-size:.68rem;color:var(--dim);margin-bottom:3px}
input{width:100%;background:#080c10;border:1px solid var(--line);color:var(--cyan);padding:7px 10px;font:inherit;font-size:.82rem;margin-bottom:9px}
input:focus{outline:none;border-color:var(--accent)}
button{background:transparent;color:var(--accent);border:1px solid var(--accent);padding:9px 18px;font:inherit;font-size:.78rem;cursor:pointer}
button:hover{background:rgba(171,3,255,.1)}
.btn-neg{color:var(--neg);border-color:var(--neg)}
.msg{font-size:.72rem;color:var(--pos);margin-top:6px;min-height:16px}
</style></head><body>
<header>
  <div class="logo">VOR<span>IX</span></div>
  <div class="st"><span class="dot" id="dot"></span><span id="st">--</span></div>
</header>
<main>
  <div class="card">
    <h2>Status</h2>
    <div class="row"><span class="lbl">IP</span><span class="val" id="s-ip">--</span></div>
    <div class="row"><span class="lbl">WiFi</span><span class="val" id="s-ssid">--</span></div>
    <div class="row"><span class="lbl">Aircraft in range</span><span class="val" id="s-ac">--</span></div>
    <div class="row"><span class="lbl">Mode</span><span class="val" id="s-mode">--</span></div>
    <div class="row"><span class="lbl">Scan radius</span><span class="val" id="s-rad">--</span></div>
    <div class="row"><span class="lbl">Location</span><span class="val" id="s-loc">--</span></div>
    <div class="row"><span class="lbl">Last fetch</span><span class="val" id="s-fetch">--</span></div>
  </div>
  <div class="card">
    <h2>Location &amp; Scan</h2>
    <label>LATITUDE</label><input id="c-lat" type="number" step="0.0001">
    <label>LONGITUDE</label><input id="c-lon" type="number" step="0.0001">
    <label>RADIUS km (10–250)</label><input id="c-rad" type="number" min="10" max="250">
    <button onclick="saveLoc()">&#9658; APPLY</button>
    <div class="msg" id="m-loc"></div>
  </div>
  <div class="card">
    <h2>WiFi</h2>
    <label>SSID</label><input id="c-ssid" autocomplete="off">
    <label>PASSWORD</label><input id="c-pass" type="password">
    <button onclick="saveWifi()">&#9658; SAVE &amp; REBOOT</button>
    <div class="msg" id="m-wifi"></div>
  </div>
</main>
<script>
var editing=false;
document.addEventListener('focusin',function(e){if(e.target.tagName==='INPUT') editing=true;});
document.addEventListener('focusout',function(e){if(e.target.tagName==='INPUT') setTimeout(function(){editing=false;},250);});
document.addEventListener('input',function(e){if(e.target.tagName==='INPUT') e.target.dataset.dirty='1';});
function fillIfIdle(id,val){
  var el=document.getElementById(id);
  if(!editing && document.activeElement!==el && !el.dataset.dirty && !el.value) el.value=val;
}
function load(){
  fetch('/api').then(r=>r.json()).then(d=>{
    document.getElementById('dot').style.background='#00e676';
    document.getElementById('st').textContent='online';
    document.getElementById('s-ip').textContent=d.ip||'--';
    document.getElementById('s-ssid').textContent=d.ssid||'--';
    document.getElementById('s-ac').textContent=d.acCount;
    document.getElementById('s-mode').textContent=d.mode||'--';
    document.getElementById('s-rad').textContent=d.radius+'km';
    document.getElementById('s-loc').textContent=d.lat.toFixed(4)+', '+d.lon.toFixed(4);
    var ago=Math.round((Date.now()-d.fetchMs)/1000);
    document.getElementById('s-fetch').textContent=ago+'s ago';
    fillIfIdle('c-lat',d.lat.toFixed(4));
    fillIfIdle('c-lon',d.lon.toFixed(4));
    fillIfIdle('c-rad',d.radius);
    fillIfIdle('c-ssid',d.ssid||'');
  }).catch(function(){
    document.getElementById('dot').style.background='#ff1744';
    document.getElementById('st').textContent='offline';
  });
}
function saveLoc(){
  var lat=document.getElementById('c-lat').value,lon=document.getElementById('c-lon').value,rad=document.getElementById('c-rad').value,msg=document.getElementById('m-loc');
  if(!lat||!lon){msg.textContent='Enter lat and lon';msg.style.color='#ff1744';return;}
  fetch('/setloc',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'lat='+encodeURIComponent(lat)+'&lon='+encodeURIComponent(lon)+'&radius='+encodeURIComponent(rad)})
  .then(()=>{msg.textContent='Saved.';msg.style.color='#00e676';}).catch(()=>{msg.textContent='Error';msg.style.color='#ff1744';});
}
function saveWifi(){
  var ssid=document.getElementById('c-ssid').value,pass=document.getElementById('c-pass').value,msg=document.getElementById('m-wifi');
  if(!ssid){msg.textContent='Enter SSID';msg.style.color='#ff1744';return;}
  if(!confirm('Save WiFi and reboot?')) return;
  fetch('/setwifi',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'ssid='+encodeURIComponent(ssid)+'&pass='+encodeURIComponent(pass)})
  .then(()=>{msg.textContent='Rebooting...';msg.style.color='#00e676';}).catch(()=>{msg.textContent='Error';msg.style.color='#ff1744';});
}
load();setInterval(load,5000);
</script></body></html>)RAWHTML";
