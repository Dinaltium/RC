/**
 * web_server.cpp — HTTP endpoints and embedded phone control UI
 *
 * Endpoints:
 *   GET  /                  → HTML control page
 *   GET  /api/status        → JSON telemetry
 *   POST /api/command       → movement command
 *   POST /api/stop          → normal stop (clear active motion)
 *   POST /api/emergency-stop→ latched emergency stop
 *   POST /api/clear-emergency→ release latched emergency stop
 *   POST /api/speed         → set speed
 *
 * HTTP handlers produce RobotCommand — they never touch GPIO directly.
 */

#include "web_server.h"
#include "config.h"
#include "command.h"
#include "command_parser.h"
#include "control_manager.h"
#include "robot_state.h"
#include <WebServer.h>
#include <Arduino.h>

static WebServer server(80);

// ============================================================
// Embedded HTML/CSS/JS control page
// ============================================================
static const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
<title>Robot Control</title>
<style>
*{margin:0;padding:0;box-sizing:border-box;}
body{font-family:'Segoe UI',system-ui,sans-serif;background:#1a1a2e;color:#e0e0e0;
  display:flex;flex-direction:column;align-items:center;min-height:100vh;padding:12px;
  -webkit-user-select:none;user-select:none;}
h1{font-size:1.3em;margin:8px 0;color:#00d4ff;}
.status{background:#16213e;border-radius:10px;padding:10px 14px;width:100%;max-width:360px;
  margin:6px 0;font-size:0.82em;line-height:1.6;}
.status .row{display:flex;justify-content:space-between;}
.status .label{color:#8899aa;}
.status .val{font-weight:600;}
.on{color:#00e676;} .off{color:#ff5252;}
.controls{display:grid;grid-template-columns:repeat(3,72px);grid-template-rows:repeat(3,72px);
  gap:6px;margin:10px 0;}
.btn{border:none;border-radius:12px;font-size:1.4em;font-weight:700;cursor:pointer;
  background:#0f3460;color:#e0e0e0;display:flex;align-items:center;justify-content:center;
  transition:background 0.15s;-webkit-tap-highlight-color:transparent;}
.btn:active{background:#e94560;}
.btn.stop{background:#f57c00;color:#fff;font-size:1em;}
.btn.stop:active{background:#e65100;}
.extra{display:flex;gap:6px;margin:6px 0;}
.extra .btn{width:110px;height:50px;font-size:0.9em;}
.speed-row{display:flex;align-items:center;gap:10px;margin:8px 0;}
.speed-row input[type=range]{width:180px;accent-color:#00d4ff;}
.speed-val{font-weight:700;color:#00d4ff;min-width:36px;text-align:center;}
.estop-row{display:flex;gap:8px;margin:10px 0;}
.btn.estop{background:#d32f2f;color:#fff;font-size:0.85em;padding:10px 14px;width:170px;height:44px;}
.btn.estop:active{background:#b71c1c;}
.btn.clrestop{background:#388e3c;color:#fff;font-size:0.85em;padding:10px 14px;width:110px;height:44px;}
.btn.clrestop:active{background:#2e7d32;}
.warn{color:#ff9800;font-weight:600;margin:4px 0;font-size:0.95em;}
.warn.danger{color:#ff1744;font-size:1.05em;}
</style>
</head>
<body>
<h1>&#x1F916; Robot Prototype</h1>

<div class="status" id="st">
  <div class="row"><span class="label">Source</span><span class="val" id="s_src">--</span></div>
  <div class="row"><span class="label">Command</span><span class="val" id="s_cmd">--</span></div>
  <div class="row"><span class="label">Speed</span><span class="val" id="s_spd">--</span></div>
  <div class="row"><span class="label">Distance</span><span class="val" id="s_dst">--</span></div>
  <div class="row"><span class="label">Obstacle</span><span class="val" id="s_obs">--</span></div>
  <div class="row"><span class="label">ESP-NOW</span><span class="val" id="s_espnow">--</span></div>
  <div class="row"><span class="label">Uptime</span><span class="val" id="s_up">--</span></div>
</div>
<div class="warn" id="s_warn"></div>

<div class="controls">
  <div></div>
  <button class="btn" ontouchstart="send('FORWARD')" onmousedown="send('FORWARD')">&#x25B2;</button>
  <div></div>
  <button class="btn" ontouchstart="send('LEFT')" onmousedown="send('LEFT')">&#x25C0;</button>
  <button class="btn stop" ontouchstart="send('STOP')" onmousedown="send('STOP')">STOP</button>
  <button class="btn" ontouchstart="send('RIGHT')" onmousedown="send('RIGHT')">&#x25B6;</button>
  <div></div>
  <button class="btn" ontouchstart="send('BACKWARD')" onmousedown="send('BACKWARD')">&#x25BC;</button>
  <div></div>
</div>

<div class="extra">
  <button class="btn" ontouchstart="send('ROTATE_LEFT')" onmousedown="send('ROTATE_LEFT')">&#x21BA; Rot L</button>
  <button class="btn" ontouchstart="send('ROTATE_RIGHT')" onmousedown="send('ROTATE_RIGHT')">&#x21BB; Rot R</button>
</div>

<div class="speed-row">
  <span class="label">Speed:</span>
  <input type="range" id="spdSlider" min="0" max="255" value="150"
    oninput="updSpd(this.value)">
  <span class="speed-val" id="spdVal">150</span>
</div>

<div class="estop-row">
  <button class="btn estop" ontouchstart="eStop()" onmousedown="eStop()">&#x26A0; E-STOP</button>
  <button class="btn clrestop" ontouchstart="clrEStop()" onmousedown="clrEStop()">Clear E-Stop</button>
</div>

<script>
let hbTimer=null, pollTimer=null;

function send(action){
  let spd=document.getElementById('spdSlider').value;
  fetch('/api/command',{method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify({action:action,speed:parseInt(spd)})
  }).catch(()=>{});
}

function eStop(){
  fetch('/api/emergency-stop',{method:'POST'}).catch(()=>{});
}

function clrEStop(){
  fetch('/api/clear-emergency',{method:'POST'}).catch(()=>{});
}

function updSpd(v){
  document.getElementById('spdVal').textContent=v;
  fetch('/api/speed',{method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify({speed:parseInt(v)})
  }).catch(()=>{});
}

function heartbeat(){
  fetch('/api/command',{method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify({action:'HEARTBEAT',speed:0})
  }).catch(()=>{});
}

function poll(){
  fetch('/api/status').then(r=>r.json()).then(d=>{
    document.getElementById('s_src').textContent=d.source||'--';
    document.getElementById('s_cmd').textContent=d.command||'--';
    document.getElementById('s_spd').textContent=d.speed;
    document.getElementById('s_dst').textContent=d.distance_cm.toFixed(1)+' cm';

    let obsEl=document.getElementById('s_obs');
    obsEl.textContent=d.obstacle?'YES':'No';
    obsEl.className='val '+(d.obstacle?'off':'on');

    let espEl=document.getElementById('s_espnow');
    espEl.textContent=d.espnow_connected?'Online':'Offline';
    espEl.className='val '+(d.espnow_connected?'on':'off');

    let s=Math.floor(d.uptime_ms/1000);
    let m=Math.floor(s/60); s=s%60;
    document.getElementById('s_up').textContent=m+'m '+s+'s';

    let w=document.getElementById('s_warn');
    if(d.emergency_stop) {
      w.textContent='🚨 LATCHED EMERGENCY STOP ACTIVE';
      w.className='warn danger';
    } else if(d.obstacle) {
      w.textContent='⚠ Obstacle detected (<30cm)';
      w.className='warn';
    } else if(d.fault) {
      w.textContent='⚠ System Fault';
      w.className='warn';
    } else {
      w.textContent='';
      w.className='warn';
    }
  }).catch(()=>{});
}

hbTimer=setInterval(heartbeat,500);
pollTimer=setInterval(poll,400);
poll();
</script>
</body>
</html>
)rawliteral";

// ============================================================
// Route handlers
// ============================================================
static void handleRoot() {
    server.send(200, "text/html", HTML_PAGE);
}

static void handleStatus() {
    stateUpdateUptime();
    String json = stateToJson();
    server.send(200, "application/json", json);
}

static void handleCommand() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"no body\"}");
        return;
    }

    String body = server.arg("plain");
    RobotCommand cmd;

    if (!parseJsonCommand(body, cmd)) {
        server.send(400, "application/json", "{\"error\":\"invalid command\"}");
        return;
    }

    cmd.source = SRC_PHONE_WIFI;

    if (cmd.action == CMD_HEARTBEAT) {
        controlHeartbeat(SRC_PHONE_WIFI);
    } else {
        controlSubmitCommand(cmd);
    }

    server.send(200, "application/json", "{\"ok\":true}");
}

// Normal STOP — clears motion, robot remains available
static void handleStop() {
    RobotCommand cmd;
    cmd.source    = SRC_PHONE_WIFI;
    cmd.action    = CMD_STOP;
    cmd.speed     = 0;
    cmd.sequence  = 0;
    cmd.timestamp = millis();

    controlSubmitCommand(cmd);
    server.send(200, "application/json", "{\"ok\":true,\"action\":\"STOP\"}");
}

// Latched EMERGENCY STOP — enters latched state
static void handleEmergencyStop() {
    RobotCommand cmd;
    cmd.source    = SRC_PHONE_WIFI;
    cmd.action    = CMD_EMERGENCY_STOP;
    cmd.speed     = 0;
    cmd.sequence  = 0;
    cmd.timestamp = millis();

    controlSubmitCommand(cmd);
    server.send(200, "application/json", "{\"ok\":true,\"action\":\"EMERGENCY_STOP\"}");
}

// Clear Latched EMERGENCY STOP
static void handleClearEmergency() {
    RobotCommand cmd;
    cmd.source    = SRC_PHONE_WIFI;
    cmd.action    = CMD_CLEAR_EMERGENCY;
    cmd.speed     = 0;
    cmd.sequence  = 0;
    cmd.timestamp = millis();

    controlSubmitCommand(cmd);
    server.send(200, "application/json", "{\"ok\":true,\"action\":\"CLEAR_EMERGENCY\"}");
}

static void handleSpeed() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"no body\"}");
        return;
    }

    String body = server.arg("plain");
    int idx = body.indexOf("\"speed\":");
    if (idx < 0) {
        server.send(400, "application/json", "{\"error\":\"missing speed\"}");
        return;
    }

    String numStr = "";
    int pos = idx + 8;
    while (pos < (int)body.length() && body.charAt(pos) == ' ') pos++;
    while (pos < (int)body.length() && body.charAt(pos) >= '0' && body.charAt(pos) <= '9') {
        numStr += body.charAt(pos);
        pos++;
    }

    int spd = numStr.toInt();
    if (spd < 0) spd = 0;
    if (spd > MAX_SPEED) spd = MAX_SPEED;

    stateSetSpeed((uint8_t)spd);
    controlHeartbeat(SRC_PHONE_WIFI);

    server.send(200, "application/json", "{\"ok\":true,\"speed\":" + String(spd) + "}");
}

// ============================================================
// Init & handle
// ============================================================
void webServerInit() {
    server.on("/",                   HTTP_GET,  handleRoot);
    server.on("/api/status",         HTTP_GET,  handleStatus);
    server.on("/api/command",        HTTP_POST, handleCommand);
    server.on("/api/stop",           HTTP_POST, handleStop);
    server.on("/api/emergency-stop", HTTP_POST, handleEmergencyStop);
    server.on("/api/clear-emergency",HTTP_POST, handleClearEmergency);
    server.on("/api/speed",          HTTP_POST, handleSpeed);

    server.begin();
    Serial.println(F("[WEB] Server started on port 80"));
}

void webServerHandle() {
    server.handleClient();
}
