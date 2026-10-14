
//⚠️

//======================================================{📜}======================================================
// ROZORC Firmware
//================================================================================================================

#define FW_NAME        "ROZORC"
#define FW_VERSION     "1.7.1-stable"
#define FW_AUTHOR      "Hussain Ali Mahmud"
#define FW_BUILD_DATE  __DATE__
#define FW_BUILD_TIME  __TIME__

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <DNSServer.h>
#include <Servo.h>
Servo steeringServo;
DNSServer dnsServer;

//======================================================(🔐)======================================================
// WIFI & SECRETS CONFIG
//================================================================================================================

//

const char* WIFI_SSID = "";
const char* WIFI_PASSWORD = "";
const uint8_t WIFI_CHANNEL = 1;
const uint8_t MAX_WIFI_CLIENT = 2;

const char* HARDCODED_OTA_PASSWORD = "";

#define RACE_PASSWORD ""

IPAddress activeDriverIP(0, 0, 0, 0);

//======================================================[🛠️]======================================================
// MOTOR CONFIG & POWER TABLES
//================================================================================================================

const uint16_t GEAR1_RACE = 700;
const uint16_t GEAR2_RACE = 800;
const uint16_t GEAR3_RACE = 900;
const uint16_t GEAR4_RACE = 1023;

const uint16_t GEAR1_SAFE = 600; 
const uint16_t GEAR2_SAFE = 700;
const uint16_t GEAR3_SAFE = 750;
const uint16_t GEAR4_SAFE = 800;

const uint8_t SERVO_CENTER = 90;  
const uint8_t SERVO_LEFT = 45;    
const uint8_t SERVO_RIGHT = 135;

//======================================================[🚨]======================================================
// FAIL SAFE & NETWORK
//================================================================================================================

const uint16_t FAILSAFE_TIMEOUT_MS = 800;
const uint16_t KEEP_ALIVE_INTERVAL_MS = 250;
const uint16_t WIFI_SYNC_INTERVAL_MS = 2000;
const uint16_t GEAR_DEBOUNCE_MS = 180;
const byte DNS_PORT = 53;

//======================================================[⚡]======================================================
// PINS
//================================================================================================================

const uint8_t pinDriveA = D1;
const uint8_t pinDriveB = D2;
const uint8_t pinServo = D6;
const uint8_t pinLED = D3;

//======================================================[💾]======================================================
// STORAGE & STATE
//================================================================================================================

struct Config {
  char ssid[33];
  char password[65];
  char racePassword[33];
};

Config config;
const char CONFIG_FILE[] = "/config.ini";
ESP8266WebServer server(80);

enum DriveMode : uint8_t { DRIVE_STOP, DRIVE_FWD, DRIVE_REV };
enum SteerMode : uint8_t { STEER_CENTER, STEER_LEFT, STEER_RIGHT };

struct CarState {
  uint8_t gear;
  uint8_t led;
  uint8_t drive;
  uint8_t steer;
};

enum RuntimeFlags : uint8_t {
  FLAG_FAILSAFE = 1 << 0
};

CarState car = {1, 0, DRIVE_STOP, STEER_CENTER};
bool isRaceMode = false;

bool isBraking = false;
unsigned long brakeTimeMs = 0;
uint8_t pendingDriveDir = DRIVE_STOP;

unsigned long lastControlMs = 0;
unsigned long lastGearPressMs = 0;
uint8_t lastStationCount = 255;
uint8_t runtimeFlags = 0;
bool pendingRestart = false;
unsigned long restartTimeMs = 0;
bool isOTA = false;

//======================================================[🕹️]======================================================
// HTML & UI (FIXED BUTTON TEXT VISIBILITY)
//================================================================================================================

const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="id"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="color-scheme" content="dark">
<title>ROZORC</title>
<style>
:root{--c:#00ffcc;--bg:#0b1014;--p:rgba(0,0,0,.72);--app-h:100vh;--b:84px;--g:48px;--gap:12px;--red:#ff4d4d;color-scheme:dark}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent;-webkit-user-select:none;user-select:none}
html,body{margin:0;height:100%}
body{background:var(--bg);color:#fff;font:600 16px/1.1 system-ui,sans-serif;overflow:hidden;overscroll-behavior:none;height:100vh;height:100dvh;height:var(--app-h)}
body{transition: background 0.5s ease-in-out; }
body.race-mode { background: linear-gradient(135deg, #004d40 0%, #05080a 100%); }
button{border:1px solid var(--c);background:var(--p);color:var(--c) !important;padding:0;cursor:pointer}
.on{background:var(--c) !important;color:#000 !important}
.btn-danger.on{background:var(--red) !important;border-color:var(--red) !important;color:#fff !important}
#start:active,#settingsBtn:active,#closeSettingsBtn:active,#settingsActions button:active{background:var(--c) !important;color:#000 !important}
#boot,#app{width:100%;height:100%}
#boot{position:absolute;inset:0;display:grid;place-items:center;background:var(--bg)}
#start{min-width:180px;min-height:66px;border-radius:999px;font-size:18px;font-weight:700;touch-action:manipulation}
#app{display:none;max-width:980px;margin:0 auto;padding:clamp(12px,3vw,20px);grid-template-rows:auto 1fr auto;gap:var(--gap)}
#top{display:flex;justify-content:space-between;align-items:center;gap:8px}
.top-btn{min-width:92px;padding:0 12px;height:40px;border-radius:999px;font-size:13px;font-weight:700;touch-action:none;flex:1;max-width:140px;}
#settingsBtn{width:40px;height:40px;border-radius:50%;font-size:18px;font-weight:700;touch-action:none;flex:0 0 auto}
#main{display:grid;grid-template-columns:minmax(0,1fr) auto;grid-template-rows:auto auto;grid-template-areas:"drive gear" "drive steer";gap:var(--gap);align-items:end}
#drive{grid-area:drive;display:grid;gap:var(--gap);justify-items:start;place-self:end start;transform:translateY(30px);}
.round{width:var(--b);aspect-ratio:1;border-radius:50%;font-size:30px;line-height:1;touch-action:none}
#gear{grid-area:gear;display:grid;grid-template-columns:repeat(4,var(--g));gap:8px;justify-content:end}
.gear{width:var(--g);height:var(--g);border-radius:50%;font-size:14px;font-weight:700;touch-action:none}
#steer{grid-area:steer;display:grid;grid-template-columns:repeat(2,var(--b));gap:var(--gap);justify-content:center}
#status{justify-self:center;align-self:end;padding:10px 16px;border:1px solid var(--c);border-radius:999px;background:var(--p);color:var(--c);font-size:13px;font-weight:700;letter-spacing:0}
.dim{color:#fff}
.net{margin-left:8px}
.net-off{color:#ff8080}
.net-on{color:var(--c)}
#settingsModal{position:fixed;inset:0;display:none;align-items:center;justify-content:center;padding:16px;background:rgba(0,0,0,.72);backdrop-filter:blur(5px);-webkit-backdrop-filter:blur(5px);z-index:20}
#settingsModal.show{display:flex}
#settingsCard{position:relative;width:min(420px,100%);max-height:90vh;overflow-y:auto;border:1px solid var(--c);border-radius:18px;background:#0c1218;box-shadow:0 0 0 1px rgba(0,255,204,.12),0 18px 50px rgba(0,0,0,.6);padding:18px 16px 16px;display:grid;gap:12px}
#closeSettingsBtn{position:absolute;top:8px;right:8px;width:30px;height:30px;border-radius:50%;font-size:18px;font-weight:800;line-height:1;touch-action:manipulation}
#settingsTitle{font-size:18px;font-weight:800;letter-spacing:.5px;color:var(--c)}
#settingsStatus{min-height:18px;font-size:12px;font-weight:700;color:var(--c)}
#settingsStatus.error{color:#ff8080}
.field{display:grid;gap:6px}
.field label{font-size:13px;font-weight:700;color:var(--c)}
.pwrow{display:grid;grid-template-columns:minmax(0,1fr) 50px;gap:8px;align-items:center}
.field input{width:100%;height:44px;border-radius:12px;border:1px solid rgba(0,255,204,.4);background:#071015;color:#fff;padding:0 12px;font:600 15px/1.1 system-ui,sans-serif;outline:none}
.pw-toggle{width:50px;height:40px;border-radius:8px;font-size:11px;font-weight:700;line-height:1;touch-action:manipulation}
.field input:focus{border-color:var(--c);box-shadow:0 0 0 2px rgba(0,255,204,.18)}
#settingsActions{display:grid;grid-template-columns:repeat(2,1fr);gap:8px;margin-top:4px}
#settingsActions button{min-height:42px;border-radius:12px;font-size:12px;font-weight:800;touch-action:manipulation}
@media (max-width:480px){#settingsActions{grid-template-columns:1fr}}
@media (max-width:480px){:root{--b:74px;--g:42px}}
</style></head><body>
<div id="boot"><button id="start" type="button">GAS MULAI</button></div>
<div id="app">
  <div id="top">
    <button id="settingsBtn" type="button">&#9881;</button>
    <div style="flex:1"></div>
    <button id="lamp" class="top-btn" type="button">LAMPU</button>
  </div>
  <div id="main">
    <div id="drive">
      <button class="round" type="button" data-k="f">^</button>
      <button class="round" type="button" data-k="b">v</button>
    </div>
    <div id="gear">
      <button class="gear" type="button" data-g="1">1</button>
      <button class="gear" type="button" data-g="2">2</button>
      <button class="gear" type="button" data-g="3">3</button>
      <button class="gear" type="button" data-g="4">4</button>
    </div>
    <div id="steer">
      <button class="round" type="button" data-k="l">&lt;</button>
      <button class="round" type="button" data-k="r">&gt;</button>
    </div>
  </div>
  <div id="status">GEAR: <span id="sg">1</span> <span class="dim">|</span> LAMPU: <span id="sl">OFF</span> <span class="dim">|</span> NET: <span id="net" class="net net-off">Disconnected</span></div>
</div>
<div id="settingsModal" aria-hidden="true">
  <div id="settingsCard" role="dialog" aria-modal="true" aria-labelledby="settingsTitle">
    <button id="closeSettingsBtn" type="button" aria-label="Close">&times;</button>
    <div id="settingsTitle">SETTINGS</div>
    <div id="settingsStatus" aria-live="polite"></div>
    <div class="field"><label for="ssidInput">SSID Wi-Fi</label><input id="ssidInput" type="text" maxlength="32" autocomplete="off"></div>
    <div class="field"><button id="openChangeWifiPwdBtn" type="button" style="height:44px;border-radius:12px;font-weight:bold;">GANTI PASSWORD WI-FI</button></div>
    <div class="field"><button id="openChangePwdBtn" type="button" style="height:44px;border-radius:12px;font-weight:bold;">GANTI PASSWORD BALAP</button></div>
    <div id="settingsActions">
      <button id="saveRestartBtn" type="button">RESTART</button>
      <button id="saveSettingsBtn" type="button" style="display:none;">SAVE</button>
      <button id="modeBtn" class="btn-danger" type="button">MODE AMAN</button>
    </div>
  </div>
</div>
<div id="pwdModal" style="position:fixed;inset:0;display:none;align-items:flex-start;justify-content:center;padding-top:15vh;background:rgba(0,0,0,.8);z-index:30">
  <div style="background:#0c1218;border:1px solid var(--c);border-radius:16px;padding:20px;width:280px;display:flex;flex-direction:column;gap:12px">
    <div id="pwdTitle" style="color:var(--c);font-weight:bold;text-align:center">Masukkan Password:</div>
    <div class="pwrow">
      <input id="pwdInput" type="password" maxlength="64" style="height:40px;border-radius:8px;border:1px solid var(--c);background:#000;color:#fff;padding:0 12px;text-align:center;font-size:16px;width:100%">
      <button id="pwdToggleBtn" class="pw-toggle" type="button">SHOW</button>
    </div>
    <div style="display:flex;gap:10px">
      <button id="pwdCancelBtn" style="flex:1;height:40px;border-radius:8px;border:1px solid #ff4d4d;color:#ff4d4d !important;background:transparent">Batal</button>
      <button id="pwdOkBtn" style="flex:1;height:40px;border-radius:8px;background:var(--c);color:#000 !important;border:none;font-weight:bold">GAS</button>
    </div>
  </div>
</div>
<script>
(function(){const KEEP_ALIVE_INTERVAL_MS=250;const WIFI_SYNC_INTERVAL_MS=2000;const VIBRATION_DURATION_MS=40;const START_VIBRATION_MS=30;const BUTTON_VIBRATION_MS=12;var h=document.documentElement,s=document.getElementById('start'),b=document.getElementById('boot'),a=document.getElementById('app'),l=document.getElementById('lamp'),mb=document.getElementById('modeBtn'),settingsBtn=document.getElementById('settingsBtn'),settingsModal=document.getElementById('settingsModal'),settingsCard=document.getElementById('settingsCard'),settingsStatus=document.getElementById('settingsStatus'),ssidInput=document.getElementById('ssidInput'),openChangePwdBtn=document.getElementById('openChangePwdBtn'),openChangeWifiPwdBtn=document.getElementById('openChangeWifiPwdBtn'),saveSettingsBtn=document.getElementById('saveSettingsBtn'),saveRestartBtn=document.getElementById('saveRestartBtn'),closeSettingsBtn=document.getElementById('closeSettingsBtn'),sg=document.getElementById('sg'),sl=document.getElementById('sl'),net=document.getElementById('net'),gb=document.querySelectorAll('[data-g]'),last='',syncTimer=0,keepAliveTimer=0,reqCtrl=null,pulseCtrl=null,syncCtrl=null,st={g:1,l:0,d:'S',s:'C',m:0},held={f:0,b:0,l:0,r:0},btn={f:document.querySelector('[data-k="f"]'),b:document.querySelector('[data-k="b"]'),l:document.querySelector('[data-k="l"]'),r:document.querySelector('[data-k="r"]')};
function vib(n){if(navigator.vibrate)navigator.vibrate(n||VIBRATION_DURATION_MS)}
function fit(){h.style.setProperty('--app-h',((window.visualViewport&&window.visualViewport.height)||window.innerHeight)+'px')}
function setNet(v){net.textContent=v?'Connected':'Disconnected';net.className='net '+(v?'net-on':'net-off')}
function paint(){sg.textContent=st.g;sl.textContent=st.l?'ON':'off';l.className='top-btn '+(st.l?'on':'');mb.textContent=st.m?' status: BALAP':' status: AMAN';mb.className='btn-danger '+(st.m?'on':'');if(st.m){document.body.classList.add('race-mode')}else{document.body.classList.remove('race-mode')}for(var i=0;i<gb.length;i++)gb[i].className='gear'+(i+1===st.g?' on':'');btn.f.className='round'+(st.d==='F'?' on':'');btn.b.className='round'+(st.d==='B'?' on':'');btn.l.className='round'+(st.s==='L'?' on':'');btn.r.className='round'+(st.s==='R'?' on':'')}
function q(){return'g='+st.g+'&l='+st.l+'&d='+st.d+'&s='+st.s+'&m='+st.m}
function abortReq(c){if(c)try{c.abort()}catch(e){}}
function send(url,kind){var c=new AbortController();var tm=setTimeout(function(){setNet(0);abortReq(c)},1200);if(kind===0){abortReq(reqCtrl);abortReq(syncCtrl);abortReq(pulseCtrl);reqCtrl=c}else if(kind===1){abortReq(syncCtrl);syncCtrl=c}else{abortReq(pulseCtrl);pulseCtrl=c}return fetch(url,{cache:'no-store',signal:c.signal}).then(function(r){clearTimeout(tm);setNet(1);if(kind===0&&reqCtrl===c)reqCtrl=null;if(kind===1&&syncCtrl===c)syncCtrl=null;if(kind===2&&pulseCtrl===c)pulseCtrl=null;return r},function(e){clearTimeout(tm);if(!e||e.name!=='AbortError')setNet(0);if(kind===0&&reqCtrl===c)reqCtrl=null;if(kind===1&&syncCtrl===c)syncCtrl=null;if(kind===2&&pulseCtrl===c)pulseCtrl=null;throw 0})}
var w=0;function push(){if(w)return;var x=q();if(x===last)return;last=x;w=1;send('/state?'+x,0).then(function(){w=0;if(q()!==last)push()},function(){w=0;if(q()!==last)push()})}
function pulse(){send('/state?'+q()+'&hb='+Date.now(),2).catch(function(){})}
function drive(){return(held.f&&held.b)?'S':held.f?'F':held.b?'B':'S'}
function steer(){return(held.l&&held.r)?'C':held.l?'L':held.r?'R':'C'}
function moving(){return st.d!=='S'||st.s!=='C'}
function updateMotion(){var d=drive(),s=steer();if(d!==st.d||s!==st.s){st.d=d;st.s=s;paint();push();if(moving())startKeepAlive();else stopKeepAlive();}}
function setGear(g){g|=0;if(g<1||g>4||g===st.g)return;st.g=g;paint();push();if(moving())startKeepAlive();vib(BUTTON_VIBRATION_MS)}
function setLamp(v){v=v?1:0;if(v===st.l)return;st.l=v;paint();push();if(moving())startKeepAlive();vib(BUTTON_VIBRATION_MS)}
function stateFromText(t){var p=(t||'').trim().split(',');if(p.length<5)return;var active=held.f||held.b||held.l||held.r;st.g=(p[0]|0)||1;st.l=p[1]==='1'?1:0;if(!active){st.d=p[2]||'S';st.s=p[3]||'C'}st.m=p[4]==='1'?1:0;paint();setNet(1)}
function sync(){if(moving())return;send('/state',1).then(function(r){return r.text()}).then(stateFromText).catch(function(){setNet(0)})}
function syncLoop(){clearTimeout(syncTimer);syncTimer=setTimeout(syncLoop,net.className.indexOf('net-off')>-1?500:WIFI_SYNC_INTERVAL_MS);sync()}
function fs(){var e=document.documentElement,r=e.requestFullscreen||e.webkitRequestFullscreen||e.mozRequestFullScreen||e.msRequestFullscreen;if(!r)return Promise.resolve();try{return Promise.resolve(r.call(e,{navigationUI:'hide'}))}catch(x){try{return Promise.resolve(r.call(e))}catch(y){return Promise.resolve()}}}
function land(){if(screen.orientation&&screen.orientation.lock)screen.orientation.lock('landscape').catch(function(){})}
function start(){vib(START_VIBRATION_MS);b.style.display='none';a.style.display='grid';fit();fs().then(land).catch(land);syncLoop()}
function sendStop(){stopKeepAlive();st.d='S';st.s='C';held.f=held.b=held.l=held.r=0;paint();push()}
function startKeepAlive(){if(keepAliveTimer||!moving())return;keepAliveTimer=setInterval(function(){if(moving())pulse();else stopKeepAlive()},KEEP_ALIVE_INTERVAL_MS)}
function stopKeepAlive(){if(!keepAliveTimer)return;clearInterval(keepAliveTimer);keepAliveTimer=0}
s.addEventListener('pointerdown',start);
function setSettingsBusy(v){ssidInput.disabled=v;openChangePwdBtn.disabled=v;openChangeWifiPwdBtn.disabled=v;saveSettingsBtn.disabled=v;saveRestartBtn.disabled=v}
function setToggleState(btn,input){btn.textContent=input.type==='password'?'SHOW':'HIDE'}
function togglePassword(input,btn){if(input.type==='password'){input.type='text'}else{input.type='password'}setToggleState(btn,input)}
function setStatus(text,isError){settingsStatus.textContent=text;settingsStatus.className=''+(isError?'error':'')}
function setActionState(btn,html,baseText,ok){btn.innerHTML=html;if(btn._tm){clearTimeout(btn._tm)}if(typeof baseText==='string'){btn._tm=setTimeout(function(){btn.textContent=baseText;btn.disabled=false},ok?2400:2600)}}
function postForm(url,body){return fetch(url,{method:'POST',cache:'no-store',headers:{'Content-Type':'application/x-www-form-urlencoded;charset=UTF-8'},body:body}).then(function(r){return r.json()})}
function openSettings(){settingsModal.classList.add('show');settingsModal.setAttribute('aria-hidden','false');setStatus('Loading...');setSettingsBusy(true);fetch('/settings',{cache:'no-store'}).then(function(r){return r.json()}).then(function(data){ssidInput.value=data.ssid||'';setStatus('');setSettingsBusy(false)}).catch(function(){setStatus('Unable to load settings',true);setSettingsBusy(false)})}
function closeSettings(){settingsModal.classList.remove('show');settingsModal.setAttribute('aria-hidden','true');setStatus('');setSettingsBusy(false)}
function settingsBody(){return'ssid='+encodeURIComponent(ssidInput.value)}
function saveSettings(restart){setSettingsBusy(true);setStatus('');var baseText=restart?'SAVE & RESTART':'SAVE';var btn=restart?saveRestartBtn:saveSettingsBtn;btn.disabled=true;setActionState(btn,'Saving...',baseText,false);postForm('/settings',settingsBody()).then(function(data){if(data&&data.success){setStatus('');setActionState(btn,'&#10003; Saved',baseText,true);if(restart){return postForm('/restart','').then(function(d){if(d&&d.success){closeSettings()}else{setActionState(btn,'&#10005; Save Failed',baseText,false);setSettingsBusy(false)}}).catch(function(){setActionState(btn,'&#10005; Save Failed',baseText,false);setSettingsBusy(false)})}setSettingsBusy(false);return}setActionState(btn,'&#10005; Save Failed',baseText,false);setSettingsBusy(false)}).catch(function(){setActionState(btn,'&#10005; Save Failed',baseText,false);setSettingsBusy(false)})}
settingsBtn.addEventListener('click',openSettings);
closeSettingsBtn.addEventListener('click',closeSettings);
saveSettingsBtn.addEventListener('click',function(){saveSettings(false)});
saveRestartBtn.addEventListener('click',function(){saveSettings(true)});
window.addEventListener('keydown',function(e){if(e.key==='Escape'&&settingsModal.classList.contains('show'))closeSettings()});
function bindTap(el,fn){el.addEventListener('pointerdown',function(e){e.preventDefault();try{el.setPointerCapture(e.pointerId)}catch(z){}fn(e)});el.addEventListener('pointerup',function(e){if(e&&e.pointerId!=null)try{el.releasePointerCapture(e.pointerId)}catch(z){}});el.addEventListener('pointercancel',function(e){if(e&&e.pointerId!=null)try{el.releasePointerCapture(e.pointerId)}catch(z){}});el.addEventListener('pointerleave',function(e){if(e&&e.pointerId!=null)try{el.releasePointerCapture(e.pointerId)}catch(z){}})}
bindTap(l,function(){setLamp(!st.l)});
var pm=document.getElementById('pwdModal'),pi=document.getElementById('pwdInput'),pc=document.getElementById('pwdCancelBtn'),po=document.getElementById('pwdOkBtn'),pt=document.getElementById('pwdTitle'),ptog=document.getElementById('pwdToggleBtn'),pwdMode=0;
ptog.addEventListener('click',function(){togglePassword(pi,ptog)});
function openPwdModal(mode){pwdMode=mode;pi.value='';pi.placeholder='';pi.type='password';setToggleState(ptog,pi);if(mode===1){pt.textContent='Masukkan Password Balap LAMA:';po.textContent='LANJUT';settingsModal.classList.remove('show')}else if(mode===2){pt.textContent='Masukkan Password Wi-Fi LAMA:';po.textContent='LANJUT';settingsModal.classList.remove('show')}else if(mode===3){pt.textContent='Masukkan Password Balap BARU:';po.textContent='SIMPAN'}else if(mode===4){pt.textContent='Masukkan Password Wi-Fi BARU:';po.textContent='SIMPAN'}else{pt.textContent='Masukkan Password:';po.textContent='GAS'}pm.style.display='flex';pi.focus()}
openChangePwdBtn.addEventListener('click',function(){openPwdModal(1)});
openChangeWifiPwdBtn.addEventListener('click',function(){openPwdModal(2)});
mb.addEventListener('click',function(){if(st.m){postForm('/race','off=1').then(function(){st.m=0;paint();vib(BUTTON_VIBRATION_MS);});}else{openPwdModal(0)}});
pc.addEventListener('click',function(){pm.style.display='none';if(pwdMode!==0){settingsModal.classList.add('show')}});
po.addEventListener('click',function(){if(pwdMode===0){postForm('/race','password='+encodeURIComponent(pi.value)).then(function(data){if(data&&data.success){pm.style.display='none';st.m=1;paint();push();vib(BUTTON_VIBRATION_MS)}else{pi.value='';pi.placeholder='Salah!';vib(80)}}).catch(function(){pi.value='';pi.placeholder='Error!';vib(80)})}else if(pwdMode===1){po.disabled=true;po.textContent='...';postForm('/race','password='+encodeURIComponent(pi.value)).then(function(data){po.disabled=false;if(data&&data.success){window.tempOldPwd=pi.value;openPwdModal(3)}else{pi.value='';pi.placeholder='Pass Lama Salah!';vib(80);po.textContent='LANJUT'}}).catch(function(){po.disabled=false;pi.value='';pi.placeholder='Error!';vib(80);po.textContent='LANJUT'})}else if(pwdMode===2){po.disabled=true;po.textContent='...';postForm('/verifywifi','password='+encodeURIComponent(pi.value)).then(function(data){po.disabled=false;if(data&&data.success){window.tempOldPwd=pi.value;openPwdModal(4)}else{pi.value='';pi.placeholder='Pass Lama Salah!';vib(80);po.textContent='LANJUT'}}).catch(function(){po.disabled=false;pi.value='';pi.placeholder='Error!';vib(80);po.textContent='LANJUT'})}else if(pwdMode===3||pwdMode===4){po.disabled=true;po.textContent='...';var url=pwdMode===3?'/changerace':'/changewifi';postForm(url,'oldpwd='+encodeURIComponent(window.tempOldPwd||'')+'&newpwd='+encodeURIComponent(pi.value)).then(function(d){po.disabled=false;if(d&&d.success){pm.style.display='none';vib(BUTTON_VIBRATION_MS);if(pwdMode===4){postForm('/restart','').then(function(){settingsModal.classList.add('show');setStatus('Merestart Wemos...',false)})}else{settingsModal.classList.add('show');setStatus('Password Balap diubah!',false)}}else{pi.value='';pi.placeholder='Gagal/Kependekan!';vib(80);po.textContent='SIMPAN'}}).catch(function(){po.disabled=false;pi.value='';pi.placeholder='Error!';vib(80);po.textContent='SIMPAN'})}});
for(var gs=document.querySelectorAll('[data-g]'),i=0;i<gs.length;i++){(function(x){bindTap(x,function(){setGear(x.getAttribute('data-g')|0)})})(gs[i])}
for(var ks=document.querySelectorAll('[data-k]'),j=0;j<ks.length;j++){(function(x){var k=x.getAttribute('data-k');x.addEventListener('pointerdown',function(e){e.preventDefault();try{x.setPointerCapture(e.pointerId)}catch(z){}held[k]=1;updateMotion();vib(BUTTON_VIBRATION_MS)});var end=function(e){if(e&&e.pointerId!=null)try{x.releasePointerCapture(e.pointerId)}catch(z){}held[k]=0;updateMotion()};x.addEventListener('pointerup',end);x.addEventListener('pointercancel',end)})(ks[j])}
document.addEventListener('pointerdown',function(){if(!document.fullscreenElement&&!document.webkitFullscreenElement){fs().then(land).catch(land)}});
window.addEventListener('resize',fit);if(window.visualViewport)window.visualViewport.addEventListener('resize',fit);
window.addEventListener('orientationchange',fit);document.addEventListener('fullscreenchange',fit);
window.addEventListener('pagehide',function(){stopKeepAlive();sendStop()});window.addEventListener('beforeunload',function(){stopKeepAlive();sendStop()});document.addEventListener('visibilitychange',function(){if(document.visibilityState==='hidden'){stopKeepAlive();sendStop()}});
fit();paint();setNet(0);syncLoop()})();
</script></body></html>
)rawliteral";

//======================================================[⚙️]======================================================
// MOTOR FUNCTIONS
//================================================================================================================

const uint16_t gearPwmTableRace[] PROGMEM = { GEAR1_RACE, GEAR2_RACE, GEAR3_RACE, GEAR4_RACE };
const uint16_t gearPwmTableSafe[] PROGMEM = { GEAR1_SAFE, GEAR2_SAFE, GEAR3_SAFE, GEAR4_SAFE };

static inline uint16_t gearPwm(uint8_t gear) {
  if (gear < 1 || gear > 4) {
    return isRaceMode ? GEAR1_RACE : GEAR1_SAFE;
  }
  if (isRaceMode) {
    return pgm_read_word_near(gearPwmTableRace + (gear - 1));
  } else {
    return pgm_read_word_near(gearPwmTableSafe + (gear - 1));
  }
}

static inline char driveChar(uint8_t drive) {
  switch (drive) {
    case DRIVE_FWD: return 'F';
    case DRIVE_REV: return 'B';
    default:        return 'S';
  }
}

static inline char steerChar(uint8_t steer) {
  switch (steer) {
    case STEER_LEFT:  return 'L';
    case STEER_RIGHT: return 'R';
    default:          return 'C';
  }
}

static inline void motorForward(uint8_t pinA, uint8_t pinB, uint16_t pwm) {
  analogWrite(pinB, 0);
  digitalWrite(pinB, LOW);
  
  if (pwm >= 1023) {
    analogWrite(pinA, 0);
    pinMode(pinA, OUTPUT);
    digitalWrite(pinA, HIGH);
  } else {
    analogWrite(pinA, pwm);
  }
}

static inline void motorReverse(uint8_t pinA, uint8_t pinB, uint16_t pwm) {
  analogWrite(pinA, 0);
  digitalWrite(pinA, LOW);
  
  if (pwm >= 1023) {
    analogWrite(pinB, 0);
    pinMode(pinB, OUTPUT);
    digitalWrite(pinB, HIGH);
  } else {
    analogWrite(pinB, pwm);
  }
}

static inline void motorStop(uint8_t pinA, uint8_t pinB) {
  analogWrite(pinA, 0);
  analogWrite(pinB, 0);
  digitalWrite(pinA, LOW);
  digitalWrite(pinB, LOW);
}

static inline void touchControl() {
  lastControlMs = millis();
  runtimeFlags &= (uint8_t)~FLAG_FAILSAFE;
}

static inline bool motionActive() {
  return car.drive != DRIVE_STOP || car.steer != STEER_CENTER;
}

static inline bool driveActive() {
  return car.drive != DRIVE_STOP;
}

static inline bool failSafeActive() {
  return (runtimeFlags & FLAG_FAILSAFE) != 0;
}

static inline const char* driveName(uint8_t drive) {
  switch (drive) {
    case DRIVE_FWD: return "forward";
    case DRIVE_REV: return "reverse";
    default:        return "stop";
  }
}

static inline const char* steerName(uint8_t steer) {
  switch (steer) {
    case STEER_LEFT:  return "left";
    case STEER_RIGHT: return "right";
    default:          return "center";
  }
}

void applyDrive() {
  static uint8_t lastActiveDirection = DRIVE_STOP;
  static unsigned long lastActiveTimeMs = 0;
  
  uint16_t currentPWM = gearPwm(car.gear);
  unsigned long now = millis();

  if (isBraking) {
    isBraking = false;
  }

  bool stillHasForwardMomentum = (lastActiveDirection == DRIVE_FWD) && (now - lastActiveTimeMs < 600);
  bool stillHasReverseMomentum = (lastActiveDirection == DRIVE_REV) && (now - lastActiveTimeMs < 600);

  if ((stillHasForwardMomentum && car.drive == DRIVE_REV) || 
      (stillHasReverseMomentum && car.drive == DRIVE_FWD)) {
    
    motorStop(pinDriveA, pinDriveB);
    isBraking = true;                
    brakeTimeMs = millis();          
    pendingDriveDir = car.drive;     
    lastActiveDirection = DRIVE_STOP; 
    car.drive = DRIVE_STOP;          
    return; 
  }

  if (car.drive == DRIVE_FWD) {
    motorForward(pinDriveA, pinDriveB, currentPWM);
    lastActiveDirection = DRIVE_FWD;
    lastActiveTimeMs = now;
  } 
  else if (car.drive == DRIVE_REV) {
    uint16_t reversePWM;
    if (currentPWM >= 1023) {
      reversePWM = 1023;
    } else {
      int16_t tempPWM = currentPWM - 100;
      reversePWM = (tempPWM < 400) ? 400 : tempPWM;
    }
    motorReverse(pinDriveA, pinDriveB, reversePWM);
    lastActiveDirection = DRIVE_REV;
    lastActiveTimeMs = now;
  }
  else {
    motorStop(pinDriveA, pinDriveB);
    lastActiveDirection = DRIVE_STOP;
  }
}

void applySteer() {
  switch (car.steer) {
    case STEER_LEFT:  
      steeringServo.write(SERVO_LEFT); 
      break;
    case STEER_RIGHT: 
      steeringServo.write(SERVO_RIGHT); 
      break;
    default:          
      steeringServo.write(SERVO_CENTER); 
      break;
  }
}

void applyLed() {
  digitalWrite(pinLED, car.led ? HIGH : LOW);
}

void applyOutputs() {
  applySteer();
  applyDrive();
  applyLed();
}

void stopMotion() {
  if (car.drive != DRIVE_STOP || car.steer != STEER_CENTER) {
      Serial.println(F("[MOTION] Berhenti (Tombol Dilepas / Failsafe)"));
  }
  
  isBraking = false;
  pendingDriveDir = DRIVE_STOP;
  
  car.drive = DRIVE_STOP;
  car.steer = STEER_CENTER;
  applyDrive();
  applySteer();
}

void logBootBanner() {
  Serial.println();
  Serial.println(F("=============================="));
  Serial.println(F(FW_NAME " Firmware"));
  Serial.print(F("Version : "));
  Serial.println(FW_VERSION);
  Serial.print(F("Author  : "));
  Serial.println(FW_AUTHOR);
  Serial.print(F("Build   : "));
  Serial.print(FW_BUILD_DATE);
  Serial.print(' ');
  Serial.println(FW_BUILD_TIME);
  Serial.print(F("Chip    : "));
  Serial.println(F("ESP8266"));
  Serial.print(F("Flash   : "));
  Serial.print(ESP.getFlashChipSize() / 1048576);
  Serial.println(F(" MB"));
  Serial.print(F("Heap    : "));
  Serial.print(ESP.getFreeHeap());
  Serial.println(F(" Bytes"));
}

void logGear(uint8_t gear) {
  Serial.print(F("[GEAR] "));
  Serial.println(gear);
}

void logLamp(bool on) {
  Serial.print(F("[LED] "));
  Serial.println(on ? F("ON") : F("OFF"));
}

void logFailSafe() {
  Serial.println(F("[FAILSAFE] Motor Stopped"));
}

void logStationCount(uint8_t currentCount) {
  if (lastStationCount == currentCount) {
    return;
  }
  if (lastStationCount != 255) {
    if (currentCount > lastStationCount) {
      Serial.println(F("[WiFi] Client Connected"));
    } else if (currentCount < lastStationCount) {
      Serial.println(F("[WiFi] Client Disconnected"));
    }
  }
  lastStationCount = currentCount;
}

static inline void copyConfigValue(char* dest, size_t destSize, const char* src) {
  if (destSize == 0) {
    return;
  }
  strncpy(dest, src, destSize - 1);
  dest[destSize - 1] = '\0';
}

static void setConfigDefaults() {
  copyConfigValue(config.ssid, sizeof(config.ssid), WIFI_SSID);
  copyConfigValue(config.password, sizeof(config.password), WIFI_PASSWORD);
  copyConfigValue(config.racePassword, sizeof(config.racePassword), RACE_PASSWORD);
}

static void logConfig() {
  Serial.println(F("[CONFIG]"));
  Serial.print(F("SSID : "));
  Serial.println(config.ssid);
  Serial.print(F("PASSWORD : "));
  Serial.println(F("********"));
  Serial.print(F("RACE PASSWORD : "));
  Serial.println(F("********"));
  Serial.print(F("OTA PASSWORD : "));
  Serial.println(F("********"));
}

bool createDefaultConfig() {
  File file = LittleFS.open(CONFIG_FILE, "w");
  if (!file) {
    Serial.println(F("[FS] Gagal membuat /config.ini"));
    return false;
  }
  
  file.print(F("SSID="));
  file.println(WIFI_SSID);
  file.print(F("PASSWORD="));
  file.println(WIFI_PASSWORD);
  file.print(F("RACE_PASSWORD="));
  file.println(RACE_PASSWORD);
  file.close();

  setConfigDefaults();
  Serial.println(F("[FS] /config.ini default berhasil dibuat"));
  logConfig();
  return true;
}

bool saveConfig() {
  steeringServo.detach();
  const char* TEMP_FILE = "/config.tmp";
  File file = LittleFS.open(TEMP_FILE, "w");
  if (!file) {
    steeringServo.attach(pinServo);
    applySteer();
    return false;
  }

  file.print(F("SSID="));
  file.println(config.ssid);
  file.print(F("PASSWORD="));
  file.println(config.password);
  file.print(F("RACE_PASSWORD="));
  file.println(config.racePassword);
  file.close();

  bool success = LittleFS.rename(TEMP_FILE, CONFIG_FILE);
  steeringServo.attach(pinServo);
  applySteer();
  return success;
}

static void trimLineEnd(char* line) {
  size_t len = strlen(line);
  while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n' || line[len - 1] == ' ' || line[len - 1] == '\t')) {
    line[--len] = '\0';
  }
}

static void loadConfigLine(const char* line) {
  if (strncmp(line, "SSID=", 5) == 0) {
    copyConfigValue(config.ssid, sizeof(config.ssid), line + 5);
  } else if (strncmp(line, "PASSWORD=", 9) == 0) {
    copyConfigValue(config.password, sizeof(config.password), line + 9);
  } else if (strncmp(line, "RACE_PASSWORD=", 14) == 0) {
      copyConfigValue(
          config.racePassword,
          sizeof(config.racePassword),
          line + 14
      );
  }
}

bool loadConfig() {
  if (!LittleFS.exists(CONFIG_FILE)) {
    Serial.println(F("[FS] /config.ini belum ada, membuat default"));
    return createDefaultConfig();
  }

  setConfigDefaults();

  File file = LittleFS.open(CONFIG_FILE, "r");
  if (!file) {
    Serial.println(F("[FS] Gagal membuka /config.ini"));
    return false;
  }

  char line[128];
  while (file.available()) {
    size_t len = file.readBytesUntil('\n', line, sizeof(line) - 1);
    line[len] = '\0';
    trimLineEnd(line);
    if (line[0] != '\0') {
      loadConfigLine(line);
    }
  }
  file.close();

  if (strlen(config.racePassword) < 6) {
    copyConfigValue(config.racePassword, sizeof(config.racePassword), RACE_PASSWORD);
  }
  if (strlen(config.password) < 8) {
    copyConfigValue(config.password, sizeof(config.password), WIFI_PASSWORD);
  }

  logConfig();
  return true;
}

bool initStorage() {
  bool mounted = false;
  
  for (int i = 0; i < 3; i++) {
    if (LittleFS.begin()) {
      mounted = true;
      break;
    }
    delay(100);
  }

  if (!mounted) {
    Serial.println(F("[FS] Memori error/kosong, melakukan format..."));
    ESP.wdtFeed();
    LittleFS.format(); 
    ESP.wdtFeed();
    
    if (!LittleFS.begin()) {
      Serial.println(F("[FS] FATAL: Format gagal!"));
      return false;
    }
  }

  if (!loadConfig()) {
    return false;
  }

  return true;
}

void initOTA() {
  ArduinoOTA.setHostname("ROZON_RC");
  ArduinoOTA.setPassword(HARDCODED_OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    Serial.println(F("[OTA] Start"));
    isOTA = true;
    stopMotion();
    steeringServo.detach();
  });

  ArduinoOTA.onEnd([]() {
    Serial.println(F("[OTA] End"));
    isOTA = false;
    steeringServo.attach(pinServo);
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    const unsigned int percent = total ? (progress * 100U) / total : 0;
    Serial.print(F("[OTA] Progress "));
    Serial.print(percent);
    Serial.println('%');
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.print(F("[OTA] Error "));
    Serial.println((int)error);
    isOTA = false;
    stopMotion();
    steeringServo.attach(pinServo);
  });

  ArduinoOTA.begin();
  Serial.println(F("[OTA] Ready"));
}

static void escapeJson(const char* src, char* dest, size_t destSize) {
  size_t j = 0;
  for (size_t i = 0; src[i] != '\0' && j < destSize - 1; i++) {
    if (src[i] == '"' || src[i] == '\\') {
      if (j < destSize - 2) {
        dest[j++] = '\\';
        dest[j++] = src[i];
      }
    } else {
      dest[j++] = src[i];
    }
  }
  dest[j] = '\0';
}

void sendInfoJson() {
  char buf[196];
  const uint32_t flashMb = ESP.getFlashChipSize() / 1048576UL;
  snprintf(buf, sizeof(buf),
           "{\"name\":\"%s\",\"version\":\"%s\",\"build_date\":\"%s\",\"build_time\":\"%s\",\"chip\":\"ESP8266\",\"flash\":\"%luMB\"}",
           FW_NAME, FW_VERSION, FW_BUILD_DATE, FW_BUILD_TIME, (unsigned long)flashMb);
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("Connection", "close"); 
  server.send(200, "application/json", buf);
}

void sendStatusJson() {
  char buf[220];
  const bool fsActive = failSafeActive();
  snprintf(buf, sizeof(buf),
           "{\"gear\":%u,\"drive\":\"%s\",\"steer\":\"%s\",\"lamp\":%s,\"failsafe\":%s,\"heap\":%u,\"uptime\":%lu}",
           car.gear,
           driveName(car.drive),
           steerName(car.steer),
           car.led ? "true" : "false",
           fsActive ? "true" : "false",
           ESP.getFreeHeap(),
           (unsigned long)millis());
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("Connection", "close");
  server.send(200, "application/json", buf);
}

void sendStateText() {
  char buf[20];
  snprintf(buf, sizeof(buf), "%u,%u,%c,%c,%u", car.gear, car.led, driveChar(car.drive), steerChar(car.steer), isRaceMode ? 1 : 0);
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", buf);
}

void setDriveMode(uint8_t mode) {
  car.drive = mode;
  applyDrive();
}

void setSteerMode(uint8_t mode) {
  car.steer = mode;
  applySteer();
}

void setGearValue(uint8_t gear) {
  if (gear < 1 || gear > 4) {
    return;
  }
  const unsigned long now = millis();
  if (gear == car.gear) {
    if ((now - lastGearPressMs) < GEAR_DEBOUNCE_MS) {
      return;
    }
    lastGearPressMs = now;
    return;
  }
  lastGearPressMs = now;
  car.gear = gear;
  logGear(gear);
  if (driveActive()) {
    applyDrive();
  }
}

void setLampValue(uint8_t on) {
  const uint8_t next = on ? 1 : 0;
  if (next == car.led) {
    return;
  }
  car.led = next;
  logLamp(car.led != 0);
  applyLed();
}

void setDriveFromChar(char d) {
  uint8_t prev = car.drive;
  if (d == 'F' || d == 'f') {
    car.drive = DRIVE_FWD;
  } else if (d == 'B' || d == 'b') {
    car.drive = DRIVE_REV;
  } else {
    car.drive = DRIVE_STOP;
  }
  
  if (car.drive != prev) {
    if (isBraking) {
      isBraking = false;
      pendingDriveDir = DRIVE_STOP;
    }
    Serial.print(F("[DRIVE] "));
    Serial.println(driveName(car.drive));
  }
}

void setSteerFromChar(char s) {
  uint8_t prev = car.steer;
  if (s == 'L' || s == 'l') {
    car.steer = STEER_LEFT;
  } else if (s == 'R' || s == 'r') {
    car.steer = STEER_RIGHT;
  } else {
    car.steer = STEER_CENTER;
  }
  if (car.steer != prev) {
    Serial.print(F("[STEER] "));
    Serial.println(steerName(car.steer));
  }
}

//======================================================[🌐]======================================================
// WEB HANDLERS
//================================================================================================================

void updateFromStateEndpoint() {
  if (isOTA) return;

  IPAddress clientIP = server.client().remoteIP();
  bool driverExpired = (millis() - lastControlMs > FAILSAFE_TIMEOUT_MS);
  
  if (activeDriverIP == IPAddress(0, 0, 0, 0) || driverExpired) {
    activeDriverIP = clientIP;
  } else if (activeDriverIP != clientIP) {
    return;
  }

  bool validControl = false;
  bool gearDirty = false;
  bool driveDirty = false;
  bool steerDirty = false;
  bool ledDirty = false;
  char argBuf[8];

  if (server.hasArg("g")) {
    server.arg("g").toCharArray(argBuf, sizeof(argBuf));
    const char g = argBuf[0];
    if (g >= '1' && g <= '4') {
      const uint8_t nextGear = (uint8_t)(g - '0');
      if (nextGear != car.gear) {
        setGearValue(nextGear);
        gearDirty = true;
      }
      validControl = true;
    }
  }

  if (server.hasArg("l")) {
    server.arg("l").toCharArray(argBuf, sizeof(argBuf));
    if (argBuf[0] != '\0') {
      const char c = argBuf[0];
      const uint8_t nextLed = (c == '1' || c == 'O' || c == 'o' || c == 'Y' || c == 'y') ? 1 : 0;
      if (nextLed != car.led) {
        setLampValue(nextLed);
        ledDirty = true;
      }
      validControl = true;
    }
  }

  if (server.hasArg("d")) {
    server.arg("d").toCharArray(argBuf, sizeof(argBuf));
    const char d = argBuf[0];
    if (d == 'F' || d == 'f' || d == 'B' || d == 'b' || d == 'S' || d == 's') {
      uint8_t oldDrive = car.drive;
      setDriveFromChar(d);
      if (car.drive != oldDrive) driveDirty = true;
      validControl = true;
    }
  }

  if (server.hasArg("s")) {
    server.arg("s").toCharArray(argBuf, sizeof(argBuf));
    const char s = argBuf[0];
    if (s == 'L' || s == 'l' || s == 'R' || s == 'r' || s == 'C' || s == 'c') {
      uint8_t oldSteer = car.steer;
      setSteerFromChar(s);
      if (car.steer != oldSteer) steerDirty = true;
      validControl = true;
    }
  }

  bool isHeartbeat = server.hasArg("hb");
  if (isHeartbeat || driveDirty || steerDirty) {
    touchControl(); 
}
  if (steerDirty) applySteer();
  if (driveDirty || (gearDirty && driveActive())) applyDrive();
  if (ledDirty) applyLed();
}

void handleRoot() {
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html", htmlPage);
}

void handleInfo() {
  sendInfoJson();
}

void handleStatus() {
  sendStatusJson();
}

void handleSettings() {
  char escapedSsid[65];
  escapeJson(config.ssid, escapedSsid, sizeof(escapedSsid));
  
  char buf[128];
  snprintf(buf, sizeof(buf), "{\"ssid\":\"%s\"}", escapedSsid);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", buf);
}

void handleSaveSettings() {
  if (!server.hasArg("ssid")) {
    server.sendHeader("Cache-Control", "no-store");
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Missing arguments\"}");
    return;
  }

  Config configBackup = config;
  
  if (server.arg("ssid").length() < 1 || server.arg("ssid").length() > 32) {
    server.sendHeader("Cache-Control", "no-store");
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid Input Length\"}");
    return;
  }

  server.arg("ssid").toCharArray(config.ssid, sizeof(config.ssid));

  if (!saveConfig()) {
    config = configBackup;
    server.sendHeader("Cache-Control", "no-store");
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Save Failed\"}");
    return;
  }

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", "{\"success\":true}");
}

void handleChangeRace() {
  if (!server.hasArg("oldpwd") || !server.hasArg("newpwd")) {
    server.send(400, "application/json", "{\"success\":false}");
    return;
  }
  
  if (server.arg("oldpwd") != config.racePassword) {
    server.send(401, "application/json", "{\"success\":false}");
    return;
  }
  
  if (server.arg("newpwd").length() < 6 || server.arg("newpwd").length() > 32) {
    server.send(400, "application/json", "{\"success\":false}");
    return;
  }

  char backupPwd[33];
  strcpy(backupPwd, config.racePassword);
  
  server.arg("newpwd").toCharArray(config.racePassword, sizeof(config.racePassword));
  
  if (!saveConfig()) {
    strcpy(config.racePassword, backupPwd);
    server.send(500, "application/json", "{\"success\":false}");
    return;
  }
  
  server.send(200, "application/json", "{\"success\":true}");
}

void handleRace() {
  server.sendHeader("Cache-Control", "no-store");

  IPAddress clientIP = server.client().remoteIP();
  if (activeDriverIP != IPAddress(0, 0, 0, 0) && activeDriverIP != clientIP) {
    server.send(403, "application/json", "{\"success\":false}");
    return;
  }

  if (server.hasArg("off")) {
    isRaceMode = false;
    
    if (driveActive()) {
      applyDrive();
    }
    
    server.send(200, "application/json", "{\"success\":true}");
    return;
  }

  if (!server.hasArg("password")) {
    server.send(400, "application/json", "{\"success\":false}");
    return;
  }

  if (strcmp(server.arg("password").c_str(), config.racePassword) == 0) {
    isRaceMode = true;
    if (driveActive()) applyDrive();
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(200, "application/json", "{\"success\":false}");
  }
}

void handleVerifyWifi() {
  if (!server.hasArg("password")) {
    server.send(400, "application/json", "{\"success\":false}");
    return;
  }
  if (server.arg("password") == config.password) {
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(200, "application/json", "{\"success\":false}");
  }
}

void handleChangeWifi() {
  if (!server.hasArg("oldpwd") || !server.hasArg("newpwd")) {
    server.send(400, "application/json", "{\"success\":false}");
    return;
  }
  
  if (server.arg("oldpwd") != config.password) {
    server.send(401, "application/json", "{\"success\":false}");
    return;
  }
  
  if (server.arg("newpwd").length() < 8 || server.arg("newpwd").length() > 63) {
    server.send(400, "application/json", "{\"success\":false}");
    return;
  }

  char backupPwd[65];
  strcpy(backupPwd, config.password);
  
  server.arg("newpwd").toCharArray(config.password, sizeof(config.password));
  
  if (!saveConfig()) {
    strcpy(config.password, backupPwd);
    server.send(500, "application/json", "{\"success\":false}");
    return;
  }
  
  server.send(200, "application/json", "{\"success\":true}");
}

void handleRestart() {
  Serial.println(F("[SYSTEM] Restart Requested"));
  stopMotion();
  
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", "{\"success\":true}");
  
  pendingRestart = true;
  restartTimeMs = millis();
}

void handleState() {
  if (server.args() > 0) {
    updateFromStateEndpoint();
  }
  sendStateText();
}

void checkFailSafe() {
  if (motionActive() && (millis() - lastControlMs > FAILSAFE_TIMEOUT_MS)) {
    if (!failSafeActive()) {
      runtimeFlags |= FLAG_FAILSAFE;
      logFailSafe();
    }
    stopMotion();
  }
}

//======================================================[♾️]======================================================
// SETUP & LOOP
//================================================================================================================

void setup() {
  Serial.begin(115200);
  delay(50);

  logBootBanner();

  if (!initStorage()) {
    Serial.println(F("[SYSTEM] Warning: Gagal inisialisasi FS"));
  }

  pinMode(pinDriveA, OUTPUT);
  pinMode(pinDriveB, OUTPUT);
  pinMode(pinLED, OUTPUT);

  pinMode(D4, OUTPUT);
  digitalWrite(D4, LOW);

  digitalWrite(pinDriveA, LOW);
  digitalWrite(pinDriveB, LOW);
  digitalWrite(pinLED, LOW);

  steeringServo.write(SERVO_CENTER);
  steeringServo.attach(pinServo);
  
  analogWriteRange(1023);
  analogWriteFreq(1000);

  applyOutputs();

  WiFi.mode(WIFI_AP);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setOutputPower(15.0);
  const char* apSsid = config.ssid[0] ? config.ssid : WIFI_SSID;
  const char* apPassword = config.password[0] ? config.password : WIFI_PASSWORD;
  
  if (strlen(apPassword) < 8) {
    apPassword = WIFI_PASSWORD; 
  }

  if (WiFi.softAP(apSsid, apPassword, WIFI_CHANNEL, false, MAX_WIFI_CLIENT)) {
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    initOTA();
  } else {
    Serial.println(F("[WiFi] SoftAP Gagal! Restarting..."));
    delay(1000);
    ESP.restart();
  }
  
  server.on("/", handleRoot);
  server.on("/state", handleState);
  server.on("/info", handleInfo);
  server.on("/status", handleStatus);
  server.on("/settings", HTTP_GET, handleSettings);
  server.on("/settings", HTTP_POST, handleSaveSettings);
  server.on("/race", HTTP_POST, handleRace);
  server.on("/changerace", HTTP_POST, handleChangeRace);
  server.on("/changewifi", HTTP_POST, handleChangeWifi);
  server.on("/verifywifi", HTTP_POST, handleVerifyWifi);
  server.on("/restart", HTTP_POST, handleRestart);
  server.begin();
  
  server.onNotFound([](){
    server.send(404, "text/plain", "Not Found");
  });

  lastControlMs = millis();
  lastStationCount = WiFi.softAPgetStationNum();
}

void loop() {
  server.handleClient();
  ArduinoOTA.handle();
  logStationCount(WiFi.softAPgetStationNum());
  checkFailSafe();
  dnsServer.processNextRequest();

  if (isBraking && (millis() - brakeTimeMs >= 150)) {
    isBraking = false;             
    car.drive = pendingDriveDir;   
    applyDrive();                  
  }

  if (pendingRestart && (millis() - restartTimeMs > 500)) {
    ESP.restart();
  }
  yield();
}