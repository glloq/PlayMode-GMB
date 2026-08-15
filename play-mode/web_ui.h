#ifndef WEB_UI_H
#define WEB_UI_H

#include <Arduino.h>

// ============================================================================
// PlayMode — Embedded Web UI (Phase 6)
// ============================================================================
//
// HTML/CSS/JS interface embedded in PROGMEM.
// Single-page app with:
//   - Real-time dashboard (WebSocket)
//   - Instrument / actuator / MIDI mapping management
//   - Interactive virtual piano
//   - Power / safety monitoring
//   - Advanced configuration
//

const char WEB_UI_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>PlayMode</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
:root{
  --bg:#0d1117;--bg2:#161b22;--bg3:#21262d;
  --fg:#c9d1d9;--fg2:#8b949e;--accent:#58a6ff;
  --green:#3fb950;--yellow:#d29922;--red:#f85149;
  --border:#30363d;--radius:8px;
}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;
  background:var(--bg);color:var(--fg);font-size:14px;line-height:1.5}
a{color:var(--accent);text-decoration:none}

/* Layout */
.header{background:var(--bg2);border-bottom:1px solid var(--border);
  padding:12px 20px;display:flex;align-items:center;gap:12px;flex-wrap:wrap;position:relative}
.header h1{font-size:18px;font-weight:600;margin:0}
.header h1 span{color:var(--accent)}
.header .logo{position:absolute;left:50%;top:50%;transform:translate(-50%,-50%);line-height:0}
.header .status{margin-left:auto}
.header .dot{width:8px;height:8px;border-radius:50%;display:inline-block;
  margin-right:4px;background:var(--green)}
.header .dot.off{background:var(--red)}
.mstate{font-size:11px;font-weight:700;text-transform:uppercase;letter-spacing:.5px;
  padding:3px 8px;border-radius:10px;margin:0 8px;white-space:nowrap}
.mstate-armed{background:rgba(46,160,67,.18);color:var(--green);border:1px solid var(--green)}
.mstate-disarmed{background:rgba(210,153,34,.18);color:var(--yellow);border:1px solid var(--yellow)}
.mstate-fault{background:rgba(248,81,73,.18);color:var(--red);border:1px solid var(--red)}
.mstate-unknown{background:var(--bg2);color:var(--fg2);border:1px solid var(--border)}
#toast-container{position:fixed;top:12px;right:12px;z-index:9999;display:flex;
  flex-direction:column;gap:8px;max-width:min(360px,90vw);pointer-events:none}
#toast-container .alert{pointer-events:auto;box-shadow:0 4px 16px rgba(0,0,0,.35)}

nav{background:var(--bg2);border-bottom:1px solid var(--border);
  display:flex;gap:0;overflow-x:auto;-webkit-overflow-scrolling:touch}
nav::-webkit-scrollbar{height:2px}
nav::-webkit-scrollbar-thumb{background:var(--border)}
nav button{background:none;border:none;color:var(--fg2);padding:10px 18px;
  cursor:pointer;font-size:13px;border-bottom:2px solid transparent;
  white-space:nowrap;transition:all .15s}
nav button:hover{color:var(--fg);background:var(--bg3)}
nav button.active{color:var(--accent);border-bottom-color:var(--accent)}

.page{display:none;padding:20px;max-width:1200px;margin:0 auto}
.page.active{display:block}

/* Cards */
.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:12px;margin-bottom:20px}
.card{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:16px}
.card h3{font-size:13px;color:var(--fg2);margin-bottom:8px;text-transform:uppercase;letter-spacing:.5px}
.card .val{font-size:28px;font-weight:700}
.card .unit{font-size:13px;color:var(--fg2);margin-left:4px}
.card .bar{height:6px;background:var(--bg3);border-radius:3px;margin-top:8px;overflow:hidden}
.card .bar-fill{height:100%;border-radius:3px;transition:width .3s}
.card .sub{font-size:12px;color:var(--fg2);margin-top:4px}

/* Alerts */
.alert{padding:10px 14px;border-radius:var(--radius);margin-bottom:12px;
  font-size:13px;display:none;align-items:center;gap:8px}
.alert.warn{display:flex;background:#d299221a;border:1px solid #d2992233;color:var(--yellow)}
.alert.danger{display:flex;background:#f851491a;border:1px solid #f8514933;color:var(--red)}
.alert.ok{display:flex;background:#3fb9501a;border:1px solid #3fb95033;color:var(--green)}

/* Tables */
.table-responsive{overflow-x:auto;-webkit-overflow-scrolling:touch}
table{width:100%;border-collapse:collapse;margin-bottom:16px}
th,td{text-align:left;padding:8px 12px;border-bottom:1px solid var(--border)}
th{font-size:12px;color:var(--fg2);text-transform:uppercase;letter-spacing:.5px;background:var(--bg2)}
td{font-size:13px}
tr:hover td{background:var(--bg2)}
.badge{display:inline-block;padding:2px 8px;border-radius:12px;font-size:11px;font-weight:600}
.badge.on{background:#3fb95033;color:var(--green)}
.badge.off{background:var(--bg3);color:var(--fg2)}
.badge.servo{background:#58a6ff22;color:var(--accent)}
.badge.sol{background:#d2992222;color:var(--yellow)}
.inst-separator td{background:var(--bg3);font-weight:600;font-size:13px;padding:8px 10px !important;
  border-top:2px solid var(--border)}

/* Buttons — min-height 44px WCAG 2.5.5 touch target */
.btn{background:var(--bg3);border:1px solid var(--border);color:var(--fg);
  padding:6px 14px;border-radius:6px;cursor:pointer;font-size:13px;
  transition:all .15s;display:inline-flex;align-items:center;gap:6px;min-height:44px}
.btn:hover{background:var(--border);border-color:var(--fg2)}
.btn.primary{background:var(--accent);border-color:var(--accent);color:#fff}
.btn.primary:hover{opacity:.85}
.btn.danger{background:var(--red);border-color:var(--red);color:#fff}
.btn.danger:hover{opacity:.85}
.btn.sm{padding:4px 10px;font-size:12px;min-height:36px}
.btn-row{display:flex;gap:8px;margin:12px 0;flex-wrap:wrap}

/* Forms */
.form-group{margin-bottom:14px}
.form-group label{display:block;font-size:12px;color:var(--fg2);
  margin-bottom:4px;text-transform:uppercase;letter-spacing:.5px}
.form-group input,.form-group select{width:100%;background:var(--bg);
  border:1px solid var(--border);color:var(--fg);padding:8px 10px;
  border-radius:6px;font-size:13px}
.form-group input:focus,.form-group select:focus{border-color:var(--accent);outline:none}
.form-row{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.form-row.tri{grid-template-columns:1fr 1fr 1fr}
.form-select{background:var(--bg);color:var(--fg);border:1px solid var(--border);
  padding:4px 8px;border-radius:4px;font-size:13px}

/* Modal */
.modal-overlay{display:none;position:fixed;inset:0;background:rgba(0,0,0,.6);
  z-index:100;align-items:center;justify-content:center;padding:8px}
.modal-overlay.show{display:flex}
.modal{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);
  padding:24px;width:90%;max-width:500px;max-height:90vh;overflow-y:auto}
.modal h2{font-size:16px;margin-bottom:16px}

/* Piano — wider keys for touch */
.piano-scroll-wrap{display:flex;align-items:center;gap:4px}
.piano-scroll-wrap .piano-nav{display:none;background:var(--bg3);border:none;color:var(--fg);
  font-size:20px;padding:8px 6px;border-radius:6px;cursor:pointer;flex-shrink:0;
  height:60px;align-self:center;line-height:1;touch-action:manipulation}
.piano-scroll-wrap .piano-nav:active{background:var(--accent);color:#fff}
.piano-container{overflow-x:auto;padding:12px 0;-webkit-overflow-scrolling:touch;touch-action:pan-x;flex:1;min-width:0}
.piano{--wk:40px;display:flex;position:relative;height:130px;user-select:none;touch-action:none}
.piano .white{width:var(--wk);height:130px;background:#f0f0f0;border:1px solid #999;
  border-radius:0 0 4px 4px;cursor:pointer;position:relative;z-index:1;
  display:flex;align-items:flex-end;justify-content:center;padding-bottom:4px;
  font-size:9px;color:#666;transition:background .1s;flex-shrink:0;
  -webkit-user-select:none;user-select:none;-webkit-touch-callout:none}
.piano .white:hover{background:#e0e8f0}
.piano .white.active{background:var(--accent);color:#fff}
.piano .white.muted{display:none}
.piano .black{width:calc(var(--wk) * 0.65);height:85px;background:#222;border:1px solid #000;
  border-radius:0 0 3px 3px;cursor:pointer;position:absolute;z-index:2;
  transition:background .1s;
  -webkit-user-select:none;user-select:none;-webkit-touch-callout:none}
.piano .black:hover{background:#444}
.piano .black.active{background:var(--accent)}
.piano .black.muted{display:none}

/* Section titles — flexbox for mobile alignment */
.section-title{font-size:16px;font-weight:600;margin-bottom:16px;
  padding-bottom:8px;border-bottom:1px solid var(--border);
  display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:8px}

/* Log container */
.log-container{overflow-y:auto;max-height:calc(100vh - 280px)}

/* Responsive — mobile (<600px) */
@media(max-width:600px){
  .page{padding:10px 8px}
  .cards{grid-template-columns:1fr 1fr}
  .form-row{grid-template-columns:1fr}
  .form-row.tri{grid-template-columns:1fr}
  nav{flex-wrap:wrap;gap:4px;padding:8px}
  nav button{padding:8px 12px;font-size:12px;flex:1;min-width:80px}
  .form-group input,.form-group select{font-size:16px}
  .header{padding:12px}
  .header h1{font-size:20px}
  .header .status{}
  .log-container{max-height:calc(100vh - 150px)}
  .modal{padding:14px;width:95%}
  .section-title{font-size:14px;flex-wrap:wrap;gap:6px}
  .btn-row{flex-wrap:wrap;gap:6px}
  .table-responsive{overflow-x:auto;-webkit-overflow-scrolling:touch}
  table{font-size:12px;min-width:500px}
  table th,table td{padding:6px 4px;white-space:nowrap}
  .piano-container{overflow-x:auto;-webkit-overflow-scrolling:touch;padding-bottom:8px}
  .piano-scroll-wrap .piano-nav{display:block}
  .piano{--wk:44px;min-width:auto;height:120px}
  .piano .white{height:120px;font-size:8px}
  .piano .black{height:75px}
  .welcome h2{font-size:22px}
  .welcome-steps{flex-direction:column;align-items:center}
  .welcome-step{max-width:100%;min-width:auto}
  .section-collapse-toggle{font-size:13px;padding:10px 12px}
}
@media(max-width:380px){
  .cards{grid-template-columns:1fr}
  .piano{--wk:40px;height:110px}
  .piano .white{height:110px;font-size:7px}
  .piano .black{height:68px}
  nav button{font-size:11px;padding:6px 8px}
}
@media(min-width:601px) and (max-width:768px){
  .cards{grid-template-columns:repeat(auto-fit,minmax(160px,1fr))}
  .form-row.tri{grid-template-columns:1fr 1fr}
}
@media(max-width:768px) and (orientation:landscape){
  .log-container{max-height:calc(100vh - 120px)}
  .modal{max-height:95vh}
}
/* Gear settings dropdown */
.gear-btn{background:none;border:none;color:var(--fg2);font-size:20px;cursor:pointer;
  padding:6px 10px;border-radius:6px;transition:all .15s;min-width:44px;min-height:44px;
  display:flex;align-items:center;justify-content:center}
.gear-btn:hover{color:var(--fg);background:var(--bg3)}
/* Help text under form fields */
.help{font-size:11px;color:var(--fg2);margin-top:2px;line-height:1.3}

/* Angle preview for servo */
.angle-preview{background:var(--bg);border:1px solid var(--border);border-radius:var(--radius);
  padding:12px;margin:12px 0;text-align:center}
.angle-preview svg{display:block;margin:0 auto}
.angle-info{display:flex;justify-content:space-around;margin-top:8px;font-size:12px;color:var(--fg2)}
.angle-info span{color:var(--fg);font-weight:600}

/* Mic status indicator */
.mic-status{display:flex;align-items:center;gap:8px;padding:10px 14px;border-radius:var(--radius);
  margin-bottom:16px;font-size:13px}
.mic-status.ok{background:#3fb9501a;border:1px solid #3fb95033;color:var(--green)}
.mic-status.no{background:#f851491a;border:1px solid #f8514933;color:var(--red)}

/* Wizard steps */
.wiz-steps{display:flex;gap:4px;margin-bottom:16px;align-items:flex-start}
.wiz-step-item{display:flex;flex-direction:column;align-items:center;gap:2px;flex:1}
.wiz-dot{width:28px;height:28px;border-radius:50%;background:var(--bg3);color:var(--fg2);
  display:flex;align-items:center;justify-content:center;font-size:12px;font-weight:600}
.wiz-dot.active{background:var(--accent);color:#fff}
.wiz-dot.done{background:var(--green);color:#fff}
.wiz-step-label{font-size:10px;color:var(--fg2);text-align:center;white-space:nowrap}
.wiz-step-item.active .wiz-step-label{color:var(--accent);font-weight:600}
.wiz-connector{flex:1;height:2px;background:var(--bg3);align-self:center;margin-top:13px;min-width:12px}
.wiz-connector.done{background:var(--green)}
.wiz-panel{min-height:120px}

/* Inline note input in actuator table */
.note-input{width:56px;background:var(--bg);border:1px solid var(--border);color:var(--fg);
  padding:2px 6px;border-radius:4px;font-size:12px;text-align:center}
.note-input:focus{border-color:var(--accent);outline:none}
.note-label{font-size:11px;color:var(--fg2);margin-left:4px}

/* Expert collapsible sections */
.expert-section{margin:12px 0}
.expert-toggle{background:none;border:1px dashed var(--border);color:var(--fg2);padding:8px 12px;
  border-radius:6px;cursor:pointer;font-size:12px;width:100%;text-align:left;
  display:flex;align-items:center;gap:6px;transition:all .15s}
.expert-toggle:hover{color:var(--fg);background:var(--bg3)}
.expert-toggle::before{content:'\25B6';font-size:10px;transition:transform .2s;display:inline-block}
.expert-toggle.open::before{transform:rotate(90deg)}
.expert-body{display:none;padding:12px 0 0}
.expert-body.open{display:block}

/* Wizard note table */
.wiz-note-table{max-height:200px;overflow-y:auto;border:1px solid var(--border);
  border-radius:var(--radius);margin:8px 0}
.wiz-note-table table{margin:0}
.wiz-note-table td{padding:4px 8px;font-size:12px}
.wiz-note-table th{padding:4px 8px;position:sticky;top:0;z-index:1}

/* Welcome page (first-run) */
.welcome{text-align:center;padding:40px 20px;max-width:600px;margin:0 auto}
.welcome h2{font-size:28px;font-weight:700;margin-bottom:8px}
.welcome h2 span{color:var(--accent)}
.welcome .subtitle{color:var(--fg2);font-size:15px;margin-bottom:36px;line-height:1.6}
.welcome-steps{display:flex;gap:20px;justify-content:center;margin-bottom:36px;flex-wrap:wrap}
.welcome-step{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);
  padding:20px 16px;flex:1;min-width:150px;max-width:180px}
.welcome-step .step-num{width:36px;height:36px;border-radius:50%;background:var(--accent);color:#fff;
  display:inline-flex;align-items:center;justify-content:center;font-size:16px;font-weight:700;margin-bottom:10px}
.welcome-step h3{font-size:14px;font-weight:600;margin-bottom:4px}
.welcome-step p{font-size:12px;color:var(--fg2);line-height:1.4}
.welcome .btn-big{padding:14px 32px;font-size:16px;font-weight:600;min-height:52px;border-radius:8px}

/* Collapsible sections for merged content */
.section-collapse{margin:20px 0}
.section-collapse-toggle{background:var(--bg2);border:1px solid var(--border);color:var(--fg);
  padding:12px 16px;border-radius:var(--radius);cursor:pointer;font-size:14px;font-weight:600;
  width:100%;text-align:left;display:flex;align-items:center;justify-content:space-between;
  transition:all .15s}
.section-collapse-toggle:hover{background:var(--bg3);border-color:var(--fg2)}
.section-collapse-toggle .toggle-count{font-size:12px;color:var(--fg2);font-weight:400}
.section-collapse-toggle::after{content:'\25B6';font-size:11px;color:var(--fg2);transition:transform .2s}
.section-collapse-toggle.open::after{transform:rotate(90deg)}
.section-collapse-body{display:none;padding:16px 0 0}
.section-collapse-body.open{display:block}

/* Wiring page — auto-generated electrical diagram */
.wire-note{font-size:13px;color:var(--fg2);margin-bottom:12px;line-height:1.6}
.wire-wrap{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);
  padding:12px;margin-bottom:20px}
.wire-scroll{overflow-x:auto;-webkit-overflow-scrolling:touch}
.wire-svg{display:block;height:auto;margin:0 auto}
.wire-empty{color:var(--fg2);font-size:13px;text-align:center;padding:40px 12px}
.wire-legend{display:flex;flex-wrap:wrap;gap:8px 18px;font-size:12px;color:var(--fg2);
  margin-top:12px;padding-top:12px;border-top:1px solid var(--border)}
.wire-legend i{display:inline-block;width:12px;height:12px;border-radius:3px;
  margin-right:6px;vertical-align:-2px;font-style:normal}
.wire-legend i.line{height:3px;width:16px;border-radius:2px;vertical-align:3px}
.wire-issue{display:flex;gap:8px;align-items:flex-start;padding:9px 12px;border-radius:6px;
  font-size:13px;margin-bottom:8px;border:1px solid;line-height:1.5}
.wire-issue.err{background:#f851491a;border-color:#f8514933;color:var(--red)}
.wire-issue.warn{background:#d299221a;border-color:#d2992233;color:var(--yellow)}
.wire-issue.ok{background:#3fb9501a;border-color:#3fb95033;color:var(--green)}
.wire-issue .ico{flex:0 0 auto}
.wire-tips{list-style:none;font-size:13px;line-height:1.6;margin-bottom:16px}
.wire-tips li{padding:7px 0 7px 22px;position:relative;border-bottom:1px solid var(--border);color:var(--fg)}
.wire-tips li:last-child{border-bottom:none}
.wire-tips li::before{content:'\25B8';position:absolute;left:4px;color:var(--accent)}
.wire-tips li b{color:var(--fg)}
.wire-check{list-style:none;font-size:13px;line-height:1.6}
.wire-check li{padding:8px 0 8px 26px;position:relative;border-bottom:1px solid var(--border)}
.wire-check li:last-child{border-bottom:none}
.wire-check li::before{content:'\2610';position:absolute;left:4px;color:var(--fg2);font-size:15px}
</style>
</head>
<body>

<!-- Header -->
<div class="header">
  <h1><span>Play</span>Mode</h1>
  <div class="logo" aria-label="B-infini-P">
    <svg viewBox="0 0 80 32" width="80" height="32">
      <text x="4" y="23" font-size="18" font-weight="700" fill="currentColor">B</text>
      <circle cx="40" cy="16" r="13" fill="none" stroke="var(--accent)" stroke-width="2"/>
      <text x="40" y="22" font-size="18" font-weight="700" fill="var(--accent)" text-anchor="middle">&infin;</text>
      <text x="64" y="23" font-size="18" font-weight="700" fill="currentColor">P</text>
    </svg>
  </div>
  <div class="status">
    <span class="dot" id="ws-dot"></span>
    <span id="ws-status" style="font-size:11px;color:var(--fg2)">Connecting…</span>
  </div>
  <!-- AUDIT FIX (UX): permanent machine state + arm/kill control in the header -->
  <span id="machine-state" class="mstate mstate-unknown" title="Output state">—</span>
  <button id="arm-btn" class="btn sm" onclick="toggleArm()" title="Arm or disable the outputs" style="display:none">Arm</button>
  <button class="gear-btn" onclick="showPage('settings')" title="System settings">&#9881;</button>
</div>

<!-- AUDIT FIX (UI-P1): global toast container, outside the pages, so messages
     are visible on every page (not only the Instrument page). -->
<div id="toast-container" aria-live="polite" aria-atomic="false"></div>

<!-- Navigation -->
<nav id="main-nav">
  <button class="active" onclick="showPage('instrument')">Instrument</button>
  <button onclick="showPage('midi')">MIDI</button>
  <button onclick="showPage('actuators')">Actuators</button>
  <button onclick="showPage('wiring')">Wiring</button>
  <button onclick="showPage('calibration')" id="nav-cal" style="display:none">Calibration</button>
</nav>

<!-- ============ WELCOME (First-run) ============ -->
<div class="page" id="page-welcome">
  <div class="welcome">
    <h2>Welcome to Midi <span>B&infin;p</span></h2>
    <p class="subtitle">Turn any object into a MIDI musical instrument.<br>
    Servos, solenoids, percussion... anything is possible.</p>
    <div class="welcome-steps">
      <div class="welcome-step">
        <div class="step-num">1</div>
        <h3>Create</h3>
        <p>Define your instrument and its actuators</p>
      </div>
      <div class="welcome-step">
        <div class="step-num">2</div>
        <h3>Connect</h3>
        <p>Plug in your servos or solenoids via PCA9685</p>
      </div>
      <div class="welcome-step">
        <div class="step-num">3</div>
        <h3>Play</h3>
        <p>Send MIDI and let the music do its thing</p>
      </div>
    </div>
    <button class="btn primary btn-big" onclick="openWizard()">Create my first instrument</button>
    <p style="color:var(--fg2);font-size:12px;margin-top:16px">Or <a href="#" onclick="showPage('instrument');return false">skip to manual configuration</a></p>
  </div>
</div>

<!-- ============ INSTRUMENT (Instruments + independent pianos) ============ -->
<div class="page active" id="page-instrument">
  <div id="alert-zone"></div>

  <div class="section-title"><span>My instruments</span>
    <div style="display:flex;gap:8px">
      <button class="btn primary sm" onclick="openWizard()">+ Wizard</button>
      <button class="btn sm" onclick="openInstrumentModal()">+ Manual</button>
    </div>
  </div>
  <div class="table-responsive">
  <table>
    <thead><tr><th>Name</th><th>Channel</th><th>Type</th><th>Actuators</th><th>Status</th><th>Actions</th></tr></thead>
    <tbody id="home-instruments-table"><tr><td colspan="6" style="color:var(--fg2)">Loading...</td></tr></tbody>
  </table>
  </div>

  <!-- One piano per instrument (generated dynamically) -->
  <div id="pianos-container"></div>
</div>

<!-- ============ SETTINGS (unified gear menu page) ============ -->
<div class="page" id="page-settings">

  <!-- Monitoring -->
  <div class="section-title">Monitoring</div>
  <div class="cards">
    <div class="card">
      <h3>MIDI</h3>
      <div class="val" id="d-midi-recv">0</div>
      <div class="sub">Routed: <span id="d-midi-routed">0</span> | Rejected: <span id="d-midi-unmapped">0</span></div>
    </div>
    <div class="card">
      <h3>Polyphony</h3>
      <div class="val"><span id="d-active">0</span><span class="unit">/ <span id="p-poly-max">12</span></span></div>
      <div class="bar"><div class="bar-fill" id="p-total-bar" style="width:0%;background:var(--green)"></div></div>
      <div class="sub">Rejected: <span id="p-rejected">0</span></div>
    </div>
    <div class="card">
      <h3>Scheduler</h3>
      <div class="val" id="d-sched-queued">0</div>
      <div class="sub">queued | <span id="d-sched-processed">0</span> processed</div>
    </div>
    <div class="card">
      <h3>WiFi</h3>
      <div class="val" id="d-wifi-rssi">-</div>
      <div class="sub" id="d-wifi-status">-</div>
    </div>
  </div>

  <!-- Polyphony & Safety -->
  <div class="section-title" style="margin-top:24px">Polyphony &amp; Safety</div>
  <div id="safety-alert-zone"></div>
  <div class="form-row" style="margin-bottom:12px">
    <div class="form-group">
      <label>Max polyphony</label>
      <input type="number" id="pw-poly" value="12" min="1" max="64">
      <div class="help">Maximum number of simultaneously active actuators</div>
    </div>
    <div class="form-group" style="display:flex;align-items:center;gap:8px;padding-top:18px">
      <button class="btn primary sm" onclick="savePowerBudget()">Apply</button>
    </div>
  </div>
  <div class="cards" style="grid-template-columns:repeat(auto-fit,minmax(120px,1fr));margin-bottom:12px">
    <div class="card">
      <h3>Kill Switch</h3>
      <div class="val" id="s-kill" style="color:var(--green)">OFF</div>
    </div>
    <div class="card">
      <h3>Degradation</h3>
      <div class="val" id="s-degrad" style="color:var(--green)">No</div>
    </div>
  </div>
  <div class="btn-row" style="margin-bottom:12px">
    <button class="btn danger sm" onclick="toggleKillSwitch(true)">KILL SWITCH ON</button>
    <button class="btn sm" onclick="toggleKillSwitch(false)">Kill Switch Off</button>
  </div>

  <div class="section-collapse">
    <button class="section-collapse-toggle" onclick="toggleCollapse(this)">
      <span>Safety limits (advanced)</span>
    </button>
    <div class="section-collapse-body">
      <div class="form-row tri">
        <div class="form-group">
          <label>Duty cycle max (%)</label>
          <input type="number" id="sf-duty" value="80" min="10" max="100">
        </div>
        <div class="form-group">
          <label>Max frequency (Hz)</label>
          <input type="number" id="sf-freq" value="50" min="1" max="200">
        </div>
        <div class="form-group">
          <label>Watchdog timeout (ms)</label>
          <input type="number" id="sf-watchdog" value="5000" min="1000" max="30000">
        </div>
      </div>
      <button class="btn primary sm" onclick="saveSafetyConfig()">Apply limits</button>
    </div>
  </div>

  <!-- Logs -->
  <div class="section-title" style="margin-top:24px"><span>System log</span>
    <div style="display:flex;gap:6px">
      <button class="btn sm" onclick="loadLogs()">Refresh</button>
      <button class="btn sm" onclick="clearLogs()">Clear</button>
    </div>
  </div>
  <div style="display:flex;gap:8px;margin-bottom:10px;flex-wrap:wrap;align-items:center;font-size:12px">
    <select id="log-level-filter" onchange="renderLogs()" class="form-select" style="font-size:12px">
      <option value="0">DEBUG+</option>
      <option value="1">INFO+</option>
      <option value="2">WARN+</option>
      <option value="3">ERROR+</option>
    </select>
    <select id="log-cat-filter" onchange="renderLogs()" class="form-select" style="font-size:12px">
      <option value="-1">All</option>
      <option value="0">System</option>
      <option value="1">MIDI</option>
      <option value="2">Scheduler</option>
      <option value="3">Safety</option>
      <option value="4">Power</option>
      <option value="5">Calibration</option>
      <option value="6">Test</option>
    </select>
    <label style="margin-left:auto;display:flex;align-items:center;gap:4px"><input type="checkbox" id="log-autoscroll" checked> Auto-scroll</label>
  </div>
  <div class="log-container">
    <div class="table-responsive">
    <table>
      <thead><tr><th style="width:80px">Time</th><th style="width:50px">Level</th><th style="width:65px">Cat</th><th>Message</th></tr></thead>
      <tbody id="log-table"><tr><td colspan="4" style="color:var(--fg2)">Loading...</td></tr></tbody>
    </table>
    </div>
  </div>
  <div id="log-count-info" style="color:var(--fg2);font-size:12px;margin-top:6px;text-align:right"></div>

  <!-- WiFi -->
  <div class="section-title" style="margin-top:24px">WiFi Connection</div>
  <div class="form-row">
    <div class="form-group">
      <label>Network SSID</label>
      <input type="text" id="set-ssid" maxlength="32" placeholder="WiFi name">
    </div>
    <div class="form-group">
      <label>Password</label>
      <input type="password" id="set-pass" maxlength="64" placeholder="leave empty to keep current">
      <div class="help">Leave empty to keep the stored WiFi password.</div>
    </div>
  </div>
  <div class="form-row">
    <div class="form-group">
      <label>Hostname</label>
      <input type="text" id="set-hostname" value="play-mode" maxlength="31">
      <div class="help">Accessible via hostname.local on the network</div>
    </div>
    <div class="form-group">
      <label>AP Fallback</label>
      <select id="set-ap-fallback">
        <option value="1">Yes &mdash; creates an access point if WiFi fails</option>
        <option value="0">No</option>
      </select>
    </div>
  </div>
  <div class="form-row">
    <div class="form-group">
      <label>Access Point password (WPA2)</label>
      <input type="password" id="set-ap-pass" maxlength="64" placeholder="leave empty to keep current">
      <div class="help">Protects the device's own hotspot (min 8 characters). Leave empty to keep the current password. New devices use a unique password shown in the SSID hint below.</div>
    </div>
  </div>
  <button class="btn primary" onclick="saveWiFiConfig()">Save WiFi</button>

  <!-- I&sup2;C Bus -->
  <div class="section-collapse" style="margin-top:24px">
    <button class="section-collapse-toggle" onclick="toggleCollapse(this)">
      <span>I&sup2;C Bus (advanced)</span>
    </button>
    <div class="section-collapse-body">
      <div class="table-responsive">
      <table>
        <thead><tr><th>Bus</th><th>SDA</th><th>SCL</th><th>OE</th><th>Freq I&sup2;C</th><th>Freq PWM</th><th>PCA detected</th><th>Status</th></tr></thead>
        <tbody id="buses-table"><tr><td colspan="8" style="color:var(--fg2)">Loading...</td></tr></tbody>
      </table>
      </div>
      <button class="btn" onclick="scanI2C()">Scan I&sup2;C bus</button>
    </div>
  </div>

  <!-- Config -->
  <div class="section-title" style="margin-top:24px">Configuration</div>
  <div class="btn-row">
    <button class="btn primary" onclick="saveConfig()">Save to flash</button>
    <button class="btn" onclick="exportConfig()">Export &darr;</button>
    <button class="btn" onclick="document.getElementById('import-file').click()">Import &uarr;</button>
    <button class="btn danger" onclick="confirmResetDefaults()">Reset</button>
    <input type="file" id="import-file" accept="application/json,.json" style="display:none" onchange="importConfig(this)">
  </div>
  <div class="sub" style="margin-top:8px">Config version: <span id="set-version">-</span></div>
  <div class="sub" style="margin-top:4px">Firmware: <span id="set-fw">-</span> &middot; build <span id="set-build">-</span></div>
</div>

<!-- ============ MIDI (MIDI Inputs + Received Messages) ============ -->
<div class="page" id="page-midi">
  <div class="section-title">MIDI Inputs</div>
  <p style="color:var(--fg2);font-size:12px;margin-bottom:12px">Choose how PlayMode receives MIDI messages. Multiple inputs can be active at the same time.</p>
  <div class="cards" style="margin-bottom:16px">
    <div class="card">
      <h3>MIDI Cable (DIN / TRS)</h3>
      <label><input type="checkbox" id="midi-serial" onchange="updateMidiConfig()"> Active</label>
      <div class="sub">Classic wired connection via 5-pin MIDI jack or TRS jack.</div>
      <div class="sub" style="margin-top:4px;font-size:11px;color:var(--fg2)">GPIO <span id="midi-rx-pin">4</span> &mdash; 31250 baud</div>
    </div>
    <div class="card">
      <h3>WiFi &mdash; direct send (UDP)</h3>
      <label><input type="checkbox" id="midi-udp" onchange="updateMidiConfig()"> Active</label>
      <div class="sub">Send MIDI messages from software to PlayMode's IP address. Simple but without synchronization.</div>
      <div class="sub" style="margin-top:4px;font-size:11px;color:var(--fg2)">Port <span id="midi-udp-port">5004</span> &mdash; Requires WiFi</div>
    </div>
    <div class="card">
      <h3>WiFi &mdash; Apple / RTP-MIDI</h3>
      <label><input type="checkbox" id="midi-rtp" onchange="updateMidiConfig()"> Active</label>
      <div class="sub">Compatible with macOS, iOS, rtpMIDI (Windows). Appears automatically in MIDI software. Synchronized.</div>
      <div class="sub" style="margin-top:4px;font-size:11px;color:var(--fg2)">Port <span id="midi-rtp-port">5004</span> &mdash; Requires WiFi</div>
    </div>
    <div class="card">
      <h3>Network Delay</h3>
      <div class="val"><span id="midi-jitter-val">30</span><span class="unit">ms</span></div>
      <input type="range" id="midi-jitter" min="10" max="80" value="30" style="width:100%;margin-top:8px"
        oninput="document.getElementById('midi-jitter-val').textContent=this.value"
        onchange="updateMidiConfig()">
      <div class="help">Fixed hold delay for network MIDI (UDP / RTP) that absorbs arrival-time jitter. It does not reorder messages, so it cannot fix genuinely out-of-order delivery.</div>
    </div>
  </div>

  <!-- Latest received MIDI messages -->
  <div class="section-title" style="margin-top:24px"><span>Received MIDI messages</span>
    <div style="display:flex;gap:6px;align-items:center">
      <label style="font-size:12px;display:flex;align-items:center;gap:4px;color:var(--fg2)"><input type="checkbox" id="midi-log-pause"> Pause</label>
      <button class="btn sm" onclick="clearMidiLog()">Clear</button>
    </div>
  </div>
  <div class="table-responsive" style="max-height:320px;overflow-y:auto" id="midi-log-scroll">
  <table>
    <thead><tr><th style="width:70px">Time</th><th>Source</th><th>Channel</th><th>Type</th><th>Data</th><th>Routed</th></tr></thead>
    <tbody id="midi-log-table"><tr><td colspan="6" style="color:var(--fg2)">Waiting for MIDI messages...</td></tr></tbody>
  </table>
  </div>
  <div id="midi-log-count" style="color:var(--fg2);font-size:11px;margin-top:4px;text-align:right"></div>
</div>

<!-- (Config is now in the unified Settings page) -->

<!-- ============ MODALS ============ -->

<!-- Modal Confirm (replaces native confirm/alert) -->
<div class="modal-overlay" id="modal-confirm">
  <div class="modal" style="max-width:380px;text-align:center">
    <div id="confirm-icon" style="font-size:32px;margin-bottom:8px"></div>
    <h2 id="confirm-title" style="text-align:center">Confirm</h2>
    <p id="confirm-message" style="color:var(--fg2);margin-bottom:20px;white-space:pre-line"></p>
    <div class="btn-row" id="confirm-buttons" style="justify-content:center"></div>
  </div>
</div>

<!-- Modal Instrument -->
<div class="modal-overlay" id="modal-instrument">
  <div class="modal">
    <h2 id="modal-inst-title">New instrument</h2>
    <div class="form-group">
      <label>Instrument name</label>
      <input type="text" id="mi-name" maxlength="31" placeholder="E.g.: Xylophone, Snare drum...">
      <div class="help">Free-form name to identify this instrument in the interface</div>
    </div>
    <div class="form-row">
      <div class="form-group">
        <label>MIDI Channel (0=Omni, 1-16)</label>
        <input type="number" id="mi-channel" min="0" max="16" value="0">
        <div class="help">0 = listens on all channels, 1-16 = specific channel</div>
      </div>
      <div class="form-group">
        <label>Bus I&sup2;C</label>
        <select id="mi-bus"><option value="0">Bus 0 (Servos)</option><option value="1">Bus 1 (Solenoids)</option></select>
        <div class="help">Physical bus to which the actuators are connected</div>
      </div>
    </div>
    <div class="form-row">
      <div class="form-group">
        <label>Latency (ms)</label>
        <input type="number" id="mi-latency" value="10" min="0" max="500">
        <div class="help">Compensation delay between MIDI reception and triggering</div>
      </div>
      <div class="form-group">
        <label>Microphone auto-calibration</label>
        <select id="mi-autocal"><option value="0">No</option><option value="1">Yes</option></select>
        <div class="help">Automatic latency measurement via I&sup2;S microphone</div>
      </div>
    </div>
    <div class="btn-row">
      <button class="btn primary" onclick="saveInstrument()">Save</button>
      <button class="btn" onclick="closeModal('modal-instrument')">Cancel</button>
    </div>
  </div>
</div>

<!-- Modal Actuator -->
<div class="modal-overlay" id="modal-actuator">
  <div class="modal">
    <h2 id="modal-act-title">New actuator</h2>
    <div class="form-row">
      <div class="form-group">
        <label>Actuator ID</label>
        <input type="number" id="ma-id" min="0" max="127" value="0" readonly>
        <div class="help">Unique identifier (0-127), assigned automatically to the first free slot</div>
      </div>
      <div class="form-group">
        <label>Actuator type</label>
        <select id="ma-type" onchange="toggleActuatorFields()"><option value="0">Servo motor</option><option value="1">Solenoid</option></select>
        <div class="help">Servo = rotary motion | Solenoid = linear strike</div>
      </div>
    </div>
    <div class="form-row">
      <div class="form-group">
        <label>Bus I&sup2;C</label>
        <select id="ma-bus"><option value="0">Bus 0</option><option value="1">Bus 1</option></select>
        <div class="help">Physical I&sup2;C bus (PWM frequency configurable in MIDI &gt; I&sup2;C)</div>
      </div>
      <div class="form-group">
        <label>PCA9685 board</label>
        <select id="ma-pca"><option value="64">0x40 (board 1)</option><option value="65">0x41 (board 2)</option><option value="66">0x42 (board 3)</option><option value="67">0x43 (board 4)</option></select>
        <div class="help">I&sup2;C address of the PWM board (16 channels each)</div>
      </div>
    </div>
    <div class="form-group">
      <label>PCA Channel (0-15)</label>
      <input type="number" id="ma-ch" min="0" max="15" value="0">
      <div class="help">PWM output on the board (auto-incremented)</div>
    </div>

    <div class="expert-section">
      <button type="button" class="expert-toggle" onclick="toggleExpert(this)">Advanced settings</button>
      <div class="expert-body">
        <div class="form-group">
          <label>Latency (ms)</label>
          <input type="number" id="ma-latency" min="0" max="500" value="10">
          <div class="help">Mechanical compensation delay (default: 10ms)</div>
        </div>
      </div>
    </div>

    <!-- Servo fields -->
    <div id="servo-fields">
      <div style="border-top:1px solid var(--border);margin:12px 0;padding-top:12px">
        <div style="font-size:13px;font-weight:600;margin-bottom:8px">Servo settings</div>
      </div>
      <div class="form-row">
        <div class="form-group">
          <label>Play mode</label>
          <select id="ma-servo-behavior" onchange="toggleServoDirection()"><option value="0">Strike (quick back-and-forth)</option><option value="1">Alternate (A/B toggle)</option><option value="2">Strum (continuous motion)</option><option value="3">Key (hold down)</option></select>
          <div class="help">Strike: percussion | Alternate: toggle between 2 positions | Strum: back-and-forth | Key: hold</div>
        </div>
        <div class="form-group" id="servo-direction-group">
          <label>Strike direction</label>
          <select id="ma-hit-reverse" onchange="updateAnglePreview()">
            <option value="0">Clockwise (+) &mdash; rest &rarr; rest + amplitude</option>
            <option value="1">Counter-clockwise (&minus;) &mdash; rest &rarr; rest &minus; amplitude</option>
          </select>
          <div class="help">Direction of motion relative to the rest angle</div>
        </div>
      </div>
      <div id="servo-standard-fields">
        <div class="form-row">
          <div class="form-group">
            <label>Rest angle (&deg;)</label>
            <input type="number" id="ma-angle-init" min="0" max="180" value="90" oninput="updateAnglePreview()">
            <div class="help">Arm position at rest (0&deg; to 180&deg;)</div>
          </div>
          <div class="form-group">
            <label>Amplitude (&deg;)</label>
            <input type="number" id="ma-amplitude" min="0" max="180" value="45" oninput="updateAnglePreview()">
            <div class="help">Range of motion in degrees</div>
          </div>
        </div>
      </div>
      <div id="servo-alterne-fields" style="display:none">
        <div class="form-row">
          <div class="form-group">
            <label>Angle A (&deg;)</label>
            <input type="number" id="ma-angle-a-alt" min="0" max="180" value="90" oninput="updateAnglePreview()">
            <div class="help">First toggle position</div>
          </div>
          <div class="form-group">
            <label>Angle B (&deg;)</label>
            <input type="number" id="ma-angle-b" min="0" max="180" value="120" oninput="updateAnglePreview()">
            <div class="help">Second toggle position</div>
          </div>
        </div>
      </div>
      <div class="form-group">
        <label>Movement duration (ms)</label>
        <input type="number" id="ma-speed" min="10" max="2000" value="150">
        <div class="help">Time for a single stroke (10=fast, 500=slow)</div>
      </div>
      <!-- Angle visual preview -->
      <div class="angle-preview" id="angle-preview"></div>
    </div>

    <!-- Solenoid fields -->
    <div id="solenoid-fields" style="display:none">
      <div style="border-top:1px solid var(--border);margin:12px 0;padding-top:12px">
        <div style="font-size:13px;font-weight:600;margin-bottom:8px">Solenoid settings</div>
      </div>
      <div class="form-group">
        <label>Strike mode</label>
        <select id="ma-sol-behavior" onchange="toggleHitHoldFields()"><option value="0">Strike (short pulse)</option><option value="1">Hit-and-Hold (strike then hold)</option></select>
        <div class="help">Strike: short pulse | Hit-and-Hold: strong strike then soft hold</div>
      </div>
      <div class="form-row">
        <div class="form-group">
          <label>Min pulse (ms) &mdash; velocity 1</label>
          <input type="number" id="ma-pulse-min" min="2" max="100" value="5">
          <div class="help">Shortest duration (low velocity, soft strike)</div>
        </div>
        <div class="form-group">
          <label>Max pulse (ms) &mdash; velocity 127</label>
          <input type="number" id="ma-pulse-max" min="5" max="200" value="30">
          <div class="help">Longest duration (high velocity, hard strike)</div>
        </div>
      </div>
      <div class="form-row">
        <div class="form-group">
          <label>Attack PWM (0-4095)</label>
          <input type="number" id="ma-pwm-init" min="0" max="4095" value="4095">
          <div class="help">Initial strike power. 4095 = maximum</div>
        </div>
      </div>
      <div id="hit-hold-fields" class="expert-section" style="display:none">
        <button type="button" class="expert-toggle" onclick="toggleExpert(this)">Hit-and-Hold (advanced)</button>
        <div class="expert-body">
          <div class="form-row">
            <div class="form-group">
              <label>Hold PWM (0-4095)</label>
              <input type="number" id="ma-pwm-hold" min="0" max="4095" value="2048">
              <div class="help">Reduced power after the strike</div>
            </div>
            <div class="form-group">
              <label>Transition ramp (ms)</label>
              <input type="number" id="ma-ramp" min="10" max="500" value="50">
              <div class="help">Duration of attack &rarr; hold transition</div>
            </div>
          </div>
        </div>
      </div>
    </div>

    <div class="btn-row">
      <button class="btn primary" onclick="saveActuator()">Save</button>
      <button class="btn" onclick="closeModal('modal-actuator')">Cancel</button>
    </div>
  </div>
</div>

<!-- Modal Add/Edit CC Mapping -->
<div class="modal-overlay" id="modal-cc">
  <div class="modal" style="max-width:460px">
    <h2 id="modal-cc-title">Add CC Mapping</h2>
    <div class="form-row">
      <div class="form-group">
        <label>CC Number (0-127)</label>
        <input type="number" id="cc-num" min="0" max="127" value="1">
        <div class="help">E.g.: CC1 = Modulation, CC7 = Volume, CC11 = Expression</div>
      </div>
      <div class="form-group">
        <label>Control type</label>
        <select id="cc-category" class="form-select" onchange="toggleCCCategory()">
          <option value="position">Servo position (direct movement)</option>
          <option value="modifier">Modifier (amplitude / speed)</option>
        </select>
      </div>
    </div>

    <!-- Category: Position — free servo -->
    <div id="cc-cat-position">
      <div class="form-group">
        <label>Dedicated servo</label>
        <select id="cc-actuator-free" class="form-select"></select>
        <div class="help">Only servos not assigned to an instrument are listed</div>
      </div>
      <div id="cc-create-servo-hint" style="display:none;margin:-4px 0 10px">
        <span style="color:var(--yellow);font-size:12px">No free servo.</span>
        <button class="btn sm" onclick="quickCreateServoForCC()" style="margin-left:6px">Create a servo</button>
      </div>
      <div class="form-row">
        <div class="form-group">
          <label>Angle min (&deg;)</label>
          <input type="number" id="cc-pos-min" min="0" max="180" value="0">
          <div class="help">Position when CC = 0</div>
        </div>
        <div class="form-group">
          <label>Angle max (&deg;)</label>
          <input type="number" id="cc-pos-max" min="0" max="180" value="180">
          <div class="help">Position when CC = 127</div>
        </div>
      </div>
    </div>

    <!-- Category: Modifier — instrument servo -->
    <div id="cc-cat-modifier" style="display:none">
      <div class="form-group">
        <label>Instrument servo</label>
        <select id="cc-actuator-inst" class="form-select" onchange="updateCCServoInfo()"></select>
        <div class="help">Servos assigned to the selected instrument</div>
      </div>
      <div id="cc-servo-info" style="display:none;background:var(--bg3);border-radius:6px;padding:8px 10px;margin-bottom:10px;font-size:12px;color:var(--fg2)"></div>
      <div class="form-group">
        <label>Parameter to modify</label>
        <select id="cc-mod-target" class="form-select" onchange="updateCCModRangeHints()">
          <option value="1">Amplitude &mdash; strike range (&deg;)</option>
          <option value="2">Speed &mdash; movement duration (ms)</option>
        </select>
        <div class="help" id="cc-mod-help">Modifies the strike range in real time for upcoming notes</div>
      </div>
      <div class="form-row">
        <div class="form-group">
          <label>Min value <span id="cc-mod-min-unit">(&deg;)</span></label>
          <input type="number" id="cc-mod-min" value="0">
          <div class="help">Value when CC = 0</div>
        </div>
        <div class="form-group">
          <label>Max value <span id="cc-mod-max-unit">(&deg;)</span></label>
          <input type="number" id="cc-mod-max" value="180">
          <div class="help">Value when CC = 127</div>
        </div>
      </div>
    </div>

    <div class="btn-row">
      <button class="btn primary" id="cc-save-btn" onclick="saveCC()">Add</button>
      <button class="btn" onclick="closeModal('modal-cc')">Cancel</button>
    </div>
  </div>
</div>

<!-- Wizard Instrument -->
<div class="modal-overlay" id="modal-wizard">
  <div class="modal" style="max-width:550px">
    <h2>Instrument creation wizard</h2>
    <div class="wiz-steps" id="wiz-steps">
      <div class="wiz-step-item active" id="wiz-item-1"><div class="wiz-dot active" id="wiz-dot-1">1</div><div class="wiz-step-label">Identity</div></div>
      <div class="wiz-connector" id="wiz-conn-1"></div>
      <div class="wiz-step-item" id="wiz-item-2"><div class="wiz-dot" id="wiz-dot-2">2</div><div class="wiz-step-label">Type</div></div>
      <div class="wiz-connector" id="wiz-conn-2"></div>
      <div class="wiz-step-item" id="wiz-item-3"><div class="wiz-dot" id="wiz-dot-3">3</div><div class="wiz-step-label">Notes</div></div>
      <div class="wiz-connector" id="wiz-conn-3"></div>
      <div class="wiz-step-item" id="wiz-item-4"><div class="wiz-dot" id="wiz-dot-4">4</div><div class="wiz-step-label">Creation</div></div>
    </div>

    <!-- Step 1: Identity -->
    <div id="wiz-step-1" class="wiz-panel">
      <div style="font-size:14px;font-weight:600;margin-bottom:12px">Instrument identity</div>
      <div class="form-group">
        <label>Name</label>
        <input type="text" id="wiz-name" maxlength="31" placeholder="E.g.: Xylophone, Snare drum...">
      </div>
      <div class="form-group">
        <label>MIDI Channel (0=Omni, 1-16)</label>
        <input type="number" id="wiz-channel" min="0" max="16" value="1">
        <div class="help">0 = all channels, 1-16 = specific channel</div>
      </div>
    </div>

    <!-- Step 2: Actuator type -->
    <div id="wiz-step-2" class="wiz-panel" style="display:none">
      <div style="font-size:14px;font-weight:600;margin-bottom:12px">Actuator type</div>
      <div class="form-group">
        <label>Type</label>
        <select id="wiz-type" onchange="wizUpdateBehaviors()">
          <option value="0">Servo motor (rotary motion)</option>
          <option value="1">Solenoid (linear strike)</option>
        </select>
        <div class="help">All actuators will use this type</div>
      </div>
      <div class="form-group">
        <label>Play mode</label>
        <select id="wiz-behavior"></select>
        <div class="help">Default behavior for all actuators</div>
      </div>
    </div>

    <!-- Step 3: Notes + PCA -->
    <div id="wiz-step-3" class="wiz-panel" style="display:none">
      <div style="font-size:14px;font-weight:600;margin-bottom:12px">MIDI note assignment</div>
      <div class="form-row tri">
        <div class="form-group">
          <label>Actuators</label>
          <input type="number" id="wiz-count" min="1" max="64" value="8" oninput="wizBuildNoteTable()">
        </div>
        <div class="form-group">
          <label>Scale</label>
          <select id="wiz-scale" onchange="wizBuildNoteTable()">
            <option value="chromatic">Chromatic (semitones)</option>
            <option value="major">Major (C D E F G A B)</option>
            <option value="pentatonic">Pentatonic (5 notes)</option>
          </select>
        </div>
        <div class="form-group">
          <label>Start note</label>
          <input type="number" id="wiz-start-note" min="0" max="127" value="48" oninput="wizBuildNoteTable()">
          <div class="help">C3=48, C4=60</div>
        </div>
      </div>
      <div class="help" style="margin-bottom:4px">Edit each note individually if needed:</div>
      <div class="wiz-note-table" id="wiz-note-table"></div>
      <div class="expert-section">
        <button type="button" class="expert-toggle" onclick="toggleExpert(this)">Advanced PCA settings</button>
        <div class="expert-body">
          <div class="form-row">
            <div class="form-group">
              <label>Starting PCA board</label>
              <select id="wiz-pca">
                <option value="64">0x40 (board 1)</option>
                <option value="65">0x41 (board 2)</option>
                <option value="66">0x42 (board 3)</option>
                <option value="67">0x43 (board 4)</option>
              </select>
            </div>
            <div class="form-group">
              <label>Starting PCA channel</label>
              <input type="number" id="wiz-start-ch" min="0" max="15" value="0">
              <div class="help">Auto-incremented</div>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- Step 4: Review -->
    <div id="wiz-step-4" class="wiz-panel" style="display:none">
      <div style="font-size:14px;font-weight:600;margin-bottom:12px">Summary</div>
      <div id="wiz-summary" style="background:var(--bg);border:1px solid var(--border);border-radius:var(--radius);padding:12px;font-size:13px;line-height:1.8"></div>
    </div>

    <div class="btn-row" style="margin-top:16px">
      <button class="btn" id="wiz-prev" onclick="wizPrev()" style="display:none">&larr; Previous</button>
      <button class="btn primary" id="wiz-next" onclick="wizNext()">Next &rarr;</button>
      <button class="btn" onclick="closeModal('modal-wizard')">Cancel</button>
    </div>
  </div>
</div>

<!-- ============ CALIBRATION (Actuators + CC + Acoustic Calibration) ============ -->
<!-- ============ ACTUATORS (table + CC mapping) ============ -->
<div class="page" id="page-actuators">

  <!-- === Actuators === -->
  <div class="section-title"><span>Actuators</span>
    <button class="btn primary sm" onclick="openActuatorModal()">+ Add</button>
  </div>
  <div class="table-responsive">
  <table>
    <thead><tr><th>ID</th><th>Type</th><th>MIDI Note</th><th>Bus</th><th>PCA</th><th>Ch</th><th>Mode</th><th>Status</th><th>Actions</th></tr></thead>
    <tbody id="actuators-table"><tr><td colspan="9" style="color:var(--fg2)">Loading...</td></tr></tbody>
  </table>
  </div>

  <!-- === CC Mapping === -->
  <div class="section-title" style="margin-top:20px"><span>Control Changes (CC)</span>
    <button class="btn primary sm" onclick="openAddCCModal()">+ Add CC</button>
  </div>
  <div style="margin-bottom:8px">
    <select id="cc-instrument" onchange="loadCCRouting()" class="form-select" style="max-width:250px"></select>
  </div>
  <p style="color:var(--fg2);font-size:12px;margin-bottom:8px">Position: moves a dedicated servo. Modifier: adjusts amplitude or speed of an instrument servo.</p>
  <div class="table-responsive">
  <table>
    <thead><tr><th>CC#</th><th>Type</th><th>Servo</th><th>Action</th><th>Range</th><th>Actions</th></tr></thead>
    <tbody id="mapping-cc-table"><tr><td colspan="6" style="color:var(--fg2)">Select an instrument</td></tr></tbody>
  </table>
  </div>
</div>

<!-- ============ WIRING (electrical diagram generated from the configuration) ============ -->
<div class="page" id="page-wiring">

  <div class="section-title"><span>Electrical wiring</span>
    <div style="display:flex;gap:8px">
      <button class="btn sm" onclick="loadWiring()" title="Rebuild from the current configuration">&#8635; Refresh</button>
      <button class="btn primary sm" onclick="downloadWiringSVG()" title="Download the diagram as an SVG file">&#8681; SVG</button>
    </div>
  </div>
  <p class="wire-note">Generated from the live configuration (I&sup2;C buses + actuators).
    Every board, channel and signal drawn below is what the firmware will actually drive &mdash;
    print it or keep it open on the bench while you wire the machine.</p>

  <div class="wire-wrap">
    <div class="wire-scroll" id="wiring-diagram"><div class="wire-empty">Loading&hellip;</div></div>
    <div class="wire-legend" id="wiring-legend"></div>
  </div>

  <div class="cards" id="wiring-cards"></div>

  <div class="section-title">Boards</div>
  <div class="table-responsive">
  <table>
    <thead><tr><th>Bus</th><th>Board</th><th>Type</th><th>Instruments</th><th>Channels</th><th>Power rail</th></tr></thead>
    <tbody id="wiring-boards"><tr><td colspan="6" style="color:var(--fg2)">Loading...</td></tr></tbody>
  </table>
  </div>

  <div class="section-title">Wiring checks</div>
  <div id="wiring-issues"></div>

  <div class="section-title">Pinout</div>
  <div class="table-responsive">
  <table>
    <thead><tr><th>Signal</th><th>GPIO</th><th>Direction</th><th>Notes</th></tr></thead>
    <tbody id="wiring-pins"><tr><td colspan="4" style="color:var(--fg2)">Loading...</td></tr></tbody>
  </table>
  </div>

  <div class="section-title">Power distribution</div>
  <div id="wiring-power"></div>

  <div class="section-title">Commissioning &mdash; power up in stages</div>
  <ul class="wire-check">
    <li><b>Logic only.</b> ESP32 powered from USB, actuator supplies OFF. Check the web UI answers and the state chip in the header reads <em>Disarmed</em>.</li>
    <li><b>I&sup2;C scan.</b> Settings &rarr; Buses &rarr; <em>Scan I&sup2;C</em>: every PCA9685 address above must be detected. A missing address = wrong A0&ndash;A2 jumpers or missing pull-ups.</li>
    <li><b>/OE check.</b> With the outputs disarmed, the /OE pin must be HIGH (outputs disabled). Keep the 10&nbsp;k&Omega; pull-up to V<sub>logic</sub> so the boards stay off during the ESP32 boot.</li>
    <li><b>One actuator.</b> Connect a single servo/solenoid, arm the outputs, trigger it from the virtual piano. Confirm the mechanical travel before wiring the rest.</li>
    <li><b>Full rail.</b> Connect the remaining actuators, current-limit the bench supply to the estimated peak above, then arm.</li>
    <li><b>Kill switch.</b> Verify the header Kill button (and any external E-stop on /OE) cuts every output instantly.</li>
  </ul>
</div>

<!-- ============ ACOUSTIC CALIBRATION (visible only if microphone present) ============ -->
<div class="page" id="page-calibration">
  <div class="section-title">Acoustic Calibration</div>
  <div class="mic-status" id="mic-status">Checking microphone...</div>
  <div id="cal-controls">
    <div class="cards" style="grid-template-columns:repeat(auto-fit,minmax(140px,1fr))">
      <div class="card">
        <h3>Status</h3>
        <div class="val" id="cal-state" style="font-size:18px">Inactive</div>
      </div>
      <div class="card">
        <h3>Progress</h3>
        <div class="val"><span id="cal-progress">0</span><span class="unit">%</span></div>
        <div class="bar"><div class="bar-fill" id="cal-bar" style="width:0%;background:var(--accent)"></div></div>
      </div>
      <div class="card">
        <h3>Actuator</h3>
        <div class="val" id="cal-cur-act">&mdash;</div>
      </div>
      <div class="card">
        <h3>Results</h3>
        <div class="val"><span id="cal-result-count">0</span><span class="unit">measurements</span></div>
      </div>
    </div>
    <div class="btn-row" style="margin:16px 0">
      <button class="btn primary sm" onclick="startCalibrateAll()">&#9654; Calibrate all</button>
      <button class="btn sm" onclick="startCalibrateOne()" id="cal-btn-one">Calibrate one...</button>
      <button class="btn danger sm" onclick="stopCalibration()" id="cal-btn-stop" style="display:none">Stop</button>
      <button class="btn sm" onclick="applyCalibrateResults()" id="cal-btn-apply" style="display:none">&#10003; Apply</button>
      <button class="btn sm" onclick="loadCalibrateResults()" style="margin-left:auto">&#8635;</button>
    </div>
    <div id="cal-single-sel" style="display:none;margin-bottom:12px">
      <label style="font-size:13px;margin-right:8px">Actuator:</label>
      <select id="cal-act-select" class="form-select"></select>
      <button class="btn primary sm" style="margin-left:8px" onclick="confirmCalibrateOne()">Go</button>
      <button class="btn sm" style="margin-left:4px" onclick="document.getElementById('cal-single-sel').style.display='none'">&#10005;</button>
    </div>
    <div class="table-responsive">
    <table>
      <thead><tr><th>ID</th><th>Type</th><th>Current latency</th><th>Measured latency</th><th>Measurements</th><th>Status</th></tr></thead>
      <tbody id="cal-results-table"><tr><td colspan="6" style="color:var(--fg2);text-align:center">Start a calibration</td></tr></tbody>
    </table>
    </div>
  </div>
</div>

<!-- (Logs are now in the unified Settings page) -->

<script>
// ============================================================================
// State
// ============================================================================
let ws = null;
let wsConnected = false;
let currentPage = 'instrument';
let instruments = [];
let actuators = [];
let routing = [];
let pianoNotes = {}; // instIndex -> { note -> actuator_id }
let pressedKeys = {}; // "instIdx-note" -> true (keys currently held down by user)
let editingInstrumentIdx = -1;
let editingActuatorId = -1;

const SERVO_BEHAVIORS = ['Strike','Alternate','Strum','Key'];
const SOL_BEHAVIORS = ['Strike','Hit-and-Hold'];
const CC_TARGETS = ['Position (\u00b0)','Amplitude (\u00b0)','Speed (ms)','PWM hold'];
const CC_TARGET_UNITS = ['\u00b0','\u00b0','ms',''];
const CC_TARGET_RANGES = [[0,180],[0,180],[10,2000],[0,4095]];
const NOTE_NAMES = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
const SCALES = {
  chromatic:{intervals:[0,1,2,3,4,5,6,7,8,9,10,11]},
  major:{intervals:[0,2,4,5,7,9,11]},
  pentatonic:{intervals:[0,2,4,7,9]}
};

// Log Manager (Phase 9)
let logCache = [];
let logLastCount = 0;
let logPollInterval = null;

function noteName(n) { return NOTE_NAMES[n%12] + Math.floor(n/12-1); }

// ============================================================================
// WebSocket
// ============================================================================
function connectWS() {
  const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
  ws = new WebSocket(proto + '//' + location.host + '/ws');

  ws.onopen = () => {
    wsConnected = true;
    // AUDIT FIX: reset log counter to -1 to force a refresh
    // on the first message (handles case of server restart + already connected client)
    logLastCount = -1;
    const dot = document.getElementById('ws-dot');
    if (dot) dot.className = 'dot';
    el('ws-status', 'Connected');   // AUDIT FIX (UI-P0): null-safe, was crashing
  };

  ws.onclose = () => {
    wsConnected = false;
    const dot = document.getElementById('ws-dot');
    if (dot) dot.className = 'dot off';
    // AUDIT FIX (UI-P0): use the null-safe helper — a missing #ws-status used to
    // throw here BEFORE the reconnect was scheduled, so it never reconnected.
    el('ws-status', 'Disconnected');
    setTimeout(connectWS, 2000);
  };

  ws.onerror = () => { ws.close(); };

  ws.onmessage = (evt) => {
    try {
      const d = JSON.parse(evt.data);
      if (d.machine_state) updateMachineState(d.machine_state);
      updateDashboard(d);
      // MIDI messages received via WebSocket
      if (d.midi_msg) pushMidiLog(d.midi_msg);
      if (d.midi_msgs) for (const m of d.midi_msgs) pushMidiLog(m);
      // Refresh logs if new entries detected
      if (d.log_count !== undefined && d.log_count !== logLastCount) {
        logLastCount = d.log_count;
        if (currentPage === 'settings') loadLogs();
      }
    } catch(e) {}
  };
}

// AUDIT FIX (UX): permanent machine-state indicator + arm/kill control.
let currentMachineState = 'unknown';
function updateMachineState(state) {
  currentMachineState = state;
  const chip = document.getElementById('machine-state');
  const btn = document.getElementById('arm-btn');
  if (!chip) return;
  const labels = { armed: 'Armed', disarmed: 'Disarmed', fault: 'Fault' };
  chip.textContent = labels[state] || '—';
  chip.className = 'mstate mstate-' + (labels[state] ? state : 'unknown');
  if (btn) {
    btn.style.display = '';
    if (state === 'armed') {
      btn.textContent = 'Kill'; btn.className = 'btn sm danger';
    } else if (state === 'disarmed') {
      btn.textContent = 'Arm'; btn.className = 'btn sm primary';
    } else { // fault
      btn.textContent = 'Clear'; btn.className = 'btn sm';
    }
  }
}

async function toggleArm() {
  if (currentMachineState === 'armed') {
    if (!await appConfirm('Disable outputs', 'Cut all outputs now (kill switch)?',
        {danger:true, confirmText:'Kill', icon:'🛑'})) return;
    await api('/api/killswitch', 'POST', {active: true});
  } else if (currentMachineState === 'fault') {
    // AUDIT FIX (P0.1): a latched fault must be ACKNOWLEDGED first (this does
    // NOT re-arm). The state then becomes "disarmed" and a separate Arm press
    // re-enables the outputs.
    if (!await appConfirm('Acknowledge fault',
        'Acknowledge the latched fault? Fix the cause first. Outputs stay OFF until you arm them.',
        {danger:true, confirmText:'Acknowledge', icon:'⚠️'})) return;
    await api('/api/fault/ack', 'POST');
  } else {
    // disarmed → arm
    if (!await appConfirm('Arm outputs', 'Re-enable the outputs? Make sure the machine is clear.',
        {confirmText:'Arm', icon:'⚡'})) return;
    await api('/api/killswitch', 'POST', {active: false});
  }
}

function updateDashboard(d) {
  // AUDIT FIX (UX): firmware identity (set once when present).
  if (d.fw_version) { el('set-fw', d.fw_version); el('set-build', d.fw_build || '-'); }

  // MIDI Transport
  // AUDIT FIX (UI-P1): do NOT sum incompatible units (serial bytes + UDP
  // packets + RTP messages). Show each source separately with its unit.
  if (d.midi) {
    el('d-midi-recv', 'cable ' + (d.midi.serial_bytes || 0) + ' B / UDP '
        + (d.midi.udp_packets || 0) + ' pkt / RTP ' + (d.midi.rtp_packets || 0) + ' msg');
    el('d-midi-routed', d.dispatcher ? d.dispatcher.dispatched : 0);
    el('d-midi-unmapped', d.dispatcher ? d.dispatcher.dropped : 0);
  }
  if (d.dispatcher) {
    el('p-rejected', d.dispatcher.pwr_rejected || 0);
  }

  // Scheduler
  if (d.scheduler) {
    el('d-sched-queued', d.scheduler.queued || 0);
    el('d-sched-processed', d.scheduler.processed || 0);
  }

  // Power + Polyphony (unified card)
  if (d.power || d.safety) {
    const active = d.power?.active_count || d.safety?.active || 0;
    el('d-active', active);
    const pbar = document.getElementById('p-total-bar');
    if (pbar) {
      const max = d.power?.max_polyphony || 12;
      const pct = max > 0 ? Math.min(100, Math.round(active / max * 100)) : 0;
      pbar.style.width = pct + '%';
      pbar.style.background = pct > 80 ? 'var(--red)' : 'var(--green)';
    }
  }

  // Safety
  if (d.safety) {

    const killEl = document.getElementById('s-kill');
    if (killEl) {
      killEl.textContent = d.safety.kill_switch ? 'ON' : 'OFF';
      killEl.style.color = d.safety.kill_switch ? 'var(--red)' : 'var(--green)';
    }
    const degEl = document.getElementById('s-degrad');
    if (degEl) {
      degEl.textContent = d.safety.degradation ? 'Yes' : 'No';
      degEl.style.color = d.safety.degradation ? 'var(--yellow)' : 'var(--green)';
    }

    // Alerts
    updateAlerts(d);
  }

  // WiFi
  if (d.wifi) {
    el('d-wifi-rssi', d.wifi.rssi ? d.wifi.rssi + ' dBm' : 'N/A');
    el('d-wifi-status', d.wifi.connected ? 'Connected' : 'Disconnected');
  }

  // Update piano active notes
  if (d.active_actuators) {
    updatePianoActive(d.active_actuators);
  }
}

function updateAlerts(d) {
  let html = '';
  if (d.safety && d.safety.kill_switch) {
    html += '<div class="alert danger">KILL SWITCH ACTIVE — All outputs are disabled</div>';
  }
  if (d.safety && d.safety.degradation) {
    html += '<div class="alert warn">DEGRADATION — Approaching safety threshold</div>';
  }
  if (d.power && d.power.degradation) {
    html += '<div class="alert warn">POWER BUDGET — Graceful degradation active</div>';
  }
  document.getElementById('alert-zone').innerHTML = html;

  let safetyHtml = '';
  if (d.safety && d.safety.kill_switch) {
    safetyHtml += '<div class="alert danger">KILL SWITCH ACTIVE</div>';
  }
  const sz = document.getElementById('safety-alert-zone');
  if (sz) sz.innerHTML = safetyHtml;
}

// ============================================================================
// Navigation
// ============================================================================
function showPage(page) {
  currentPage = page;
  document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
  const navEl = document.getElementById('main-nav');
  if (navEl) navEl.querySelectorAll('button').forEach(b => b.classList.remove('active'));

  const pageEl = document.getElementById('page-' + page);
  if (pageEl) pageEl.classList.add('active');

  // Highlight nav button
  if (navEl) navEl.querySelectorAll('button').forEach(b => {
    if (b.onclick && b.onclick.toString().includes("'" + page + "'"))
      b.classList.add('active');
  });

  // Show/hide nav for welcome page
  if (navEl) navEl.style.display = (page === 'welcome') ? 'none' : 'flex';

  // Load data for specific pages
  if (page === 'instrument') {
    loadHomeInstruments(); loadInstrumentSelects(); buildAllPianos();
  }
  if (page === 'midi') { loadMidiConfig(); }
  if (page === 'actuators') {
    loadActuatorsWithNotes(); loadInstrumentSelects(); loadCCRouting();
  }
  if (page === 'wiring') { loadWiring(); }
  if (page === 'calibration') {
    loadCalibrateStatus(); loadCalibrateResults();
  }
  if (page === 'settings') {
    loadPower(); loadSafety(); loadLogs(); loadWiFiConfig(); loadBuses();
  }
}

// ============================================================================
// API helpers
// ============================================================================
// AUDIT FIX (P0.9): in AP mode the device requires an auth token on writes.
// Fetch it once at startup and attach it (header + query fallback) to every
// non-GET request. In STA mode the endpoint reports ap_mode=false and no token
// is needed.
let authToken = null;

async function fetchAuthToken() {
  try {
    const res = await fetch('/api/auth-token');
    const d = await res.json();
    authToken = (d && d.ap_mode && d.token) ? d.token : null;
  } catch (e) {
    authToken = null;
  }
}

// AUDIT FIX (UI-P0): robust helper. Checks res.ok, guards against non-JSON
// bodies, and enforces a timeout. On any failure it surfaces the error (toast)
// and returns null, so callers that test the result treat it as a failure
// instead of silently proceeding as if it succeeded.
async function api(url, method='GET', body=null) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), 8000);
  const opts = { method, headers: {'Content-Type':'application/json'}, signal: controller.signal };
  if (authToken && method !== 'GET') {
    opts.headers['X-PlayMode-Token'] = authToken;
  }
  if (body) opts.body = JSON.stringify(body);
  try {
    const res = await fetch(url, opts);
    const text = await res.text();
    let data = {};
    if (text) { try { data = JSON.parse(text); } catch (e) { data = {}; } }
    if (!res.ok) {
      const msg = (data && data.error) ? data.error : ('HTTP ' + res.status);
      if (typeof toast === 'function') toast('Error: ' + msg, 'error');
      return null;
    }
    return data;
  } catch (e) {
    if (typeof toast === 'function') {
      toast(e.name === 'AbortError' ? 'Request timed out' : 'Network error', 'error');
    }
    return null;
  } finally {
    clearTimeout(timer);
  }
}

function el(id, val) {
  const e = document.getElementById(id);
  if (e) e.textContent = val;
}

// ============================================================================
// Instruments (Home page)
// ============================================================================
async function loadHomeInstruments() {
  instruments = await api('/api/instruments') || [];
  loadHomeInstruments_render();
}

function loadHomeInstruments_render() {
  const tbody = document.getElementById('home-instruments-table');
  if (!tbody) return;
  if (!instruments || instruments.length === 0) {
    tbody.innerHTML = '<tr><td colspan="6" style="color:var(--fg2)">No instrument &mdash; use the wizard to get started</td></tr>';
    return;
  }
  let html = '';
  for (const inst of instruments) {
    const busType = inst.bus_id === 0 ? 'Servos' : 'Solenoids';
    // Compute actuator count from routing if backend sends 0
    let actCount = inst.actuator_count || 0;
    if (actCount === 0 && routing) {
      const r = routing.find(x => x.instrument === inst.index);
      if (r && r.notes) actCount = r.notes.filter(n => n.enabled).length;
    }
    html += '<tr>';
    html += '<td><strong>' + esc(inst.name) + '</strong></td>';
    html += '<td>' + (inst.channel === 0 ? 'Omni' : 'Ch. ' + inst.channel) + '</td>';
    html += '<td>' + busType + '</td>';
    html += '<td>' + actCount + '</td>';
    html += '<td>' + (inst.enabled ? '<span class="badge on">Active</span>' : '<span class="badge off">Inactive</span>') + '</td>';
    html += '<td><button class="btn sm" onclick="editInstrument(' + inst.index + ')">Edit</button> ';
    html += '<button class="btn sm" onclick="deleteInstrument(' + inst.index + ')">Delete</button></td>';
    html += '</tr>';
  }
  tbody.innerHTML = html;
}

function openInstrumentModal() {
  editingInstrumentIdx = -1;
  document.getElementById('mi-name').value = '';
  document.getElementById('mi-channel').value = Math.min(instruments ? instruments.length + 1 : 1, 16);
  document.getElementById('mi-bus').value = '0';
  document.getElementById('mi-latency').value = '10';
  document.getElementById('mi-autocal').value = '0';
  document.getElementById('modal-inst-title').textContent = 'New instrument';
  document.getElementById('modal-instrument').classList.add('show');
}

function editInstrument(idx) {
  const inst = instruments.find(i => i.index === idx);
  if (!inst) return;
  editingInstrumentIdx = idx;
  document.getElementById('mi-name').value = inst.name;
  document.getElementById('mi-channel').value = inst.channel;
  document.getElementById('mi-bus').value = inst.bus_id;
  document.getElementById('mi-latency').value = inst.latency_ms;
  document.getElementById('mi-autocal').value = inst.auto_cal ? '1' : '0';
  document.getElementById('modal-inst-title').textContent = 'Edit ' + inst.name;
  document.getElementById('modal-instrument').classList.add('show');
}

function closeModal(id) {
  document.getElementById(id).classList.remove('show');
}

// Themed confirm/alert modals (replace native confirm/alert)
function appConfirm(title, message, {confirmText='Confirm', cancelText='Cancel', danger=false, icon='\u26a0\ufe0f'}={}) {
  return new Promise(resolve => {
    document.getElementById('confirm-icon').textContent = icon;
    document.getElementById('confirm-title').textContent = title;
    document.getElementById('confirm-message').textContent = message;
    const btns = document.getElementById('confirm-buttons');
    btns.innerHTML = '';
    const btnCancel = document.createElement('button');
    btnCancel.className = 'btn';
    btnCancel.textContent = cancelText;
    btnCancel.onclick = () => { closeModal('modal-confirm'); resolve(false); };
    const btnOk = document.createElement('button');
    btnOk.className = 'btn ' + (danger ? 'danger' : 'primary');
    btnOk.textContent = confirmText;
    btnOk.onclick = () => { closeModal('modal-confirm'); resolve(true); };
    btns.appendChild(btnCancel);
    btns.appendChild(btnOk);
    document.getElementById('modal-confirm').classList.add('show');
  });
}

function appAlert(title, message, {btnText='OK', icon='\u2139\ufe0f'}={}) {
  return new Promise(resolve => {
    document.getElementById('confirm-icon').textContent = icon;
    document.getElementById('confirm-title').textContent = title;
    document.getElementById('confirm-message').textContent = message;
    const btns = document.getElementById('confirm-buttons');
    btns.innerHTML = '';
    const btnOk = document.createElement('button');
    btnOk.className = 'btn primary';
    btnOk.textContent = btnText;
    btnOk.onclick = () => { closeModal('modal-confirm'); resolve(); };
    btns.appendChild(btnOk);
    document.getElementById('modal-confirm').classList.add('show');
  });
}

// AUDIT FIX (P2): explicit instrument picker (replaces the silent "assign to
// instrument 0" behaviour). Resolves to the chosen index, or null if cancelled.
function appSelectInstrument(title, message, options) {
  return new Promise(resolve => {
    document.getElementById('confirm-icon').textContent = '🎹';
    document.getElementById('confirm-title').textContent = title;
    const msg = document.getElementById('confirm-message');
    msg.textContent = message + ' ';
    const sel = document.createElement('select');
    sel.className = 'input';
    sel.style.marginTop = '8px';
    for (const o of options) {
      const opt = document.createElement('option');
      opt.value = o.value;
      opt.textContent = o.label;
      sel.appendChild(opt);
    }
    msg.appendChild(document.createElement('br'));
    msg.appendChild(sel);
    const btns = document.getElementById('confirm-buttons');
    btns.innerHTML = '';
    const btnCancel = document.createElement('button');
    btnCancel.className = 'btn';
    btnCancel.textContent = 'Cancel';
    btnCancel.onclick = () => { closeModal('modal-confirm'); resolve(null); };
    const btnOk = document.createElement('button');
    btnOk.className = 'btn primary';
    btnOk.textContent = 'Assign';
    btnOk.onclick = () => { closeModal('modal-confirm'); resolve(parseInt(sel.value)); };
    btns.appendChild(btnCancel);
    btns.appendChild(btnOk);
    document.getElementById('modal-confirm').classList.add('show');
  });
}

async function saveInstrument() {
  const data = {
    name: document.getElementById('mi-name').value || 'Instrument',
    channel: parseInt(document.getElementById('mi-channel').value),
    bus_id: parseInt(document.getElementById('mi-bus').value),
    latency_ms: parseInt(document.getElementById('mi-latency').value),
    auto_cal: document.getElementById('mi-autocal').value === '1',
    enabled: true
  };
  if (editingInstrumentIdx >= 0) data.index = editingInstrumentIdx;
  const resp = await api('/api/instrument', 'POST', data);
  if (!resp || !resp.ok) return;   // error already surfaced by api()
  closeModal('modal-instrument');
  editingInstrumentIdx = -1;
  instruments = [];  // Force refresh
  routing = await api('/api/routing') || [];
  loadHomeInstruments();
  loadInstrumentSelects();
  buildAllPianos();
  // After creating first instrument, go to instrument page (exit welcome)
  if (currentPage === 'welcome') showPage('instrument');
}

async function deleteInstrument(idx) {
  if (!await appConfirm('Delete instrument', 'This will delete the instrument and its mappings.', {danger:true, icon:'\ud83d\uddd1\ufe0f'})) return;
  await api('/api/instrument?index=' + idx, 'DELETE');
  instruments = [];  // Force refresh
  routing = await api('/api/routing') || [];
  loadHomeInstruments();
  loadInstrumentSelects();
  buildAllPianos();
}

// ============================================================================
// Actuators (with MIDI note display)
// ============================================================================
async function loadActuators() {
  actuators = await api('/api/actuators');
}

function buildActNoteMap() {
  // Build reverse map: actuator_id -> {note, instIdx}
  const map = {};
  if (routing) {
    for (const r of routing) {
      if (r.notes) {
        for (const nm of r.notes) {
          if (nm.enabled) map[nm.actuator] = {note: nm.note, inst: r.instrument};
        }
      }
    }
  }
  return map;
}

async function loadActuatorsWithNotes() {
  await loadActuators();
  if (!routing || routing.length === 0) {
    routing = await api('/api/routing') || [];
  }
  const noteMap = buildActNoteMap();
  const tbody = document.getElementById('actuators-table');
  if (!actuators || actuators.length === 0) {
    tbody.innerHTML = '<tr><td colspan="9" style="color:var(--fg2)">No actuator configured</td></tr>';
    return;
  }
  // Group actuators by instrument
  const groups = {};
  const unassigned = [];
  for (const act of actuators) {
    const nm = noteMap[act.id];
    if (nm) {
      if (!groups[nm.inst]) groups[nm.inst] = [];
      groups[nm.inst].push(act);
    } else {
      unassigned.push(act);
    }
  }
  let html = '';
  function renderRow(act) {
    const isServo = act.type === 0;
    const behaviors = isServo ? SERVO_BEHAVIORS : SOL_BEHAVIORS;
    const nm = noteMap[act.id];
    html += '<tr>';
    html += '<td>' + act.id + '</td>';
    html += '<td>' + (isServo ? '<span class="badge servo">Servo</span>' : '<span class="badge sol">Solenoid</span>') + '</td>';
    html += '<td><input class="note-input" type="number" min="0" max="127" value="' + (nm ? nm.note : '') + '" placeholder="-" '
      + 'onchange="setActuatorNote(' + act.id + ',this.value)">';
    if (nm) html += '<span class="note-label">' + noteName(nm.note) + '</span>';
    html += '</td>';
    html += '<td>Bus ' + act.bus_id + '</td>';
    html += '<td>0x' + act.pca_addr.toString(16).toUpperCase() + '</td>';
    html += '<td>' + act.pca_ch + '</td>';
    html += '<td>' + (behaviors[act.behavior] || '?') + '</td>';
    html += '<td>' + (act.state && act.state.active ? '<span class="badge on">Active</span>' : '<span class="badge off">Idle</span>') + '</td>';
    html += '<td><button class="btn sm" onclick="editActuator(' + act.id + ')">Edit</button> ';
    html += '<button class="btn sm" onclick="testActuator(' + act.id + ')">Test</button> ';
    html += '<button class="btn sm" onclick="deleteActuator(' + act.id + ')">Delete</button></td>';
    html += '</tr>';
  }
  // Render each instrument group
  for (const inst of (instruments || [])) {
    const acts = groups[inst.index];
    if (!acts || acts.length === 0) continue;
    html += '<tr class="inst-separator"><td colspan="9">' + esc(inst.name) + ' <span style="font-weight:400;color:var(--fg2)">(' + acts.length + ')</span></td></tr>';
    for (const act of acts) renderRow(act);
  }
  // Unassigned actuators
  if (unassigned.length > 0) {
    html += '<tr class="inst-separator"><td colspan="9">Unassigned <span style="font-weight:400;color:var(--fg2)">(' + unassigned.length + ')</span></td></tr>';
    for (const act of unassigned) renderRow(act);
  }
  tbody.innerHTML = html;
  updateCountBadges();
}

async function setActuatorNote(actId, noteVal) {
  const note = parseInt(noteVal);
  if (isNaN(note) || note < 0 || note > 127) return;
  // Find which instrument this actuator is already assigned to.
  let instIdx = -1;
  if (routing) {
    for (const r of routing) {
      if (r.notes && r.notes.find(n => n.actuator === actId)) {
        instIdx = r.instrument;
        break;
      }
    }
  }
  // AUDIT FIX (P2): if the actuator is not assigned to any instrument, ask
  // explicitly which one to use instead of silently defaulting to index 0.
  if (instIdx < 0) {
    const insts = instruments || [];
    if (insts.length === 0) {
      await appAlert('No instrument', 'Create an instrument first, then assign notes to its actuators.');
      loadActuatorsWithNotes();  // revert the edited input
      return;
    } else if (insts.length === 1) {
      instIdx = insts[0].index;
    } else {
      const chosen = await appSelectInstrument(
        'Assign actuator', 'Which instrument should this note belong to?',
        insts.map(i => ({ value: i.index, label: i.name + ' (' + (i.channel === 0 ? 'Omni' : 'ch.' + i.channel) + ')' })));
      if (chosen === null || isNaN(chosen)) { loadActuatorsWithNotes(); return; }
      instIdx = chosen;
    }
  }
  // Get current routing for this instrument
  const r = routing ? routing.find(x => x.instrument === instIdx) : null;
  let notes = r && r.notes ? r.notes.filter(n => n.actuator !== actId) : [];
  notes.push({note: note, actuator: actId, enabled: true});
  await api('/api/routing', 'POST', {instrument: instIdx, notes: notes});
  routing = await api('/api/routing') || [];
  loadActuatorsWithNotes();
}

function toggleActuatorFields() {
  const type = document.getElementById('ma-type').value;
  document.getElementById('servo-fields').style.display = type === '0' ? 'block' : 'none';
  document.getElementById('solenoid-fields').style.display = type === '1' ? 'block' : 'none';
  // Auto-select bus: servo=bus0, solenoid=bus1
  if (editingActuatorId < 0) {
    document.getElementById('ma-bus').value = type === '1' ? '1' : '0';
  }
  toggleHitHoldFields();
  toggleServoDirection();
  updateAnglePreview();
}

function toggleHitHoldFields() {
  const el = document.getElementById('hit-hold-fields');
  if (!el) return;
  const mode = document.getElementById('ma-sol-behavior').value;
  el.style.display = (mode === '1') ? 'block' : 'none';
}

function toggleServoDirection() {
  const el = document.getElementById('servo-direction-group');
  const stdFields = document.getElementById('servo-standard-fields');
  const altFields = document.getElementById('servo-alterne-fields');
  const mode = document.getElementById('ma-servo-behavior').value;
  const isAlterne = mode === '1';
  // Show direction only for strike (0) and key (3)
  if (el) el.style.display = (mode === '0' || mode === '3') ? 'block' : 'none';
  // Show standard fields (idle + amplitude) for all modes except alternate
  if (stdFields) stdFields.style.display = isAlterne ? 'none' : '';
  // Show angle A / angle B only for alternate
  if (altFields) altFields.style.display = isAlterne ? '' : 'none';
  updateAnglePreview();
}

function openActuatorModal() {
  editingActuatorId = -1;
  // AUDIT FIX (UI-P1): assign the first FREE id in 0..127 (not max+1, which
  // could exceed 127 or collide after deletions).
  const usedIds = new Set((actuators || []).map(a => a.id));
  let nextId = 0;
  while (nextId < 128 && usedIds.has(nextId)) nextId++;
  document.getElementById('ma-id').value = nextId;
  document.getElementById('ma-type').value = '0';
  // Auto-increment PCA channel based on existing actuators
  let nextCh = 0;
  let pcaAddr = 64; // 0x40
  if (actuators && actuators.length > 0) {
    const sorted = [...actuators].sort((a,b) =>
      a.pca_addr !== b.pca_addr ? a.pca_addr - b.pca_addr : a.pca_ch - b.pca_ch);
    const last = sorted[sorted.length - 1];
    nextCh = last.pca_ch + 1;
    pcaAddr = last.pca_addr;
    if (nextCh > 15) { nextCh = 0; pcaAddr = Math.min(pcaAddr + 1, 67); }
  }
  document.getElementById('ma-ch').value = nextCh;
  document.getElementById('ma-pca').value = pcaAddr;
  document.getElementById('ma-latency').value = '10';
  // Reset servo fields
  document.getElementById('ma-servo-behavior').value = '0';
  document.getElementById('ma-angle-init').value = '90';
  document.getElementById('ma-amplitude').value = '45';
  document.getElementById('ma-speed').value = '150';
  document.getElementById('ma-angle-a-alt').value = '90';
  document.getElementById('ma-angle-b').value = '120';
  // Reset solenoid fields
  document.getElementById('ma-sol-behavior').value = '0';
  document.getElementById('ma-pulse-min').value = '5';
  document.getElementById('ma-pulse-max').value = '30';
  document.getElementById('ma-pwm-init').value = '4095';
  document.getElementById('ma-pwm-hold').value = '2048';
  document.getElementById('ma-ramp').value = '50';
  toggleActuatorFields();
  document.getElementById('modal-act-title').textContent = 'New actuator';
  document.getElementById('modal-actuator').classList.add('show');
  updateAnglePreview();
}

function editActuator(id) {
  const act = actuators.find(a => a.id === id);
  if (!act) return;
  editingActuatorId = id;
  document.getElementById('ma-id').value = act.id;
  document.getElementById('ma-type').value = act.type;
  document.getElementById('ma-bus').value = act.bus_id;
  document.getElementById('ma-pca').value = act.pca_addr;
  document.getElementById('ma-ch').value = act.pca_ch;
  document.getElementById('ma-latency').value = act.latency_ms;
  toggleActuatorFields();
  if (act.type === 0) {
    document.getElementById('ma-servo-behavior').value = act.behavior || 0;
    document.getElementById('ma-hit-reverse').value = act.hit_reverse ? '1' : '0';
    document.getElementById('ma-angle-init').value = act.angle_init !== undefined ? act.angle_init : 90;
    document.getElementById('ma-amplitude').value = act.amplitude !== undefined ? act.amplitude : 45;
    document.getElementById('ma-speed').value = act.speed_ms || 150;
    document.getElementById('ma-angle-a-alt').value = act.angle_init !== undefined ? act.angle_init : 90;
    document.getElementById('ma-angle-b').value = act.angle_b !== undefined ? act.angle_b : 120;
    toggleServoDirection();
  } else {
    document.getElementById('ma-sol-behavior').value = act.behavior || 0;
    document.getElementById('ma-pulse-min').value = act.pulse_min_ms !== undefined ? act.pulse_min_ms : 5;
    document.getElementById('ma-pulse-max').value = act.pulse_max_ms !== undefined ? act.pulse_max_ms : (act.pulse_ms || 30);
    document.getElementById('ma-pwm-init').value = act.pwm_initial !== undefined ? act.pwm_initial : 4095;
    document.getElementById('ma-pwm-hold').value = act.pwm_hold !== undefined ? act.pwm_hold : 2048;
    document.getElementById('ma-ramp').value = act.ramp_ms || 50;
    toggleHitHoldFields();
  }
  document.getElementById('modal-act-title').textContent = 'Edit actuator #' + id;
  document.getElementById('modal-actuator').classList.add('show');
  updateAnglePreview();
}

async function saveActuator() {
  const type = parseInt(document.getElementById('ma-type').value);
  const data = {
    id: parseInt(document.getElementById('ma-id').value),
    type: type,
    bus_id: parseInt(document.getElementById('ma-bus').value),
    pca_addr: parseInt(document.getElementById('ma-pca').value),
    pca_ch: parseInt(document.getElementById('ma-ch').value),
    latency_ms: parseInt(document.getElementById('ma-latency').value),
    enabled: true
  };

  if (type === 0) { // Servo
    data.behavior = parseInt(document.getElementById('ma-servo-behavior').value);
    data.hit_reverse = document.getElementById('ma-hit-reverse').value === '1';
    if (data.behavior === 1) { // Alternate: use angle A/B fields
      data.angle_init = parseInt(document.getElementById('ma-angle-a-alt').value);
      data.amplitude = 0;
    } else {
      data.angle_init = parseInt(document.getElementById('ma-angle-init').value);
      data.amplitude = parseInt(document.getElementById('ma-amplitude').value);
    }
    data.speed_ms = parseInt(document.getElementById('ma-speed').value);
    data.angle_b = parseInt(document.getElementById('ma-angle-b').value);
  } else { // Solenoid
    data.behavior = parseInt(document.getElementById('ma-sol-behavior').value);
    data.pulse_min_ms = parseInt(document.getElementById('ma-pulse-min').value);
    data.pulse_max_ms = parseInt(document.getElementById('ma-pulse-max').value);
    data.pulse_ms = data.pulse_max_ms; // backward compat
    data.pwm_initial = parseInt(document.getElementById('ma-pwm-init').value);
    data.pwm_hold = parseInt(document.getElementById('ma-pwm-hold').value);
    data.ramp_ms = parseInt(document.getElementById('ma-ramp').value);
  }

  const resp = await api('/api/actuator', 'POST', data);
  if (!resp || !resp.ok) return;   // error already surfaced by api()
  closeModal('modal-actuator');
  editingActuatorId = -1;
  loadActuatorsWithNotes();
}

async function testActuator(id) {
  await api('/api/test/actuator', 'POST', {id: id, velocity: 100, note_on: true});
  setTimeout(() => {
    api('/api/test/actuator', 'POST', {id: id, velocity: 0, note_on: false});
  }, 500);
}

async function deleteActuator(id) {
  if (!await appConfirm('Delete actuator', 'Delete actuator #' + id + '?', {danger:true, icon:'\ud83d\uddd1\ufe0f'})) return;
  await api('/api/actuator?id=' + id, 'DELETE');
  loadActuatorsWithNotes();
}

// ============================================================================
// MIDI Config
// ============================================================================
async function loadMidiConfig() {
  const d = await api('/api/midi');
  if (!d) return;
  document.getElementById('midi-serial').checked = d.serial_enabled;
  document.getElementById('midi-udp').checked = d.udp_enabled;
  document.getElementById('midi-rtp').checked = d.rtp_enabled;
  el('midi-rx-pin', d.serial_rx_pin);
  el('midi-udp-port', d.udp_port);
  el('midi-rtp-port', d.rtp_port);
  document.getElementById('midi-jitter').value = d.jitter_buffer_ms;
  el('midi-jitter-val', d.jitter_buffer_ms);
}

async function updateMidiConfig() {
  await api('/api/midi', 'POST', {
    serial_enabled: document.getElementById('midi-serial').checked,
    udp_enabled: document.getElementById('midi-udp').checked,
    rtp_enabled: document.getElementById('midi-rtp').checked,
    jitter_buffer_ms: parseInt(document.getElementById('midi-jitter').value)
  });
}

// ============================================================================
// MIDI Message Log (last 50 messages, fed by WebSocket)
// ============================================================================
const MIDI_LOG_MAX = 50;
let midiLogEntries = [];
const MIDI_TYPE_NAMES = {
  0x80:'Note Off', 0x90:'Note On', 0xA0:'Aftertouch', 0xB0:'CC',
  0xC0:'Program', 0xD0:'Ch Pressure', 0xE0:'Pitch Bend'
};
const MIDI_SOURCE_NAMES = {serial:'Cable', udp:'UDP', rtp:'RTP'};

function formatMidiData(m) {
  const t = m.type & 0xF0;
  if (t === 0x90 || t === 0x80) return noteName(m.d1) + ' (' + m.d1 + ') v=' + m.d2;
  if (t === 0xB0) return 'CC' + m.d1 + ' = ' + m.d2;
  if (t === 0xC0) return 'Prog ' + m.d1;
  if (t === 0xE0) return '' + ((m.d2 << 7 | m.d1) - 8192);
  return m.d1 + (m.d2 !== undefined ? ', ' + m.d2 : '');
}

function pushMidiLog(m) {
  if (document.getElementById('midi-log-pause')?.checked) return;
  midiLogEntries.push(m);
  if (midiLogEntries.length > MIDI_LOG_MAX) midiLogEntries.shift();
  renderMidiLog();
}

function renderMidiLog() {
  const tbody = document.getElementById('midi-log-table');
  if (!tbody) return;
  if (midiLogEntries.length === 0) {
    tbody.innerHTML = '<tr><td colspan="6" style="color:var(--fg2)">Waiting for MIDI messages...</td></tr>';
    return;
  }
  let html = '';
  for (let i = midiLogEntries.length - 1; i >= 0; i--) {
    const m = midiLogEntries[i];
    const typeName = MIDI_TYPE_NAMES[m.type & 0xF0] || '0x' + m.type.toString(16);
    const ch = (m.type & 0x0F) + 1;
    const src = MIDI_SOURCE_NAMES[m.src] || m.src || '?';
    const routed = m.routed ? '<span class="badge on">Yes</span>' : '<span class="badge off">No</span>';
    // AUDIT FIX: escape every value coming from the MIDI bus (src/typeName
    // may originate from external traffic). routed is a static literal.
    html += '<tr>';
    html += '<td style="font-size:11px;font-family:monospace;white-space:nowrap">' + escHtml(formatLogTime(m.t || 0)) + '</td>';
    html += '<td>' + escHtml(src) + '</td>';
    html += '<td>' + ch + '</td>';
    html += '<td>' + escHtml(typeName) + '</td>';
    html += '<td>' + escHtml(formatMidiData(m)) + '</td>';
    html += '<td>' + routed + '</td>';
    html += '</tr>';
  }
  tbody.innerHTML = html;
  const info = document.getElementById('midi-log-count');
  if (info) info.textContent = midiLogEntries.length + ' / ' + MIDI_LOG_MAX + ' messages';
}

function clearMidiLog() {
  midiLogEntries = [];
  renderMidiLog();
}

async function loadInstrumentSelects() {
  if (!instruments || instruments.length === 0) {
    instruments = await api('/api/instruments');
  }
  const selects = ['cc-instrument'];
  for (const sid of selects) {
    const sel = document.getElementById(sid);
    if (!sel) continue;
    sel.innerHTML = '';
    if (!instruments || instruments.length === 0) {
      sel.innerHTML = '<option value="">No instrument</option>';
      continue;
    }
    for (const inst of instruments) {
      const opt = document.createElement('option');
      opt.value = inst.index;
      opt.textContent = inst.name + ' (' + (inst.channel === 0 ? 'Omni' : 'ch.' + inst.channel) + ')';
      sel.appendChild(opt);
    }
  }
}

// ============================================================================
// CC Routing (Control Changes only)
// ============================================================================
async function loadCCRouting() {
  routing = await api('/api/routing');
  const sel = document.getElementById('cc-instrument');
  const instIdx = parseInt(sel ? sel.value : '0');
  const r = routing ? routing.find(x => x.instrument === instIdx) : null;

  const ctbody = document.getElementById('mapping-cc-table');
  if (!ctbody) return;
  if (!r || !r.ccs || r.ccs.length === 0) {
    ctbody.innerHTML = '<tr><td colspan="6" style="color:var(--fg2)">No CC mapping</td></tr>';
  } else {
    let html = '';
    for (let ci = 0; ci < r.ccs.length; ci++) {
      const cm = r.ccs[ci];
      const isPos = cm.target === 0;
      const unit = CC_TARGET_UNITS[cm.target] || '';
      html += '<tr>';
      html += '<td>CC ' + cm.cc + '</td>';
      html += '<td>' + (isPos ? '<span class="badge servo">Position</span>' : '<span class="badge sol">Modifier</span>') + '</td>';
      html += '<td>Servo #' + cm.actuator + '</td>';
      html += '<td>' + (CC_TARGETS[cm.target] || '?') + '</td>';
      html += '<td>' + cm.min + unit + ' \u2192 ' + cm.max + unit + '</td>';
      html += '<td><button class="btn sm" onclick="editCC(' + instIdx + ',' + cm.cc + ')">Edit</button> ';
      html += '<button class="btn sm" onclick="deleteCC(' + instIdx + ',' + cm.cc + ')">Delete</button></td>';
      html += '</tr>';
    }
    ctbody.innerHTML = html;
  }

  updateCountBadges();
}

// Build map: actuator_id -> instrument index (for servos used by note mappings)
function getInstrumentActuatorIds() {
  const used = {};
  if (routing) {
    for (const r of routing) {
      if (r.notes) {
        for (const n of r.notes) {
          if (n.enabled) used[n.actuator] = r.instrument;
        }
      }
    }
  }
  return used;
}

// Populate servo select with FREE servos only (not assigned to any instrument note)
function populateFreeServoSelect(selectedActId) {
  const sel = document.getElementById('cc-actuator-free');
  sel.innerHTML = '';
  const usedByInst = getInstrumentActuatorIds();
  if (actuators) {
    for (const a of actuators) {
      if (a.type !== 0) continue;
      if (usedByInst[a.id] !== undefined) continue;
      const opt = document.createElement('option');
      opt.value = a.id;
      opt.textContent = 'Servo #' + a.id;
      if (selectedActId !== undefined && a.id === selectedActId) opt.selected = true;
      sel.appendChild(opt);
    }
  }
  const hint = document.getElementById('cc-create-servo-hint');
  if (sel.options.length === 0) {
    sel.innerHTML = '<option value="">No free servo</option>';
    if (hint) hint.style.display = '';
  } else {
    if (hint) hint.style.display = 'none';
  }
}

// Populate servo select with INSTRUMENT servos (assigned to current instrument notes)
function populateInstServoSelect(instIdx, selectedActId) {
  const sel = document.getElementById('cc-actuator-inst');
  sel.innerHTML = '';
  const r = routing ? routing.find(x => x.instrument === instIdx) : null;
  const instActIds = new Set();
  if (r && r.notes) {
    for (const n of r.notes) {
      if (n.enabled) instActIds.add(n.actuator);
    }
  }
  if (actuators) {
    for (const a of actuators) {
      if (a.type !== 0) continue;
      if (!instActIds.has(a.id)) continue;
      const opt = document.createElement('option');
      opt.value = a.id;
      opt.textContent = 'Servo #' + a.id;
      if (selectedActId !== undefined && a.id === selectedActId) opt.selected = true;
      sel.appendChild(opt);
    }
  }
  if (sel.options.length === 0) {
    sel.innerHTML = '<option value="">No servo in this instrument</option>';
  }
}

function toggleCCCategory() {
  const cat = document.getElementById('cc-category').value;
  document.getElementById('cc-cat-position').style.display = cat === 'position' ? '' : 'none';
  document.getElementById('cc-cat-modifier').style.display = cat === 'modifier' ? '' : 'none';
}

function updateCCModRangeHints() {
  const t = parseInt(document.getElementById('cc-mod-target').value);
  const range = CC_TARGET_RANGES[t] || [0,180];
  const unit = CC_TARGET_UNITS[t] || '';
  const help = document.getElementById('cc-mod-help');
  if (t === 1) {
    if (help) help.textContent = 'Modifies the strike range in real-time for upcoming notes';
  } else {
    if (help) help.textContent = 'Modifies the forward movement duration (10ms = fast, 2000ms = slow)';
  }
  document.getElementById('cc-mod-min-unit').textContent = unit ? '(' + unit + ')' : '';
  document.getElementById('cc-mod-max-unit').textContent = unit ? '(' + unit + ')' : '';
  if (editingCCNum < 0) {
    document.getElementById('cc-mod-min').value = range[0];
    document.getElementById('cc-mod-max').value = range[1];
  }
  updateCCServoInfo();
}

function updateCCServoInfo() {
  const infoDiv = document.getElementById('cc-servo-info');
  if (!infoDiv) return;
  const actId = parseInt(document.getElementById('cc-actuator-inst').value);
  const act = actuators ? actuators.find(a => a.id === actId) : null;
  if (!act || isNaN(actId)) { infoDiv.style.display = 'none'; return; }
  const init = act.angle_init !== undefined ? act.angle_init : 90;
  const amp = act.amplitude !== undefined ? act.amplitude : 45;
  const rev = act.hit_reverse;
  const speed = act.speed_ms || 150;
  const beh = SERVO_BEHAVIORS[act.behavior] || '?';
  let minAngle, maxAngle;
  if (act.behavior === 1) { // Alternate
    minAngle = init;
    maxAngle = act.angle_b !== undefined ? act.angle_b : 120;
  } else if (rev) {
    minAngle = Math.max(0, init - amp);
    maxAngle = init;
  } else {
    minAngle = init;
    maxAngle = Math.min(180, init + amp);
  }
  let s = '<strong>Servo #' + act.id + '</strong> \u2014 ' + beh;
  s += ' \u2022 Idle: ' + init + '\u00b0';
  s += ' \u2022 Range: ' + minAngle + '\u00b0 \u2192 ' + maxAngle + '\u00b0';
  s += ' \u2022 Speed: ' + speed + 'ms';
  infoDiv.innerHTML = s;
  infoDiv.style.display = '';
}

// Quick-create a servo for CC usage
async function quickCreateServoForCC() {
  const nextId = (actuators && actuators.length > 0) ? Math.max(...actuators.map(a => a.id)) + 1 : 0;
  let nextCh = 0, pcaAddr = 64;
  if (actuators && actuators.length > 0) {
    const sorted = [...actuators].sort((a,b) => a.pca_addr !== b.pca_addr ? a.pca_addr - b.pca_addr : a.pca_ch - b.pca_ch);
    const last = sorted[sorted.length - 1];
    nextCh = last.pca_ch + 1;
    pcaAddr = last.pca_addr;
    if (nextCh > 15) { nextCh = 0; pcaAddr = Math.min(pcaAddr + 1, 67); }
  }
  const data = {
    id: nextId, type: 0, bus_id: 0, pca_addr: pcaAddr, pca_ch: nextCh,
    latency_ms: 0, behavior: 0, hit_reverse: false,
    angle_init: 90, amplitude: 90, speed_ms: 200, angle_b: 120, enabled: true
  };
  await api('/api/actuator', 'POST', data);
  actuators = await api('/api/actuators') || [];
  populateFreeServoSelect(nextId);
  toast('Servo #' + nextId + ' created', 'ok');
}

let editingCCNum = -1;

function openAddCCModal() {
  editingCCNum = -1;
  const instSel = document.getElementById('cc-instrument');
  const instIdx = parseInt(instSel ? instSel.value : '0');
  document.getElementById('cc-category').value = 'position';
  toggleCCCategory();
  populateFreeServoSelect();
  populateInstServoSelect(instIdx);
  document.getElementById('cc-num').value = '1';
  document.getElementById('cc-num').disabled = false;
  document.getElementById('cc-pos-min').value = '0';
  document.getElementById('cc-pos-max').value = '180';
  document.getElementById('cc-mod-target').value = '1';
  updateCCModRangeHints();
  document.getElementById('modal-cc-title').textContent = 'Add CC Mapping';
  document.getElementById('cc-save-btn').textContent = 'Add';
  document.getElementById('modal-cc').classList.add('show');
}

function editCC(instIdx, ccNum) {
  const r = routing ? routing.find(x => x.instrument === instIdx) : null;
  if (!r || !r.ccs) return;
  const cm = r.ccs.find(c => c.cc === ccNum);
  if (!cm) return;
  editingCCNum = ccNum;
  const isPos = cm.target === 0;
  document.getElementById('cc-category').value = isPos ? 'position' : 'modifier';
  toggleCCCategory();
  if (isPos) {
    populateFreeServoSelect(cm.actuator);
    document.getElementById('cc-pos-min').value = cm.min;
    document.getElementById('cc-pos-max').value = cm.max;
  } else {
    populateInstServoSelect(instIdx, cm.actuator);
    document.getElementById('cc-mod-target').value = cm.target;
    document.getElementById('cc-mod-min').value = cm.min;
    document.getElementById('cc-mod-max').value = cm.max;
    updateCCModRangeHints();
  }
  document.getElementById('cc-num').value = cm.cc;
  document.getElementById('cc-num').disabled = true;
  document.getElementById('modal-cc-title').textContent = 'Edit CC ' + ccNum;
  document.getElementById('cc-save-btn').textContent = 'Save';
  document.getElementById('modal-cc').classList.add('show');
}

async function saveCC() {
  const instSel = document.getElementById('cc-instrument');
  const instIdx = parseInt(instSel ? instSel.value : '0');
  const ccNum = parseInt(document.getElementById('cc-num').value);
  const cat = document.getElementById('cc-category').value;
  let actId, target, min, max;
  if (cat === 'position') {
    actId = parseInt(document.getElementById('cc-actuator-free').value);
    target = 0;
    min = parseInt(document.getElementById('cc-pos-min').value);
    max = parseInt(document.getElementById('cc-pos-max').value);
  } else {
    actId = parseInt(document.getElementById('cc-actuator-inst').value);
    target = parseInt(document.getElementById('cc-mod-target').value);
    min = parseInt(document.getElementById('cc-mod-min').value);
    max = parseInt(document.getElementById('cc-mod-max').value);
  }
  if (isNaN(ccNum) || isNaN(actId)) { toast('Invalid values', 'error'); return; }

  const r = routing ? routing.find(x => x.instrument === instIdx) : null;
  let ccs = r && r.ccs ? [...r.ccs] : [];
  ccs = ccs.filter(c => c.cc !== ccNum);
  ccs.push({cc: ccNum, actuator: actId, target, min, max, enabled: true});
  await api('/api/routing/cc', 'POST', {instrument: instIdx, ccs});
  closeModal('modal-cc');
  toast(editingCCNum >= 0 ? 'CC ' + ccNum + ' updated' : 'CC ' + ccNum + ' added', 'ok');
  editingCCNum = -1;
  loadCCRouting();
}

async function deleteCC(instIdx, ccNum) {
  if (!await appConfirm('Delete CC', 'Delete CC mapping ' + ccNum + '?', {danger:true, icon:'\ud83d\uddd1\ufe0f'})) return;
  const r = routing ? routing.find(x => x.instrument === instIdx) : null;
  if (!r || !r.ccs) return;
  const ccs = r.ccs.filter(c => c.cc !== ccNum);
  await api('/api/routing/cc', 'POST', {instrument: instIdx, ccs});
  loadCCRouting();
}

// ============================================================================
// Piano
// ============================================================================
// Build all pianos — one per instrument
function buildAllPianos() {
  const container = document.getElementById('pianos-container');
  if (!container) return;
  container.innerHTML = '';
  pianoNotes = {};
  pressedKeys = {};

  if (!instruments || instruments.length === 0) {
    container.innerHTML = '<p style="color:var(--fg2);font-size:13px;margin-top:16px">No instrument. Use the wizard to create your first instrument.</p>';
    return;
  }

  for (const inst of instruments) {
    const idx = inst.index;
    const r = routing ? routing.find(x => x.instrument === idx) : null;
    let mappedNotes = new Set();
    pianoNotes[idx] = {};
    if (r && r.notes) {
      for (const nm of r.notes) {
        if (nm.enabled) {
          mappedNotes.add(nm.note);
          pianoNotes[idx][nm.note] = nm.actuator;
        }
      }
    }
    if (mappedNotes.size === 0) continue; // skip instruments with no notes

    // Section wrapper
    const section = document.createElement('div');
    section.style.cssText = 'margin-top:20px';

    const title = document.createElement('div');
    title.className = 'section-title';
    title.style.fontSize = '14px';
    const chLabel = inst.channel === 0 ? 'Omni' : 'ch.' + inst.channel;
    title.innerHTML = '<span>' + esc(inst.name) + ' <span style="font-weight:400;color:var(--fg2)">(' + chLabel + ')</span></span>';
    section.appendChild(title);

    const pianoWrap = document.createElement('div');
    pianoWrap.className = 'piano-container';
    const piano = document.createElement('div');
    piano.className = 'piano';
    piano.id = 'piano-' + idx;

    // Compute note range — tight range around mapped notes
    const minNote = Math.min(...mappedNotes);
    const maxNote = Math.max(...mappedNotes);
    // Pad 1 note below and above to give context, clamp to octave boundary
    let startNote = Math.max(0, minNote - 1);
    // Align to nearest white key below
    while (startNote > 0 && [1,3,6,8,10].includes(startNote % 12)) startNote--;
    let endNote = Math.min(128, maxNote + 2);
    // Align to nearest white key above
    while (endNote < 128 && [1,3,6,8,10].includes(endNote % 12)) endNote++;

    // Render ALL notes in the range for correct piano layout
    // Unmapped notes are shown dimmed but maintain proper spatial ordering
    const notesToRender = new Set();
    for (let n = startNote; n <= endNote; n++) {
      notesToRender.add(n);
    }

    // White keys first (they define layout flow)
    let wIdx = 0;
    const whiteIdxMap = {}; // note -> white key index (for black key positioning)
    for (let n = startNote; n <= endNote; n++) {
      const nio = n % 12;
      if ([1,3,6,8,10].includes(nio)) continue; // skip black
      if (!notesToRender.has(n)) continue;
      whiteIdxMap[n] = wIdx;
      const k = document.createElement('div');
      const wMapped = mappedNotes.has(n);
      k.className = 'white';
      if (!wMapped) { k.style.opacity = '0.4'; k.style.cursor = 'default'; }
      k.dataset.note = n;
      k.dataset.inst = idx;
      k.textContent = noteName(n);
      if (wMapped) {
        k.onmousedown = () => pianoNoteOn(idx, n);
        k.onmouseup = () => pianoNoteOff(idx, n);
        k.onmouseleave = () => pianoNoteOff(idx, n);
        k.addEventListener('touchstart', (e) => { e.preventDefault(); pianoNoteOn(idx, n); }, {passive:false});
        k.addEventListener('touchend', (e) => { e.preventDefault(); pianoNoteOff(idx, n); }, {passive:false});
        k.addEventListener('touchcancel', (e) => { e.preventDefault(); pianoNoteOff(idx, n); }, {passive:false});
      }
      piano.appendChild(k);
      wIdx++;
    }
    // Black keys — positioned relative to the white key on their left
    for (let n = startNote; n <= endNote; n++) {
      const nio = n % 12;
      if (![1,3,6,8,10].includes(nio)) continue; // skip white
      if (!notesToRender.has(n)) continue;
      // Find the white key just below this black key
      const prevWhite = n - 1; // always a white key for standard black positions
      if (!(prevWhite in whiteIdxMap)) continue;
      const wi = whiteIdxMap[prevWhite];
      const k = document.createElement('div');
      const bMapped = mappedNotes.has(n);
      k.className = 'black';
      if (!bMapped) { k.style.opacity = '0.3'; k.style.cursor = 'default'; }
      k.dataset.note = n;
      k.dataset.inst = idx;
      k.style.left = 'calc(' + (wi + 1) + ' * var(--wk) - var(--wk) * 0.325)';
      if (bMapped) {
        k.onmousedown = (e) => { e.preventDefault(); pianoNoteOn(idx, n); };
        k.onmouseup = () => pianoNoteOff(idx, n);
        k.onmouseleave = () => pianoNoteOff(idx, n);
        k.addEventListener('touchstart', (e) => { e.preventDefault(); pianoNoteOn(idx, n); }, {passive:false});
        k.addEventListener('touchend', (e) => { e.preventDefault(); pianoNoteOff(idx, n); }, {passive:false});
        k.addEventListener('touchcancel', (e) => { e.preventDefault(); pianoNoteOff(idx, n); }, {passive:false});
      }
      piano.appendChild(k);
    }

    const scrollWrap = document.createElement('div');
    scrollWrap.className = 'piano-scroll-wrap';
    const btnL = document.createElement('button');
    btnL.className = 'piano-nav';
    btnL.innerHTML = '&#9664;';
    btnL.onclick = () => { pianoWrap.scrollBy({left: -120, behavior: 'smooth'}); };
    const btnR = document.createElement('button');
    btnR.className = 'piano-nav';
    btnR.innerHTML = '&#9654;';
    btnR.onclick = () => { pianoWrap.scrollBy({left: 120, behavior: 'smooth'}); };

    pianoWrap.appendChild(piano);
    scrollWrap.appendChild(btnL);
    scrollWrap.appendChild(pianoWrap);
    scrollWrap.appendChild(btnR);
    section.appendChild(scrollWrap);
    container.appendChild(section);
  }
}

function pianoNoteOn(instIdx, note) {
  const instMap = pianoNotes[instIdx];
  if (!instMap) return;
  const actId = instMap[note];
  if (actId === undefined) return;
  // Track that this key is being held down
  pressedKeys[instIdx + '-' + note] = true;
  // Visual feedback only on this instrument's piano
  const piano = document.getElementById('piano-' + instIdx);
  if (piano) piano.querySelectorAll('[data-note="' + note + '"]').forEach(k => k.classList.add('active'));
  // AUDIT FIX (UI-P1): use the SAME WebSocket for ON and OFF.
  if (ws && wsConnected) {
    ws.send(JSON.stringify({cmd:'test', id:actId, on:true, vel:100, token:authToken || undefined}));
  }
}

function pianoNoteOff(instIdx, note) {
  delete pressedKeys[instIdx + '-' + note];
  const piano = document.getElementById('piano-' + instIdx);
  if (piano) piano.querySelectorAll('[data-note="' + note + '"]').forEach(k => k.classList.remove('active'));
  const instMap = pianoNotes[instIdx];
  if (!instMap) return;
  const actId = instMap[note];
  if (actId === undefined) return;
  // AUDIT FIX (UI-P1): NOTE_OFF over the same WebSocket as NOTE_ON (not REST),
  // so a drop between them cannot strand a Key / Hit-and-Hold output.
  if (ws && wsConnected) {
    ws.send(JSON.stringify({cmd:'test', id:actId, on:false, vel:0, token:authToken || undefined}));
  }
}

function updatePianoActive(activeActuators) {
  if (currentPage !== 'instrument') return;

  // Reset active states but preserve keys currently held by user
  document.querySelectorAll('.piano .active').forEach(k => {
    const key = k.dataset.inst + '-' + k.dataset.note;
    if (!pressedKeys[key]) k.classList.remove('active');
  });

  if (!activeActuators) return;

  // Highlight the correct key on the correct instrument's piano
  const activeIds = new Set(activeActuators.map(a => a.id));
  for (const [instIdx, noteMap] of Object.entries(pianoNotes)) {
    const piano = document.getElementById('piano-' + instIdx);
    if (!piano) continue;
    for (const [note, actId] of Object.entries(noteMap)) {
      if (activeIds.has(actId)) {
        piano.querySelectorAll('[data-note="' + note + '"]').forEach(k => k.classList.add('active'));
      }
    }
  }
}

// ============================================================================
// Power
// ============================================================================
async function loadPower() {
  const d = await api('/api/power');
  if (!d) return;
  if (d.budget) {
    el('p-poly-max', d.budget.max_polyphony);
    document.getElementById('pw-poly').value = d.budget.max_polyphony;
  }
  if (d.stats) {
    el('p-rejected', d.stats.rejected);
    el('d-active', d.stats.active_count);
    const max = d.budget ? d.budget.max_polyphony : 12;
    const pct = max > 0 ? Math.min(100, Math.round(d.stats.active_count / max * 100)) : 0;
    const bar = document.getElementById('p-total-bar');
    if (bar) { bar.style.width = pct + '%'; bar.style.background = pct > 80 ? 'var(--red)' : 'var(--green)'; }
  }
}

async function savePowerBudget() {
  const r = await api('/api/power/budget', 'POST', {
    max_polyphony: parseInt(document.getElementById('pw-poly').value)
  });
  if (!r || !r.ok) return;   // AUDIT FIX (UI-P1): don't claim success on error
  toast('Polyphony updated', 'ok');
  loadPower();
}

// ============================================================================
// Safety
// ============================================================================
async function loadSafety() {
  const d = await api('/api/safety');
  if (!d) return;
  if (d.config) {
    document.getElementById('sf-duty').value = d.config.max_duty_pct;
    document.getElementById('sf-freq').value = d.config.max_freq_hz;
    document.getElementById('sf-watchdog').value = d.config.watchdog_ms;
  }
}

async function saveSafetyConfig() {
  const r = await api('/api/safety', 'POST', {
    max_duty_pct: parseInt(document.getElementById('sf-duty').value),
    max_freq_hz: parseInt(document.getElementById('sf-freq').value),
    watchdog_ms: parseInt(document.getElementById('sf-watchdog').value),
    max_polyphony: parseInt(document.getElementById('pw-poly').value)
  });
  if (!r || !r.ok) return;   // AUDIT FIX (UI-P1)
  toast('Limits applied (use Save to flash to keep them)', 'ok');
}

async function toggleKillSwitch(on) {
  await api('/api/killswitch', 'POST', {active: on});
}

// ============================================================================
// Settings
// ============================================================================
async function loadWiFiConfig() {
  const d = await api('/api/wifi');
  if (!d) return;
  document.getElementById('set-ssid').value = d.ssid || '';
  document.getElementById('set-hostname').value = d.hostname || 'play-mode';
  document.getElementById('set-ap-fallback').value = d.ap_fallback ? '1' : '0';
}

async function saveWiFiConfig() {
  const apPass = document.getElementById('set-ap-pass').value;
  if (apPass.length > 0 && apPass.length < 8) {
    appAlert('Invalid AP password', 'The Access Point password must be at least 8 characters (or left empty).', {icon:'\u26a0\ufe0f'});
    return;
  }
  const resp = await api('/api/wifi', 'POST', {
    ssid: document.getElementById('set-ssid').value,
    password: document.getElementById('set-pass').value,
    ap_password: apPass,
    hostname: document.getElementById('set-hostname').value,
    ap_fallback: document.getElementById('set-ap-fallback').value === '1',
    enabled: true
  });
  // AUDIT FIX (UI-P1): api() returns null on any error (already toasted) \u2014 do
  // not claim success in that case.
  if (!resp || !resp.ok) {
    if (resp && resp.error) appAlert('Error', resp.error, {icon:'\u274c'});
    return;
  }
  appAlert('WiFi saved', 'Restart the device to apply the new settings.', {icon:'\ud83d\udce1'});
}

async function loadBuses() {
  const buses = await api('/api/buses');
  const tbody = document.getElementById('buses-table');
  if (!buses || buses.length === 0) {
    tbody.innerHTML = '<tr><td colspan="8" style="color:var(--fg2)">No bus detected</td></tr>';
    return;
  }
  let html = '';
  for (const b of buses) {
    const pwmSel50 = b.freq_pwm <= 60 ? ' selected' : '';
    const pwmSel200 = (b.freq_pwm > 60 && b.freq_pwm <= 300) ? ' selected' : '';
    const pwmSel1k = b.freq_pwm > 300 ? ' selected' : '';
    html += '<tr>';
    html += '<td>Bus ' + b.id + '</td>';
    html += '<td>GPIO ' + b.sda + '</td>';
    html += '<td>GPIO ' + b.scl + '</td>';
    html += '<td>GPIO ' + b.oe + '</td>';
    html += '<td>' + (b.freq_i2c / 1000) + ' kHz</td>';
    html += '<td><select class="form-select" style="font-size:12px;min-width:90px" onchange="setBusPwmFreq(' + b.id + ',this.value)">';
    html += '<option value="50"' + pwmSel50 + '>50 Hz (Servo)</option>';
    html += '<option value="200"' + pwmSel200 + '>200 Hz</option>';
    html += '<option value="1000"' + pwmSel1k + '>1000 Hz (Solenoid)</option>';
    html += '</select></td>';
    html += '<td>' + b.pca_count + ' PCA</td>';
    html += '<td>' + (b.enabled ? '<span class="badge on">Active</span>' : '<span class="badge off">Inactive</span>') + '</td>';
    html += '</tr>';
  }
  tbody.innerHTML = html;
}

async function setBusPwmFreq(busId, freq) {
  const hz = parseInt(freq);
  // AUDIT FIX (P0.6): a servo needs ~50 Hz — refuse a high frequency on a bus
  // that drives one (it would destroy the pulse-width mapping).
  const hasServo = (actuators || []).some(a => a.bus_id === busId && a.type === 0);
  if (hasServo && hz > 130) {
    appAlert('Frequency refused',
      'Bus ' + busId + ' drives a servo. Servos require ~50 Hz; ' + hz + ' Hz would damage the motion. Move solenoids to a separate bus for higher frequencies.',
      {icon:'⚠️'});
    loadBuses();  // revert the select
    return;
  }
  if (!await appConfirm('Change PWM frequency',
      'Changing the bus frequency disables its outputs and recomputes rest positions. Continue?',
      {confirmText:'Change', icon:'⚡'})) { loadBuses(); return; }
  const r = await api('/api/bus/pwm', 'POST', {bus_id: busId, freq_pwm: hz});
  if (!r || !r.ok) return;   // AUDIT FIX (UI-P1)
  toast('PWM frequency bus ' + busId + ' set to ' + hz + ' Hz', 'ok');
}

async function scanI2C() {
  // AUDIT FIX (P0.2): the rescan now runs on the real-time core after cutting
  // the outputs (it must not race the scheduler). It is asynchronous: request
  // it, wait for the scheduler to rebuild the drivers, then read the buses.
  if (!await appConfirm('Scan I\u00b2C',
        'This disables all outputs and re-scans the I\u00b2C buses. You will need to re-arm the kill switch afterwards. Continue?',
        {confirmText:'Scan', danger:true, icon:'\ud83d\udd0d'})) return;
  await api('/api/scan/i2c', 'POST');
  // Give the scheduler a moment to perform the rescan on Core 1.
  await new Promise(r => setTimeout(r, 800));
  const buses = await api('/api/buses');
  let msg = 'I\u00b2C scan complete (outputs disabled \u2014 re-arm to resume):\n';
  if (buses) {
    for (const b of buses) {
      msg += 'Bus ' + b.id + ': ' + (b.pca_count || 0) + ' PCA';
      if (b.pca_addrs && b.pca_addrs.length > 0) {
        msg += ' (' + b.pca_addrs.map(a => '0x' + a.toString(16).toUpperCase()).join(', ') + ')';
      }
      msg += '\n';
    }
  }
  appAlert('Scan I\u00b2C', msg, {icon:'\ud83d\udd0d'});
  loadBuses();
}

async function saveConfig() {
  const result = await api('/api/config/save', 'POST');
  if (result && result.ok) {
    appAlert('Save successful', 'Configuration saved to flash memory.', {icon:'\u2705'});
  } else {
    appAlert('Error', 'Save failed.', {icon:'\u274c'});
  }
}

// AUDIT FIX (UX): configuration backup / restore.
async function exportConfig() {
  // Fetch with auth header, then trigger a client-side download.
  try {
    const headers = {};
    if (authToken) headers['X-PlayMode-Token'] = authToken;
    const res = await fetch('/api/config/export', { headers });
    if (!res.ok) { toast('Export failed (HTTP ' + res.status + ')', 'error'); return; }
    const text = await res.text();
    const blob = new Blob([text], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url; a.download = 'playmode-config.json';
    document.body.appendChild(a); a.click(); a.remove();
    URL.revokeObjectURL(url);
    toast('Configuration exported', 'ok');
  } catch (e) { toast('Export failed', 'error'); }
}

async function importConfig(input) {
  const file = input.files && input.files[0];
  input.value = '';           // allow re-selecting the same file later
  if (!file) return;
  if (!await appConfirm('Import configuration',
      'Replace the current configuration with "' + file.name + '"? The device will restart.',
      {danger:true, confirmText:'Import', icon:'\u26a0\ufe0f'})) return;
  let text;
  try { text = await file.text(); } catch (e) { toast('Could not read file', 'error'); return; }
  let parsed;
  try { parsed = JSON.parse(text); } catch (e) { toast('Not a valid JSON file', 'error'); return; }
  const resp = await api('/api/config/import', 'POST', parsed);
  if (!resp || !resp.ok) return;   // api() surfaced the error
  await appAlert('Import complete', 'Configuration imported. The device is restarting \u2014 the page will reload shortly.', {icon:'\u2705'});
  setTimeout(() => location.reload(), 6000);
}

async function confirmResetDefaults() {
  if (!await appConfirm('Reset', 'Reset all configuration to default values?\nThe device will restart. This action is irreversible.', {danger:true, confirmText:'Reset', icon:'\u26a0\ufe0f'})) return;
  await api('/api/config/defaults', 'POST');
  // AUDIT FIX (P0.4): the device reboots after a factory reset. Wait for it to
  // come back before reloading the page.
  await appAlert('Reset complete', 'Configuration reset. The device is restarting \u2014 the page will reload shortly.', {icon:'\u2705'});
  setTimeout(() => location.reload(), 6000);
}

// ============================================================================
// Utils
// ============================================================================
function esc(s) {
  const d = document.createElement('div');
  d.textContent = s;
  return d.innerHTML;
}

// ============================================================================
// Acoustic Calibration (Phase 7)
// ============================================================================
let calPollInterval = null;

async function loadCalibrateStatus() {
  const d = await api('/api/calibrate/status');
  if (!d) return;

  const stateNames = {
    idle:'Inactive', ambient:'Measuring ambient…', triggering:'Triggering…',
    recording:'Recording…', pausing:'Pausing…',
    complete:'Complete \u2713', error:'Error \u2717'
  };
  el('cal-state', stateNames[d.state] || d.state);
  el('cal-progress', d.progress || 0);
  el('cal-result-count', d.result_count || 0);

  const bar = document.getElementById('cal-bar');
  if (bar) bar.style.width = (d.progress || 0) + '%';

  const stopBtn  = document.getElementById('cal-btn-stop');
  const applyBtn = document.getElementById('cal-btn-apply');
  if (stopBtn)  stopBtn.style.display  = d.running ? 'inline-block' : 'none';
  if (applyBtn) applyBtn.style.display = (d.state === 'complete' && d.result_count > 0) ? 'inline-block' : 'none';

  if (d.running) {
    el('cal-cur-act', 'Act ' + d.current_act);
  } else {
    el('cal-cur-act', d.state === 'complete' ? 'Complete' : '—');
  }

  // Stop polling when complete
  if (!d.running && calPollInterval) {
    clearInterval(calPollInterval);
    calPollInterval = null;
    loadCalibrateResults();
  }
}

async function loadCalibrateResults() {
  const data = await api('/api/calibrate/results');
  const tbody = document.getElementById('cal-results-table');
  if (!tbody || !data || !data.length) {
    if (tbody) tbody.innerHTML = '<tr><td colspan="6" style="color:var(--fg2);text-align:center">No results</td></tr>';
    return;
  }

  const typeNames = ['Servo', 'Solenoid'];
  let html = '';
  for (const r of data) {
    const hasMeasure = r.measured_ms !== null && r.measured_ms !== undefined;
    const badge = hasMeasure
      ? (r.success ? '<span class="badge on">OK</span>' : '<span class="badge off">Failed</span>')
      : '<span class="badge" style="background:var(--bg3)">—</span>';

    html += '<tr>';
    html += '<td>' + r.actuator_id + '</td>';
    html += '<td>—</td>';  // type not included in results API
    html += '<td>' + r.current_latency + ' ms</td>';
    html += '<td>' + (hasMeasure && r.success ? '<strong>' + r.measured_ms + ' ms</strong>' : '—') + '</td>';
    html += '<td>' + (hasMeasure ? (r.samples_taken || 0) + '/3' : '—') + '</td>';
    html += '<td>' + badge + '</td>';
    html += '</tr>';
  }
  tbody.innerHTML = html;
}

async function startCalibrateAll() {
  if (!await appConfirm('Calibration', 'Start calibration for all actuators?\nMake sure the microphone is positioned and the environment is quiet.', {icon:'\ud83c\udfaf', confirmText:'Start'})) return;
  api('/api/calibrate', 'POST', { all: true }).then(r => {
    if (r && r.ok) {
      toast('Calibration started', 'ok');
      startCalPoll();
    } else {
      toast('Error: ' + (r && r.error ? r.error : 'unknown'), 'error');
    }
  });
}

function startCalibrateOne() {
  const sel = document.getElementById('cal-single-sel');
  const select = document.getElementById('cal-act-select');
  if (!sel || !select) return;
  // Populate the select with known actuators
  select.innerHTML = '';
  for (const a of actuators) {
    const opt = document.createElement('option');
    opt.value = a.id;
    opt.textContent = 'Act ' + a.id + ' (' + (a.type === 0 ? 'Servo' : 'Sol') + ' ch' + a.pca_ch + ')';
    select.appendChild(opt);
  }
  sel.style.display = 'flex';
  sel.style.alignItems = 'center';
}

function confirmCalibrateOne() {
  const id = parseInt(document.getElementById('cal-act-select').value);
  document.getElementById('cal-single-sel').style.display = 'none';
  api('/api/calibrate', 'POST', { id }).then(r => {
    if (r && r.ok) {
      toast('Calibration started for act ' + id, 'ok');
      startCalPoll();
    } else {
      toast('Error: ' + (r && r.error ? r.error : 'unknown'), 'error');
    }
  });
}

function stopCalibration() {
  api('/api/calibrate/stop', 'POST', {}).then(() => {
    toast('Calibration stopped', 'warn');
    loadCalibrateStatus();
  });
}

async function applyCalibrateResults() {
  const r = await api('/api/calibrate/apply', 'POST', {});
  if (r && r.ok) {
    toast(r.applied + ' latencies applied to actuators', 'ok');
    loadActuators();
    loadCalibrateResults();
  } else {
    toast('Error during application', 'error');
  }
}

function startCalPoll() {
  if (calPollInterval) clearInterval(calPollInterval);
  loadCalibrateStatus();
  calPollInterval = setInterval(loadCalibrateStatus, 1000);
}

function toast(msg, type) {
  // AUDIT FIX (UI-P1): render into the always-visible global container (falls
  // back to the old in-page zone if needed).
  const z = document.getElementById('toast-container') || document.getElementById('alert-zone');
  if (!z) return;
  const div = document.createElement('div');
  div.className = 'alert ' + (type === 'ok' ? 'ok' : type === 'error' ? 'danger' : 'warn');
  div.setAttribute('role', type === 'error' ? 'alert' : 'status');
  div.textContent = msg;
  z.appendChild(div);
  setTimeout(() => div.remove(), 4000);
}

// ============================================================================
// Log Manager (Phase 9)
// ============================================================================

function logLevelBadge(lvl) {
  const names  = ['DBG','INF','WRN','ERR','CRT'];
  const styles = [
    'color:var(--fg2)',
    'color:var(--green)',
    'color:#f59e0b',
    'color:var(--red)',
    'color:var(--red);font-weight:700'
  ];
  return '<span style="font-size:11px;font-family:monospace;'+(styles[lvl]||'')+'">'+(names[lvl]||'?')+'</span>';
}

function logCatBadge(cat) {
  const names = ['SYS','MIDI','SCHED','SAFE','PWR','CAL','TEST'];
  return '<span style="font-size:11px;color:var(--fg2)">'+(names[cat]||'?')+'</span>';
}

function formatLogTime(ms) {
  const t = Math.floor(ms / 1000);
  const h = Math.floor(t / 3600).toString().padStart(2,'0');
  const m = Math.floor((t % 3600) / 60).toString().padStart(2,'0');
  const s = (t % 60).toString().padStart(2,'0');
  return h+':'+m+':'+s;
}

function escHtml(s) {
  // AUDIT FIX: added quotes for complete XSS protection (HTML attributes)
  return String(s)
    .replace(/&/g,'&amp;')
    .replace(/</g,'&lt;')
    .replace(/>/g,'&gt;')
    .replace(/"/g,'&quot;')
    .replace(/'/g,'&#39;');
}

async function loadLogs() {
  try {
    const r = await api('/api/logs');
    if (r && r.entries) {
      logCache = r.entries;
      renderLogs();
      const info = document.getElementById('log-count-info');
      if (info) info.textContent = logCache.length + ' entry(ies) — ' + r.count + ' total';
    }
  } catch(e) {}
}

function renderLogs() {
  const minLevel  = parseInt(document.getElementById('log-level-filter')?.value  || '0');
  const catFilter = parseInt(document.getElementById('log-cat-filter')?.value     || '-1');
  const tbody = document.getElementById('log-table');
  if (!tbody) return;

  const filtered = logCache.filter(e => e.lvl >= minLevel && (catFilter < 0 || e.cat === catFilter));

  if (filtered.length === 0) {
    tbody.innerHTML = '<tr><td colspan="4" style="color:var(--fg2)">No entries</td></tr>';
    return;
  }

  let html = '';
  for (const e of filtered) {
    html += '<tr>';
    html += '<td style="font-size:11px;font-family:monospace;white-space:nowrap">' + formatLogTime(e.t) + '</td>';
    html += '<td>' + logLevelBadge(e.lvl) + '</td>';
    html += '<td>' + logCatBadge(e.cat) + '</td>';
    html += '<td style="font-size:12px">' + escHtml(e.msg) + '</td>';
    html += '</tr>';
  }
  tbody.innerHTML = html;

  // AUDIT FIX: robust selector via dedicated .log-container class
  if (document.getElementById('log-autoscroll')?.checked) {
    const wrap = tbody.closest('.log-container');
    if (wrap) wrap.scrollTop = wrap.scrollHeight;
  }
}

async function clearLogs() {
  if (!await appConfirm('Clear log', 'Delete all system log entries?', {danger:true, icon:'\ud83d\uddd1\ufe0f'})) return;
  api('/api/logs/clear', 'POST', {}).then(r => {
    if (r && r.ok) {
      logCache = [];
      renderLogs();
      toast('Log cleared', 'ok');
    } else {
      toast('Error', 'error');
    }
  });
}

// ============================================================================
// Expert collapsible toggle
// ============================================================================
function toggleExpert(btn) {
  btn.classList.toggle('open');
  const body = btn.nextElementSibling;
  if (body) body.classList.toggle('open');
}

// Collapsible sections (simplified UI)
function toggleCollapse(btn) {
  btn.classList.toggle('open');
  const body = btn.nextElementSibling;
  if (body) body.classList.toggle('open');
}

// ============================================================================
// Instrument Wizard
// ============================================================================
let wizStep = 1;

function openWizard() {
  wizStep = 1;
  document.getElementById('wiz-name').value = '';
  document.getElementById('wiz-channel').value = instruments ? Math.min(instruments.length + 1, 16) : 1;
  document.getElementById('wiz-type').value = '0';
  wizUpdateBehaviors();
  document.getElementById('wiz-count').value = '8';
  document.getElementById('wiz-start-note').value = '48';
  document.getElementById('wiz-scale').value = 'chromatic';
  document.getElementById('wiz-note-table').innerHTML = '';
  document.getElementById('wiz-pca').value = '64';
  // Auto-detect next free PCA channel
  let startCh = 0;
  if (actuators && actuators.length > 0) {
    const sorted = [...actuators].sort((a,b) =>
      a.pca_addr !== b.pca_addr ? a.pca_addr - b.pca_addr : a.pca_ch - b.pca_ch);
    const last = sorted[sorted.length - 1];
    startCh = last.pca_ch + 1;
    if (startCh > 15) { startCh = 0; }
    document.getElementById('wiz-pca').value = last.pca_addr;
  }
  document.getElementById('wiz-start-ch').value = startCh;
  wizShowStep();
  document.getElementById('modal-wizard').classList.add('show');
}

function wizUpdateBehaviors() {
  const type = document.getElementById('wiz-type').value;
  const sel = document.getElementById('wiz-behavior');
  sel.innerHTML = '';
  const behaviors = type === '0'
    ? [{v:0,t:'Strike (quick back-and-forth)'},{v:1,t:'Alternate (toggle A/B)'},{v:2,t:'Strum (continuous motion)'},{v:3,t:'Key (hold while pressed)'}]
    : [{v:0,t:'Strike (short pulse)'},{v:1,t:'Hit-and-Hold (strike then hold)'}];
  for (const b of behaviors) {
    const opt = document.createElement('option');
    opt.value = b.v;
    opt.textContent = b.t;
    sel.appendChild(opt);
  }
}

function wizBuildNoteTable() {
  const count = Math.min(parseInt(document.getElementById('wiz-count').value) || 8, 64);
  const startNote = parseInt(document.getElementById('wiz-start-note').value) || 48;
  const scaleKey = document.getElementById('wiz-scale').value;
  const container = document.getElementById('wiz-note-table');
  if (!container) return;

  const scale = SCALES[scaleKey];
  let notes = [];
  if (scale) {
    const iv = scale.intervals;
    let idx = 0, octave = 0;
    for (let i = 0; i < count; i++) {
      if (idx >= iv.length) { idx = 0; octave++; }
      notes.push(Math.min(startNote + octave * 12 + iv[idx], 127));
      idx++;
    }
  } else {
    for (let i = 0; i < count; i++) notes.push(Math.min(startNote + i, 127));
  }

  let html = '<table><thead><tr><th style="width:60px">#</th><th style="width:80px">Note</th><th>Name</th></tr></thead><tbody>';
  for (let i = 0; i < count; i++) {
    html += '<tr>';
    html += '<td>Act ' + i + '</td>';
    html += '<td><input type="number" min="0" max="127" value="' + notes[i] + '" id="wiz-note-' + i + '" class="note-input" oninput="wizUpdateNoteName(' + i + ')"></td>';
    html += '<td id="wiz-nname-' + i + '">' + noteName(notes[i]) + '</td>';
    html += '</tr>';
  }
  html += '</tbody></table>';
  container.innerHTML = html;
}

function wizUpdateNoteName(i) {
  const input = document.getElementById('wiz-note-' + i);
  const label = document.getElementById('wiz-nname-' + i);
  if (input && label) {
    const n = parseInt(input.value);
    label.textContent = (n >= 0 && n <= 127) ? noteName(n) : '?';
  }
}

function wizShowStep() {
  for (let i = 1; i <= 4; i++) {
    document.getElementById('wiz-step-' + i).style.display = i === wizStep ? 'block' : 'none';
    const dot = document.getElementById('wiz-dot-' + i);
    dot.className = 'wiz-dot' + (i === wizStep ? ' active' : i < wizStep ? ' done' : '');
    const item = document.getElementById('wiz-item-' + i);
    if (item) item.className = 'wiz-step-item' + (i === wizStep ? ' active' : '');
    const conn = document.getElementById('wiz-conn-' + i);
    if (conn) conn.className = 'wiz-connector' + (i < wizStep ? ' done' : '');
  }
  // Build note table when entering step 3 for the first time
  if (wizStep === 3 && !document.getElementById('wiz-note-table').querySelector('table')) {
    wizBuildNoteTable();
  }
  document.getElementById('wiz-prev').style.display = wizStep > 1 ? 'inline-flex' : 'none';
  const nextBtn = document.getElementById('wiz-next');
  if (wizStep === 4) {
    nextBtn.textContent = 'Create';
    nextBtn.className = 'btn primary';
    wizBuildSummary();
  } else {
    nextBtn.textContent = 'Next \u2192';
    nextBtn.className = 'btn primary';
  }
}

function wizBuildSummary() {
  const name = document.getElementById('wiz-name').value || 'Instrument';
  const ch = document.getElementById('wiz-channel').value;
  const type = document.getElementById('wiz-type').value;
  const typeName = type === '0' ? 'Servo motor' : 'Solenoid';
  const behavior = document.getElementById('wiz-behavior').selectedOptions[0]?.textContent || '';
  const count = parseInt(document.getElementById('wiz-count').value);
  const pca = document.getElementById('wiz-pca').selectedOptions[0]?.textContent || '';
  const startCh = document.getElementById('wiz-start-ch').value;

  // Read notes from table
  let noteNames = [];
  for (let i = 0; i < count; i++) {
    const inp = document.getElementById('wiz-note-' + i);
    if (inp) noteNames.push(noteName(parseInt(inp.value)));
  }
  const noteStr = noteNames.length <= 12
    ? noteNames.join(', ')
    : noteNames.slice(0, 10).join(', ') + ' ... (+' + (noteNames.length - 10) + ')';

  let html = '<strong>' + esc(name) + '</strong> — MIDI Channel ' + (ch === '0' ? 'Omni (all)' : ch) + '<br>';
  html += 'Type: <strong>' + typeName + '</strong> — ' + behavior + '<br>';
  html += count + ' actuators<br>';
  html += 'Notes: ' + noteStr + '<br>';
  html += 'PCA: ' + pca + ' — Channels ' + startCh + ' \u2192 ' + (parseInt(startCh) + count - 1) + '<br>';
  html += 'Bus: <strong>' + (type === '0' ? 'Bus 0 (Servos)' : 'Bus 1 (Solenoids)') + '</strong>';
  document.getElementById('wiz-summary').innerHTML = html;
}

function wizPrev() {
  if (wizStep > 1) { wizStep--; wizShowStep(); }
}

async function wizNext() {
  // Per-step validation before advancing
  if (wizStep === 1) {
    const name = document.getElementById('wiz-name').value.trim();
    if (!name) { toast('Please enter a name for the instrument', 'error'); return; }
  }
  if (wizStep === 3) {
    const count = parseInt(document.getElementById('wiz-count').value);
    if (isNaN(count) || count < 1 || count > 64) { toast('Invalid actuator count (1–64)', 'error'); return; }
  }
  if (wizStep < 4) { wizStep++; wizShowStep(); return; }
  // Step 4 → Create everything in ONE transactional request.
  const name = document.getElementById('wiz-name').value.trim() || 'Instrument';
  const channel = parseInt(document.getElementById('wiz-channel').value);
  const type = parseInt(document.getElementById('wiz-type').value);
  const behavior = parseInt(document.getElementById('wiz-behavior').value);
  const count = parseInt(document.getElementById('wiz-count').value);
  const startNote = parseInt(document.getElementById('wiz-start-note').value);
  let pcaAddr = parseInt(document.getElementById('wiz-pca').value);
  let pcaCh = parseInt(document.getElementById('wiz-start-ch').value);
  const busId = type === 0 ? 0 : 1;

  // AUDIT FIX (UI-P0): build the whole request and let the backend validate and
  // create everything atomically (IDs auto-assigned). No client-side partial
  // state, no orphaned actuators on failure.
  const acts = [];
  for (let i = 0; i < count; i++) {
    const noteInput = document.getElementById('wiz-note-' + i);
    const note = noteInput ? parseInt(noteInput.value) : (startNote + i);
    const a = { type, bus_id: busId, pca_addr: pcaAddr, pca_ch: pcaCh,
                latency_ms: 10, behavior, enabled: true,
                note: (isNaN(note) ? -1 : note) };
    if (type === 0) { a.angle_init = 90; a.amplitude = 45; a.speed_ms = 150; a.angle_b = 120; }
    else { a.pulse_min_ms = 5; a.pulse_ms = 30; a.pwm_initial = 4095; a.pwm_hold = 2048; a.ramp_ms = 50; }
    acts.push(a);
    pcaCh++;
    if (pcaCh > 15) { pcaCh = 0; pcaAddr = Math.min(pcaAddr + 1, 67); }
  }

  const wizBtn = document.getElementById('wiz-next');
  if (wizBtn) wizBtn.disabled = true;
  const resp = await api('/api/setup/instrument', 'POST',
    { name, channel, bus_id: busId, latency_ms: 10, auto_cal: false, enabled: true, actuators: acts });
  if (wizBtn) wizBtn.disabled = false;

  if (!resp || !resp.ok) {
    // api() already toasted the backend error message.
    await appAlert('Wizard failed', 'The instrument could not be created — nothing was changed.');
    return;
  }

  closeModal('modal-wizard');
  toast(name + ' created with ' + count + ' actuators', 'ok');

  // Refresh everything
  await loadActuators();
  routing = await api('/api/routing') || [];
  loadHomeInstruments();
  loadInstrumentSelects();
  buildAllPianos();
  updateCountBadges();

  // After creation, always go to instrument page (exit welcome if needed)
  showPage('instrument');
}

// ============================================================================
// Servo angle visual preview
// ============================================================================
function updateAnglePreview() {
  const preview = document.getElementById('angle-preview');
  if (!preview || document.getElementById('servo-fields').style.display === 'none') return;
  const mode = document.getElementById('ma-servo-behavior').value;
  const isAlterne = mode === '1';
  const cx=60,cy=55,r=40;
  const toX=(deg)=>cx+r*Math.cos((180-deg)*Math.PI/180);
  const toY=(deg)=>cy-r*Math.sin((180-deg)*Math.PI/180);
  let s='<svg width="120" height="70" viewBox="0 0 120 70">';
  s+='<path d="M '+(cx-r)+' '+cy+' A '+r+' '+r+' 0 0 1 '+(cx+r)+' '+cy+'" fill="none" stroke="var(--bg3)" stroke-width="3"/>';

  if (isAlterne) {
    // Alternate: show angle A and angle B as two positions
    const angleA = parseInt(document.getElementById('ma-angle-a-alt').value) || 90;
    const angleB = parseInt(document.getElementById('ma-angle-b').value) || 120;
    const lo = Math.min(angleA, angleB), hi = Math.max(angleA, angleB);
    const la=(hi-lo)>180?1:0;
    const x1=toX(lo),y1=toY(lo),x2=toX(hi),y2=toY(hi);
    s+='<path d="M '+x1+' '+y1+' A '+r+' '+r+' 0 '+la+' 1 '+x2+' '+y2+'" fill="none" stroke="var(--yellow)" stroke-width="3"/>';
    const xa=toX(angleA),ya=toY(angleA),xb=toX(angleB),yb=toY(angleB);
    s+='<line x1="'+cx+'" y1="'+cy+'" x2="'+xa+'" y2="'+ya+'" stroke="var(--green)" stroke-width="2"/>';
    s+='<line x1="'+cx+'" y1="'+cy+'" x2="'+xb+'" y2="'+yb+'" stroke="var(--yellow)" stroke-width="2"/>';
    s+='<circle cx="'+cx+'" cy="'+cy+'" r="3" fill="var(--fg)"/>';
    s+='<text x="2" y="68" font-size="9" fill="var(--fg2)">180</text>';
    s+='<text x="104" y="68" font-size="9" fill="var(--fg2)">0</text>';
    s+='</svg>';
    s+='<div class="angle-info">';
    s+='A: <span style="color:var(--green)">'+angleA+'&deg;</span>';
    s+=' B: <span style="color:var(--yellow)">'+angleB+'&deg;</span>';
    s+='</div>';
  } else {
    // Strike / Strum / Key: show idle + amplitude arc
    const init = parseInt(document.getElementById('ma-angle-init').value) || 90;
    const amp = parseInt(document.getElementById('ma-amplitude').value) || 45;
    const reverse = document.getElementById('ma-hit-reverse').value === '1';
    let minA, maxA;
    if (mode === '2') { // Strum: bidirectional
      minA = Math.max(0, init - amp);
      maxA = Math.min(180, init + amp);
    } else if (reverse) {
      minA = Math.max(0, init - amp);
      maxA = init;
    } else {
      minA = init;
      maxA = Math.min(180, init + amp);
    }
    const la=(maxA-minA)>180?1:0;
    const x1=toX(minA),y1=toY(minA),x2=toX(maxA),y2=toY(maxA);
    const xi=toX(init),yi=toY(init);
    s+='<path d="M '+x1+' '+y1+' A '+r+' '+r+' 0 '+la+' 1 '+x2+' '+y2+'" fill="none" stroke="var(--accent)" stroke-width="3"/>';
    s+='<line x1="'+cx+'" y1="'+cy+'" x2="'+xi+'" y2="'+yi+'" stroke="var(--green)" stroke-width="2"/>';
    s+='<circle cx="'+cx+'" cy="'+cy+'" r="3" fill="var(--fg)"/>';
    s+='<text x="2" y="68" font-size="9" fill="var(--fg2)">180</text>';
    s+='<text x="104" y="68" font-size="9" fill="var(--fg2)">0</text>';
    s+='</svg>';
    s+='<div class="angle-info">';
    s+='Idle: <span>'+init+'&deg;</span>';
    s+=' Range: <span>'+minA+'&deg;&rarr;'+maxA+'&deg;</span>';
    s+='</div>';
  }
  preview.innerHTML=s;
}

// ============================================================================
// Wiring — electrical diagram generated from the live configuration
// ============================================================================
// Everything is drawn from /api/buses + /api/actuators + /api/instruments, so
// the diagram can never drift from what the firmware actually drives.
//
// Drawing rules (kept strict so the result stays readable):
//   * channel colour   = instrument, channel shape = actuator type
//   * one supply block per actuator family (servo rail / solenoid rail),
//     plus a single GND spine acting as the star point
//   * every wire owns a private vertical lane and every horizontal run sits on
//     a y of its own, so two wires can cross but never overlap
const WIRE_NS = 'http://www.w3.org/2000/svg';
const WIRE_C = {
  bg:'#0d1117', panel:'#161b22', panel2:'#21262d', line:'#30363d',
  fg:'#c9d1d9', fg2:'#8b949e',
  sda:'#58a6ff', scl:'#a371f7', oe:'#d29922',
  vps:'#f85149', vpsol:'#db6d28', gnd:'#8b949e',
  servo:'#58a6ff', sol:'#d29922', bad:'#f85149', ok:'#3fb950', aux:'#39c5cf'
};
const WIRE_MONO = 'ui-monospace,SFMono-Regular,Menlo,Consolas,monospace';
// One colour per instrument index (MAX_INSTRUMENTS = 8).
const WIRE_INST_C = ['#58a6ff','#3fb950','#d29922','#a371f7','#39c5cf','#db6d28','#ec6cb9','#e3b341'];

// Compile-time pins (config.h) — not configurable at runtime.
const WIRE_FIXED_PINS = [
  {sig:'MIDI RX (Serial2)', gpio:4,  dir:'Input',  color:WIRE_C.ok,
   note:'DIN / TRS MIDI in, 31250 baud — 6N138 opto-coupler strongly recommended'},
  {sig:'Status LED',        gpio:2,  dir:'Output', color:WIRE_C.ok,
   note:'ESP32 built-in LED'},
  {sig:'I2S WS (mic)',      gpio:15, dir:'Output', color:WIRE_C.aux, opt:true,
   note:'INMP441 — optional, acoustic calibration only'},
  {sig:'I2S SCK (mic)',     gpio:14, dir:'Output', color:WIRE_C.aux, opt:true,
   note:'INMP441 — optional'},
  {sig:'I2S SD (mic)',      gpio:32, dir:'Input',  color:WIRE_C.aux, opt:true,
   note:'INMP441 — optional (L/R pin to GND = left channel)'}
];

// Current estimates mirror config.h (POWER_* / SAFETY_*).
const WIRE_SERVO_MA = 250, WIRE_SOL_MA = 500, WIRE_IDLE_MA = 30;
const WIRE_MAX_PCA_PER_BUS = 4;

let wiringModel = null;

async function loadWiring() {
  const [buses, acts, power, insts] = await Promise.all([
    api('/api/buses'), api('/api/actuators'), api('/api/power'), api('/api/instruments')
  ]);
  if (acts) actuators = acts;
  if (insts) instruments = insts;
  wiringModel = buildWiringModel(buses || [], acts || actuators || [], power, insts || instruments || []);
  renderWiringDiagram(wiringModel);
  renderWiringLegend(wiringModel);
  renderWiringCards(wiringModel);
  renderWiringBoards(wiringModel);
  renderWiringIssues(wiringModel);
  renderWiringPins(wiringModel);
  renderWiringPower(wiringModel);
}

// --- Model -----------------------------------------------------------------
function buildWiringModel(buses, acts, power, insts) {
  const m = { buses: [], instruments: [], boards: 0, servo: 0, sol: 0, used: 0,
              disabled: 0, orphans: 0,
              power: (power && power.budget) ? power.budget : null, issues: [] };
  const byId = {};

  // actuator id -> instrument (+ its MIDI note)
  const owner = {};
  for (const inst of (insts || [])) {
    const rec = { idx: inst.index, name: inst.name || ('Instrument ' + inst.index),
                  color: WIRE_INST_C[inst.index % WIRE_INST_C.length],
                  busId: inst.bus_id, enabled: inst.enabled !== false,
                  servo: 0, sol: 0, count: 0, offBus: 0 };
    m.instruments.push(rec);
    for (const a of (inst.actuators || [])) owner[a.id] = { inst: rec, note: a.note };
  }

  for (const b of buses) {
    const bus = { id: b.id, sda: b.sda, scl: b.scl, oe: b.oe,
                  freqI2c: b.freq_i2c, freqPwm: b.freq_pwm, enabled: !!b.enabled,
                  declared: (b.pca_addrs || []).slice(),
                  boards: [], servo: 0, sol: 0, ghost: false };
    byId[b.id] = bus;
    m.buses.push(bus);
  }

  function busOf(id) {
    if (!byId[id]) {
      // An actuator points at a bus the device never reported — keep it visible.
      byId[id] = { id: id, sda: null, scl: null, oe: null, freqI2c: 0, freqPwm: 0,
                   enabled: false, declared: [], boards: [], servo: 0, sol: 0, ghost: true };
      m.buses.push(byId[id]);
    }
    return byId[id];
  }

  function boardOf(busId, addr) {
    const bus = busOf(busId);
    let bd = null;
    for (const x of bus.boards) if (x.addr === addr) bd = x;
    if (!bd) {
      bd = { bus: busId, addr: addr, declared: bus.declared.indexOf(addr) >= 0,
             ch: [], servo: 0, sol: 0, used: 0, conflicts: 0, stray: [], insts: [] };
      for (let i = 0; i < 16; i++) bd.ch.push([]);
      bus.boards.push(bd);
    }
    return bd;
  }

  // Declared boards first so an empty (but wired) board still shows up.
  for (const bus of m.buses) for (const a of bus.declared) boardOf(bus.id, a);

  for (const a of (acts || [])) {
    const bd = boardOf(a.bus_id, a.pca_addr);
    const bus = byId[a.bus_id];
    const own = owner[a.id] || null;
    const slot = { id: a.id, type: a.type, enabled: a.enabled !== false,
                   inst: own ? own.inst : null,
                   note: (own && own.note !== undefined && own.note !== 255) ? own.note : null };
    if (a.type === 0) { m.servo++; bus.servo++; bd.servo++; if (own) own.inst.servo++; }
    else { m.sol++; bus.sol++; bd.sol++; if (own) own.inst.sol++; }
    if (own) {
      own.inst.count++;
      if (own.inst.busId !== a.bus_id) own.inst.offBus++;
    } else m.orphans++;
    if (!slot.enabled) m.disabled++;

    // instruments present on this board (ordered by first appearance)
    if (bd.insts.indexOf(slot.inst) < 0) bd.insts.push(slot.inst);

    if (a.pca_ch >= 0 && a.pca_ch < 16) {
      bd.ch[a.pca_ch].push(slot);
      if (bd.ch[a.pca_ch].length === 1) { bd.used++; m.used++; }
      else bd.conflicts++;
    } else {
      bd.stray.push(slot);   // channel out of the 0-15 range
    }
  }

  m.buses.sort((x, y) => x.id - y.id);
  for (const bus of m.buses) {
    bus.boards.sort((x, y) => x.addr - y.addr);
    m.boards += bus.boards.length;
  }
  m.issues = findWiringIssues(m);
  return m;
}

function hex2(v) { return '0x' + ('0' + v.toString(16).toUpperCase()).slice(-2); }
function boardKind(b) { return (b.servo && b.sol) ? 'MIXED' : (b.sol ? 'SOLENOID' : 'SERVO'); }
function boardKindColor(b) {
  return (b.servo && b.sol) ? WIRE_C.bad : (b.sol ? WIRE_C.sol : WIRE_C.servo);
}
function instName(i) { return i ? i.name : 'unassigned'; }
function instColor(i) { return i ? i.color : WIRE_C.fg2; }

function findWiringIssues(m) {
  const out = [];
  const err  = (t) => out.push({ lvl: 'err',  txt: t });
  const warn = (t) => out.push({ lvl: 'warn', txt: t });

  for (const bus of m.buses) {
    const label = 'Bus ' + bus.id;
    if (bus.ghost) {
      err(label + ' is referenced by ' + (bus.servo + bus.sol) + ' actuator(s) but the device '
        + 'reports no such bus. Move them to bus 0 or 1 (Actuators page).');
      continue;
    }
    const acts = bus.servo + bus.sol;
    if (!bus.enabled && acts > 0)
      err(label + ' is disabled but drives ' + acts + ' actuator(s) — they will never move. '
        + 'Enable the bus in Settings → Buses.');
    if (bus.servo > 0 && bus.freqPwm > 130)
      err(label + ' runs at ' + bus.freqPwm + ' Hz and drives ' + bus.servo + ' servo(s). '
        + 'Servos need ~50 Hz — move them to a 50 Hz bus or lower the frequency.');
    if (bus.servo > 0 && bus.sol > 0)
      warn(label + ' mixes ' + bus.servo + ' servo(s) and ' + bus.sol + ' solenoid(s). A PCA9685 has '
        + 'one PWM frequency per board chain: keep servos on one bus and solenoids on the other.');
    if (bus.sol > 0 && bus.servo === 0 && bus.freqPwm <= 60)
      warn(label + ' drives solenoids at ' + bus.freqPwm + ' Hz. 200–1000 Hz gives a smoother '
        + 'hold current and avoids audible buzz.');
    if (bus.boards.length > WIRE_MAX_PCA_PER_BUS)
      err(label + ' declares ' + bus.boards.length + ' PCA9685 boards — the firmware supports '
        + WIRE_MAX_PCA_PER_BUS + ' per bus (' + (WIRE_MAX_PCA_PER_BUS * 16) + ' channels).');
    if (bus.sda === bus.scl && bus.sda !== null)
      err(label + ': SDA and SCL are both on GPIO ' + bus.sda + '.');

    for (const bd of bus.boards) {
      const who = label + ' / PCA ' + hex2(bd.addr);
      if (!bd.declared)
        err(who + ' is used by an actuator but is not declared on the bus. Add the board in '
          + 'Settings → Buses, or the firmware will not initialise it.');
      if (bd.conflicts > 0) {
        const dup = [];
        for (let c = 0; c < 16; c++) if (bd.ch[c].length > 1) {
          const ids = bd.ch[c].map(s => '#' + s.id).join(', ');
          dup.push('ch ' + c + ' (' + ids + ')');
        }
        err(who + ': several actuators share the same output — ' + dup.join(', ')
          + '. One channel drives one actuator.');
      }
      if (bd.stray.length)
        err(who + ': ' + bd.stray.length + ' actuator(s) use a channel outside 0–15.');
      if (bd.servo > 0 && bd.sol > 0)
        err(who + ' carries ' + bd.servo + ' servo(s) and ' + bd.sol + ' solenoid(s). A board has a '
          + 'single V+ terminal and a single PWM frequency — it cannot be fed by the 5–6 V servo '
          + 'rail and the 12–24 V solenoid rail at the same time. Split them onto two boards.');
    }
  }

  for (const inst of m.instruments) {
    if (inst.offBus > 0)
      warn('Instrument "' + inst.name + '" is declared on bus ' + inst.busId + ' but ' + inst.offBus
        + ' of its actuator(s) sit on another bus.');
    if (inst.servo > 0 && inst.sol > 0)
      warn('Instrument "' + inst.name + '" mixes servos and solenoids — check that both families '
        + 'end up on their own bus and their own supply rail.');
  }
  if (m.orphans > 0)
    warn(m.orphans + ' actuator(s) belong to no instrument. They are wired but nothing can play '
      + 'them until a MIDI note maps to them.');

  const peak = m.servo * WIRE_SERVO_MA + m.sol * WIRE_SOL_MA;
  const budget = m.power ? m.power.global_max_ma : 0;
  if (budget && peak > budget)
    warn('All-actuators-at-once draw is ~' + (peak / 1000).toFixed(1) + ' A, above the '
      + (budget / 1000).toFixed(1) + ' A energy budget. The scheduler limits polyphony to '
      + m.power.max_polyphony + ' simultaneous notes, but size the supply and fuses for the real '
      + 'worst case you expect.');

  if (m.servo + m.sol === 0)
    out.push({ lvl: 'warn', txt: 'No actuator configured yet — the diagram only shows the buses. '
      + 'Add actuators (or run the wizard) to get the full wiring.' });
  else if (!out.length)
    out.push({ lvl: 'ok', txt: 'No wiring problem detected in the current configuration.' });
  return out;
}

// --- SVG helpers -----------------------------------------------------------
function wEl(tag, attrs, parent) {
  const e = document.createElementNS(WIRE_NS, tag);
  for (const k in attrs) e.setAttribute(k, attrs[k]);
  if (parent) parent.appendChild(e);
  return e;
}
function wText(parent, x, y, str, o) {
  o = o || {};
  const t = wEl('text', { x: x, y: y, fill: o.fill || WIRE_C.fg,
    'font-size': o.size || 12, 'font-family': o.font || WIRE_MONO,
    'font-weight': o.weight || 400, 'text-anchor': o.anchor || 'start' }, parent);
  t.textContent = str;
  return t;
}
function wTip(parent, str) { const t = wEl('title', {}, parent); t.textContent = str; }
function wPath(parent, d, color, dash, width) {
  return wEl('path', { d: d, fill: 'none', stroke: color, 'stroke-width': width || 2,
    'stroke-linejoin': 'round', 'stroke-dasharray': dash || 'none' }, parent);
}
function wDot(parent, x, y, color) { wEl('circle', { cx: x, cy: y, r: 3.5, fill: color }, parent); }

// Router: every wire gets a private vertical lane, so two wires may cross but
// never run on top of each other.
function wireRouter(x0, step) {
  return { x0: x0, step: step, n: 0, next: function () { return this.x0 + (this.n++) * this.step; } };
}
// Left column pins live on a shared y grid; nudge any duplicate so two
// horizontal runs never share a y either.
function wireYPicker() {
  const used = {};
  return function (y) {
    while (used[y]) y += 3;
    used[y] = true;
    return y;
  };
}

// --- Diagram ---------------------------------------------------------------
function renderWiringDiagram(m) {
  const host = document.getElementById('wiring-diagram');
  if (!host) return;
  host.innerHTML = '';
  if (!m.buses.length) {
    host.innerHTML = '<div class="wire-empty">No I&sup2;C bus reported by the device.</div>';
    return;
  }

  const BW = 214, BH = 152, BGAP = 28;
  const LEFT_X = 16, LEFT_W = 212, LEFT_R = LEFT_X + LEFT_W;
  const LANE0 = LEFT_R + 14, LANE_STEP = 8;
  const RAIL_LBL = 210;   // room for the rail labels on the right
  const PIN_STEP = 21;

  // ---- which rails each bus needs (a bus with nothing gets the servo rail)
  const railPlan = [];
  for (const bus of m.buses) {
    const keys = ['sda', 'scl', 'oe'];
    if (bus.servo > 0 || bus.sol === 0) keys.push('vps');
    if (bus.sol > 0) keys.push('vpsol');
    keys.push('gnd');
    railPlan.push({ bus: bus, keys: keys });
  }

  // ---- lane budget, known before anything is drawn
  let nLanes = 1;                              // GND spine
  for (const rp of railPlan) {
    if (!rp.bus.ghost) nLanes += 3;            // SDA / SCL / OE
    if (rp.keys.indexOf('vps') >= 0) nLanes++;
    if (rp.keys.indexOf('vpsol') >= 0) nLanes++;
  }
  nLanes += WIRE_FIXED_PINS.length;
  const RAIL_X = LANE0 + nLanes * LANE_STEP + 20;
  const BOARD_X0 = RAIL_X + 30;

  // Aux blocks (MIDI in / microphone / LED) — their own band at the bottom.
  const AUX_BOXES = [
    { title: 'MIDI IN (DIN/TRS)', sub: '6N138 opto — GND to star', pins: ['MIDI RX'], w: 210 },
    { title: 'INMP441 mic (optional)', sub: 'I2S — 3V3 + GND to star',
      pins: ['I2S WS', 'I2S SCK', 'I2S SD'], w: 220 },
    { title: 'Status LED', sub: 'on-board', pins: ['Status LED'], w: 160 }
  ];
  let auxW = BOARD_X0 + 24;
  for (const b of AUX_BOXES) auxW += b.w + 30;

  // ---- vertical layout: one band per bus, then the aux band
  const bands = [];
  let y = 34, maxW = 0;
  for (const rp of railPlan) {
    const n = Math.max(rp.bus.boards.length, 1);
    const boardTop = y + 38, boardBot = boardTop + BH;
    const band = { bus: rp.bus, keys: rp.keys, top: y, boardTop: boardTop,
                   boardBot: boardBot, rails: {} };
    let ry = boardBot + 30;
    for (const k of rp.keys) {
      if (k === 'vps' || k === 'vpsol') ry += (band.rails.vps || band.rails.vpsol) ? 0 : 8;
      band.rails[k] = ry;
      ry += 19;
    }
    band.railEnd = BOARD_X0 + n * BW + (n - 1) * BGAP + 22;
    band.bottom = ry + 6;
    band.width = band.railEnd + RAIL_LBL;
    maxW = Math.max(maxW, band.width);
    bands.push(band);
    y = band.bottom + 26;
  }
  const auxTop = y;
  const auxBoxTop = auxTop + 34 + WIRE_FIXED_PINS.length * 6 + 10;
  const auxBoxH = 120;
  const auxH = (auxBoxTop - auxTop) + auxBoxH + 16;
  y = auxTop + auxH + 20;
  maxW = Math.max(maxW, auxW);

  // ---- left column: ESP32, one supply per actuator family, GND star spine
  const pickY = wireYPicker();
  const pins = [];
  for (const rp of railPlan) {
    const bus = rp.bus;
    if (bus.ghost) continue;
    pins.push({ label: 'SDA' + bus.id + '  GPIO ' + bus.sda, color: WIRE_C.sda, key: 'sda', bus: bus });
    pins.push({ label: 'SCL' + bus.id + '  GPIO ' + bus.scl, color: WIRE_C.scl, key: 'scl', bus: bus });
    pins.push({ label: '/OE' + bus.id + '  GPIO ' + bus.oe,  color: WIRE_C.oe,  key: 'oe',  bus: bus });
  }
  for (const p of WIRE_FIXED_PINS)
    pins.push({ label: p.sig.replace(' (Serial2)', '') + '  GPIO ' + p.gpio,
                color: p.color, dash: p.opt ? '4 3' : null, fixed: p });
  pins.push({ label: 'GND', color: WIRE_C.gnd, gnd: true });

  const espTop = 34;
  const espH = 44 + pins.length * PIN_STEP;
  for (let i = 0; i < pins.length; i++) pins[i].y = pickY(espTop + 38 + i * PIN_STEP);

  const needServoPsu = bands.some(b => b.keys.indexOf('vps') >= 0);
  const needSolPsu   = bands.some(b => b.keys.indexOf('vpsol') >= 0);
  const supplies = [];
  let sTop = espTop + espH + 32;
  if (needServoPsu) supplies.push({ key: 'vps', color: WIRE_C.vps,
    title: 'Servo supply', sub: '5–6 V DC' });
  if (needSolPsu) supplies.push({ key: 'vpsol', color: WIRE_C.vpsol,
    title: 'Solenoid supply', sub: '12–24 V DC' });
  for (const sup of supplies) {
    sup.outs = [];
    for (const band of bands)
      if (band.keys.indexOf(sup.key) >= 0)
        sup.outs.push({ band: band, key: sup.key, color: sup.color,
                        label: 'V+ → bus ' + band.bus.id });
    sup.outs.push({ gnd: true, color: WIRE_C.gnd, label: 'GND → star' });
    sup.top = sTop;
    sup.h = 48 + sup.outs.length * PIN_STEP;
    for (let i = 0; i < sup.outs.length; i++) sup.outs[i].y = pickY(sup.top + 42 + i * PIN_STEP);
    sTop = sup.top + sup.h + 26;
  }

  const H = Math.max(y, sTop + 10);
  const W = Math.ceil(maxW + 16);

  const svg = wEl('svg', { xmlns: WIRE_NS, viewBox: '0 0 ' + W + ' ' + H,
    width: W, height: H, class: 'wire-svg' }, host);
  wEl('rect', { x: 0, y: 0, width: W, height: H, fill: WIRE_C.bg }, svg);

  // ---- ESP32 box
  const esp = wEl('g', {}, svg);
  wEl('rect', { x: LEFT_X, y: espTop, width: LEFT_W, height: espH, rx: 10,
    fill: WIRE_C.panel, stroke: WIRE_C.sda, 'stroke-width': 1.5 }, esp);
  wText(esp, LEFT_X + 14, espTop + 24, 'ESP32-WROOM-32', { weight: 700, size: 13 });
  for (const p of pins) {
    wText(esp, LEFT_R - 16, p.y + 4, p.label, { anchor: 'end', size: 11, fill: WIRE_C.fg2 });
    wEl('rect', { x: LEFT_R - 10, y: p.y - 4, width: 10, height: 8, fill: p.color }, esp);
  }

  // ---- supply boxes
  for (const sup of supplies) {
    const g = wEl('g', {}, svg);
    wEl('rect', { x: LEFT_X, y: sup.top, width: LEFT_W, height: sup.h, rx: 10,
      fill: WIRE_C.panel, stroke: sup.color, 'stroke-width': 1.5 }, g);
    wText(g, LEFT_X + 14, sup.top + 22, sup.title, { weight: 700, size: 13 });
    wText(g, LEFT_X + 14, sup.top + 38, sup.sub, { size: 11, fill: WIRE_C.fg2 });
    for (const o of sup.outs) {
      wText(g, LEFT_R - 16, o.y + 4, o.label, { anchor: 'end', size: 11, fill: WIRE_C.fg2 });
      wEl('rect', { x: LEFT_R - 10, y: o.y - 4, width: 10, height: 8, fill: o.color }, g);
    }
  }

  // ---- routing. The GND spine takes the last lane (closest to the rails), the
  // router hands out the ones before it — one per wire, never reused.
  const spine = LANE0 + (nLanes - 1) * LANE_STEP;
  const router = wireRouter(LANE0, LANE_STEP);
  const wires = wEl('g', {}, svg);
  const gndTaps = [];     // y of every GND source on the left column
  const gndRails = [];    // y of every band GND rail

  for (const p of pins) if (p.gnd) gndTaps.push(p.y);
  for (const sup of supplies) for (const o of sup.outs) if (o.gnd) gndTaps.push(o.y);

  for (const band of bands) {
    // ESP32 signals → bus rails
    for (const p of pins) {
      if (p.bus !== band.bus) continue;
      const lane = router.next();
      wPath(wires, 'M' + LEFT_R + ',' + p.y + ' H' + lane + ' V' + band.rails[p.key]
        + ' H' + RAIL_X, p.color);
    }
    // supplies → V+ rails
    for (const sup of supplies) for (const o of sup.outs) {
      if (o.band !== band) continue;
      const lane = router.next();
      wPath(wires, 'M' + LEFT_R + ',' + o.y + ' H' + lane + ' V' + band.rails[o.key]
        + ' H' + RAIL_X, o.color, null, 3);
    }
    gndRails.push(band.rails.gnd);
  }

  // ---- GND spine (the star point): one vertical line, every ground taps on it
  const allGnd = gndTaps.concat(gndRails);
  const gTop = Math.min.apply(null, allGnd), gBot = Math.max.apply(null, allGnd);
  wPath(wires, 'M' + spine + ',' + gTop + ' V' + gBot, WIRE_C.gnd, null, 3);
  for (const ty of gndTaps) {
    wPath(wires, 'M' + LEFT_R + ',' + ty + ' H' + spine, WIRE_C.gnd, null, 3);
    wDot(wires, spine, ty, WIRE_C.gnd);
  }
  for (const ry of gndRails) {
    wPath(wires, 'M' + spine + ',' + ry + ' H' + RAIL_X, WIRE_C.gnd, null, 3);
    wDot(wires, spine, ry, WIRE_C.gnd);
  }
  // Label sits in the gap under the last band, where nothing else is drawn.
  wText(wires, spine + 6, gBot + 18, 'GND star point', { size: 10, fill: WIRE_C.gnd });

  // ---- bands
  const RAIL_LABEL = {
    sda: (b) => 'SDA' + b.id + (b.ghost ? '' : ' — GPIO ' + b.sda),
    scl: (b) => 'SCL' + b.id + (b.ghost ? '' : ' — GPIO ' + b.scl),
    oe:  (b) => '/OE' + b.id + (b.ghost ? '' : ' — GPIO ' + b.oe),
    vps: () => 'V+ servo — 5-6 V',
    vpsol: () => 'V+ solenoid — 12-24 V',
    gnd: () => 'GND — star point'
  };
  const RAIL_COLOR = { sda: WIRE_C.sda, scl: WIRE_C.scl, oe: WIRE_C.oe,
                       vps: WIRE_C.vps, vpsol: WIRE_C.vpsol, gnd: WIRE_C.gnd };

  for (const band of bands) {
    const bus = band.bus;
    const g = wEl('g', {}, svg);
    wEl('rect', { x: RAIL_X - 16, y: band.top, width: band.width - RAIL_X + 8,
      height: band.bottom - band.top, rx: 10, fill: 'none',
      stroke: WIRE_C.line, 'stroke-dasharray': '5 4' }, g);

    let title = 'Bus ' + bus.id + '  —  ' + bus.boards.length + ' board'
      + (bus.boards.length === 1 ? '' : 's') + '  —  ' + bus.servo + ' servo / '
      + bus.sol + ' solenoid';
    if (!bus.ghost) title += '  —  PWM ' + bus.freqPwm + ' Hz, I2C '
      + Math.round(bus.freqI2c / 1000) + ' kHz';
    if (bus.ghost) title += '  —  UNKNOWN BUS';
    else if (!bus.enabled) title += '  —  DISABLED';
    wText(g, RAIL_X - 6, band.top + 20, title,
      { size: 12, weight: 700, fill: (bus.ghost || !bus.enabled) ? WIRE_C.bad : WIRE_C.fg });

    for (const k of band.keys) {
      const ry = band.rails[k], color = RAIL_COLOR[k];
      wEl('line', { x1: RAIL_X, y1: ry, x2: band.railEnd, y2: ry, stroke: color,
        'stroke-width': (k === 'sda' || k === 'scl' || k === 'oe') ? 2 : 3 }, g);
      wText(g, band.railEnd + 10, ry + 4, RAIL_LABEL[k](bus), { size: 11, fill: color });
    }

    if (!bus.boards.length) {
      wEl('rect', { x: BOARD_X0, y: band.boardTop, width: BW, height: BH, rx: 9,
        fill: 'none', stroke: WIRE_C.line, 'stroke-dasharray': '5 4' }, g);
      wText(g, BOARD_X0 + BW / 2, band.boardTop + BH / 2 + 4, 'no board',
        { anchor: 'middle', size: 12, fill: WIRE_C.fg2 });
    }
    for (let i = 0; i < bus.boards.length; i++)
      drawWiringBoard(g, bus.boards[i], BOARD_X0 + i * (BW + BGAP), band, BW, BH, i);
  }

  // ---- aux band: MIDI in, microphone, status LED
  // Each wire owns a corridor lane above the boxes and enters through the top
  // edge, so it never runs across a neighbouring block.
  const aux = wEl('g', {}, svg);
  wEl('rect', { x: RAIL_X - 16, y: auxTop, width: maxW - RAIL_X + 8, height: auxH, rx: 10,
    fill: 'none', stroke: WIRE_C.line, 'stroke-dasharray': '5 4' }, aux);
  wText(aux, RAIL_X - 6, auxTop + 20, 'Inputs & indicator', { size: 12, weight: 700 });

  let bx = BOARD_X0, corridor = auxTop + 34;
  for (const box of AUX_BOXES) {
    wEl('rect', { x: bx, y: auxBoxTop, width: box.w, height: auxBoxH, rx: 9,
      fill: WIRE_C.panel, stroke: WIRE_C.line }, aux);
    wText(aux, bx + 12, auxBoxTop + 20, box.title, { size: 11, weight: 700 });
    wText(aux, bx + 12, auxBoxTop + 38, box.sub, { size: 10, fill: WIRE_C.fg2 });
    let py = auxBoxTop + 62, px = bx + 26;
    for (const label of box.pins) {
      const pin = pins.filter(p => p.fixed && p.fixed.sig.indexOf(label) === 0)[0];
      if (!pin) continue;
      wText(aux, bx + 12, py, label + '  GPIO ' + pin.fixed.gpio, { size: 10, fill: pin.color });
      wEl('rect', { x: px - 5, y: auxBoxTop - 4, width: 10, height: 8, fill: pin.color }, aux);
      wPath(wires, 'M' + LEFT_R + ',' + pin.y + ' H' + router.next() + ' V' + corridor
        + ' H' + px + ' V' + auxBoxTop, pin.color, pin.dash);
      corridor += 6; py += 16; px += 26;
    }
    bx += box.w + 30;
  }
}

function drawWiringBoard(g, bd, x, band, BW, BH, idx) {
  const top = band.boardTop;
  wEl('rect', { x: x, y: top, width: BW, height: BH, rx: 9, fill: WIRE_C.panel,
    stroke: bd.declared ? WIRE_C.line : WIRE_C.bad, 'stroke-width': bd.declared ? 1 : 1.5,
    'stroke-dasharray': bd.declared ? 'none' : '5 4' }, g);
  wText(g, x + 12, top + 20, 'PCA9685 #' + (idx + 1), { size: 11, weight: 700 });
  wText(g, x + BW - 12, top + 20, hex2(bd.addr),
    { size: 12, weight: 700, anchor: 'end', fill: bd.declared ? WIRE_C.sda : WIRE_C.bad });

  // actuator family carried by this board
  const kind = boardKind(bd), kc = boardKindColor(bd);
  wEl('rect', { x: x + 12, y: top + 28, width: 8 * kind.length + 12, height: 15, rx: 7,
    fill: kc, 'fill-opacity': 0.18, stroke: kc }, g);
  wText(g, x + 18, top + 39, kind, { size: 9, weight: 700, fill: kc });
  wText(g, x + BW - 12, top + 39, bd.used + '/16 ch', { size: 9, anchor: 'end', fill: WIRE_C.fg2 });

  // 16 channels: colour = instrument, shape = actuator type
  const CELL = 20, GAP = 3;
  const gw = 8 * CELL + 7 * GAP;
  const gx = x + (BW - gw) / 2, gy = top + 50;
  for (let c = 0; c < 16; c++) {
    const cx = gx + (c % 8) * (CELL + GAP), cy = gy + Math.floor(c / 8) * (CELL + GAP);
    const slots = bd.ch[c];
    if (!slots.length) {
      const r = wEl('rect', { x: cx, y: cy, width: CELL, height: CELL, rx: 4,
        fill: WIRE_C.panel2, stroke: WIRE_C.line, 'stroke-dasharray': '2 2' }, g);
      wTip(r, 'Channel ' + c + ' — free');
      continue;
    }
    const s = slots[0];
    const conflict = slots.length > 1;
    const color = conflict ? WIRE_C.bad : instColor(s.inst);
    let shape;
    if (s.type === 0) {
      shape = wEl('rect', { x: cx, y: cy, width: CELL, height: CELL, rx: 4, fill: color,
        'fill-opacity': s.enabled ? 1 : 0.3, stroke: conflict ? WIRE_C.bad : color,
        'stroke-width': conflict ? 2 : 1 }, g);
    } else {
      wEl('rect', { x: cx, y: cy, width: CELL, height: CELL, rx: 4, fill: WIRE_C.panel2,
        stroke: WIRE_C.line }, g);
      shape = wEl('circle', { cx: cx + CELL / 2, cy: cy + CELL / 2, r: CELL / 2 - 2.5,
        fill: color, 'fill-opacity': s.enabled ? 1 : 0.3,
        stroke: conflict ? WIRE_C.bad : color, 'stroke-width': conflict ? 2 : 1 }, g);
    }
    let tip = 'Channel ' + c + ' — ' + (s.type === 0 ? 'servo' : 'solenoid') + ' #' + s.id
            + ' — ' + instName(s.inst);
    if (s.note !== null && s.note !== undefined) tip += ' — note ' + noteName(s.note);
    if (!s.enabled) tip += ' (disabled)';
    if (conflict) tip = 'Channel ' + c + ' — CONFLICT: ' + slots.map(o => '#' + o.id).join(', ');
    wTip(shape, tip);
  }

  // which instruments live on this board
  let ly = top + 108;
  const shown = bd.insts.slice(0, 3);
  for (const inst of shown) {
    let n = 0;
    for (let c = 0; c < 16; c++) for (const s of bd.ch[c]) if (s.inst === inst) n++;
    for (const s of bd.stray) if (s.inst === inst) n++;
    wEl('rect', { x: x + 12, y: ly - 7, width: 8, height: 8, rx: 2, fill: instColor(inst) }, g);
    const label = instName(inst) + ' (' + n + ')';
    wText(g, x + 24, ly, label.length > 24 ? label.slice(0, 23) + '…' : label,
      { size: 10, fill: WIRE_C.fg2 });
    ly += 14;
  }
  if (bd.insts.length > shown.length)
    wText(g, x + 24, ly, '+' + (bd.insts.length - shown.length) + ' more',
      { size: 10, fill: WIRE_C.fg2 });
  if (!bd.insts.length)
    wText(g, x + 12, ly, 'no actuator wired', { size: 10, fill: WIRE_C.fg2 });

  // board terminals down to the rails it belongs to
  const drops = [
    { key: 'sda', color: WIRE_C.sda, dx: 24 },
    { key: 'scl', color: WIRE_C.scl, dx: 48 },
    { key: 'oe',  color: WIRE_C.oe,  dx: 72 }
  ];
  if (bd.servo > 0 || bd.sol === 0) drops.push({ key: 'vps', color: WIRE_C.vps, dx: BW - 70 });
  if (bd.sol > 0) drops.push({ key: 'vpsol', color: WIRE_C.vpsol, dx: BW - 46 });
  drops.push({ key: 'gnd', color: WIRE_C.gnd, dx: BW - 22 });
  for (const d of drops) {
    const ry = band.rails[d.key];
    if (ry === undefined) continue;
    const dx = x + d.dx;
    wEl('line', { x1: dx, y1: top + BH, x2: dx, y2: ry, stroke: d.color,
      'stroke-width': (d.key === 'gnd' || d.key === 'vps' || d.key === 'vpsol') ? 3 : 2 }, g);
    wDot(g, dx, ry, d.color);
  }
}

// --- Panels ----------------------------------------------------------------
function renderWiringLegend(m) {
  const host = document.getElementById('wiring-legend');
  if (!host) return;
  let html = '';
  html += '<span><i style="border-radius:3px;background:' + WIRE_C.fg2 + '"></i>Servo channel (square)</span>';
  html += '<span><i style="border-radius:50%;background:' + WIRE_C.fg2 + '"></i>Solenoid channel (disc)</span>';
  html += '<span><i style="background:' + WIRE_C.panel2 + ';border:1px dashed ' + WIRE_C.line + '"></i>Free channel</span>';
  html += '<span><i style="background:' + WIRE_C.bad + '"></i>Conflict</span>';
  html += '<span style="opacity:.55"><i style="background:' + WIRE_C.fg2 + '"></i>Faded = disabled</span>';
  html += '<span style="flex-basis:100%;height:0"></span>';
  for (const inst of m.instruments)
    html += '<span><i style="background:' + inst.color + '"></i>' + esc(inst.name) + '</span>';
  if (m.orphans) html += '<span><i style="background:' + WIRE_C.fg2 + '"></i>Unassigned</span>';
  html += '<span style="flex-basis:100%;height:0"></span>';
  html += '<span><i class="line" style="background:' + WIRE_C.sda + '"></i>SDA</span>';
  html += '<span><i class="line" style="background:' + WIRE_C.scl + '"></i>SCL</span>';
  html += '<span><i class="line" style="background:' + WIRE_C.oe + '"></i>/OE</span>';
  html += '<span><i class="line" style="background:' + WIRE_C.vps + '"></i>V+ servo 5–6 V</span>';
  html += '<span><i class="line" style="background:' + WIRE_C.vpsol + '"></i>V+ solenoid 12–24 V</span>';
  html += '<span><i class="line" style="background:' + WIRE_C.gnd + '"></i>GND star point</span>';
  host.innerHTML = html;
}

function renderWiringCards(m) {
  const host = document.getElementById('wiring-cards');
  if (!host) return;
  const servoA = m.servo * WIRE_SERVO_MA, solA = m.sol * WIRE_SOL_MA;
  const budget = m.power ? m.power.global_max_ma : 0;
  const pct = budget ? Math.min(100, Math.round((servoA + solA) / budget * 100)) : 0;
  const capacity = m.boards * 16;
  let html = '';
  html += '<div class="card"><h3>Actuators</h3><div class="val">' + (m.servo + m.sol) + '</div>'
       + '<div class="sub">' + m.servo + ' servo &middot; ' + m.sol + ' solenoid'
       + (m.disabled ? ' &middot; ' + m.disabled + ' disabled' : '') + '</div></div>';
  html += '<div class="card"><h3>Instruments</h3><div class="val">' + m.instruments.length + '</div>'
       + '<div class="sub">' + (m.orphans ? m.orphans + ' actuator(s) unassigned'
                                          : 'every actuator is assigned') + '</div></div>';
  html += '<div class="card"><h3>PCA9685 boards</h3><div class="val">' + m.boards + '</div>'
       + '<div class="sub">' + m.used + ' / ' + capacity + ' channels wired</div></div>';
  html += '<div class="card"><h3>Peak current</h3><div class="val">'
       + ((servoA + solA) / 1000).toFixed(1) + '<span class="unit">A</span></div>'
       + '<div class="bar"><div class="bar-fill" style="width:' + pct + '%;background:'
       + (pct > 80 ? 'var(--red)' : 'var(--green)') + '"></div></div>'
       + '<div class="sub">Servo rail ' + (servoA / 1000).toFixed(1) + ' A &middot; solenoid rail '
       + (solA / 1000).toFixed(1) + ' A</div></div>';
  host.innerHTML = html;
}

function renderWiringBoards(m) {
  const tbody = document.getElementById('wiring-boards');
  if (!tbody) return;
  let html = '';
  for (const bus of m.buses) {
    for (const bd of bus.boards) {
      const kind = boardKind(bd);
      const kindBadge = kind === 'MIXED'
        ? '<span class="badge" style="background:#f8514933;color:var(--red)">MIXED</span>'
        : '<span class="badge ' + (kind === 'SERVO' ? 'servo' : 'sol') + '">' + kind + '</span>';
      let insts = '';
      for (const inst of bd.insts) {
        let n = 0;
        for (let c = 0; c < 16; c++) for (const s of bd.ch[c]) if (s.inst === inst) n++;
        insts += '<span style="display:inline-block;width:8px;height:8px;border-radius:2px;'
              + 'background:' + instColor(inst) + ';margin:0 5px 0 0"></span>'
              + esc(instName(inst)) + ' (' + n + ') ';
      }
      if (!insts) insts = '<span style="color:var(--fg2)">&mdash;</span>';
      let rail = kind === 'SOLENOID' ? '12&ndash;24 V' : (kind === 'SERVO' ? '5&ndash;6 V'
               : '<span style="color:var(--red)">conflict: needs both</span>');
      html += '<tr><td>Bus ' + bus.id + '</td>'
           + '<td><strong>' + hex2(bd.addr) + '</strong>'
           + (bd.declared ? '' : ' <span class="badge off" style="color:var(--red)">undeclared</span>')
           + '</td>'
           + '<td>' + kindBadge + '</td>'
           + '<td>' + insts + '</td>'
           + '<td>' + bd.used + ' / 16'
           + (bd.conflicts ? ' <span style="color:var(--red)">(' + bd.conflicts + ' conflict)</span>' : '')
           + '</td>'
           + '<td>' + rail + '</td></tr>';
    }
  }
  tbody.innerHTML = html || '<tr><td colspan="6" style="color:var(--fg2)">No board configured</td></tr>';
}

function renderWiringIssues(m) {
  const host = document.getElementById('wiring-issues');
  if (!host) return;
  const icon = { err: '❌', warn: '⚠️', ok: '✅' };
  let html = '';
  for (const i of m.issues)
    html += '<div class="wire-issue ' + i.lvl + '"><span class="ico">' + icon[i.lvl]
         + '</span><span>' + esc(i.txt) + '</span></div>';
  host.innerHTML = html;
}

function renderWiringPins(m) {
  const tbody = document.getElementById('wiring-pins');
  if (!tbody) return;
  let html = '';
  for (const bus of m.buses) {
    if (bus.ghost) continue;
    const rows = [
      ['SDA' + bus.id, bus.sda, 'I/O', 'Bus ' + bus.id + ' data — 2.2–4.7 kΩ pull-up to 3.3 V'],
      ['SCL' + bus.id, bus.scl, 'Output', 'Bus ' + bus.id + ' clock — ' + Math.round(bus.freqI2c / 1000) + ' kHz'],
      ['/OE' + bus.id, bus.oe, 'Output', 'Output Enable, LOW = outputs live. 10 kΩ pull-up so the boards stay off at boot']
    ];
    for (const r of rows)
      html += '<tr><td><strong>' + r[0] + '</strong></td><td>GPIO ' + r[1] + '</td><td>' + r[2]
           + '</td><td style="color:var(--fg2)">' + esc(r[3]) + '</td></tr>';
  }
  for (const p of WIRE_FIXED_PINS)
    html += '<tr><td><strong>' + esc(p.sig) + '</strong></td><td>GPIO ' + p.gpio + '</td><td>'
         + p.dir + '</td><td style="color:var(--fg2)">' + esc(p.note) + '</td></tr>';
  html += '<tr><td><strong>GND</strong></td><td>GND</td><td>&mdash;</td>'
       + '<td style="color:var(--fg2)">Tie to the GND star point together with both supply '
       + 'grounds — the PWM signal has no reference otherwise</td></tr>';
  tbody.innerHTML = html;
}

function renderWiringPower(m) {
  const host = document.getElementById('wiring-power');
  if (!host) return;
  const servoMa = m.servo * WIRE_SERVO_MA, solMa = m.sol * WIRE_SOL_MA;
  const servoSize = Math.max(1, Math.ceil(servoMa / 1000 * 1.3));
  const solSize = Math.max(1, Math.ceil(solMa / 1000 * 1.3));
  const tips = [];
  tips.push('<b>Two independent supplies.</b> Servo rail 5–6 V for ' + m.servo + ' servo(s) ('
    + m.servo + ' × ' + WIRE_SERVO_MA + ' mA ≈ ' + (servoMa / 1000).toFixed(1) + ' A, size it at ~'
    + servoSize + ' A) and solenoid rail 12–24 V for ' + m.sol + ' solenoid(s) (' + m.sol + ' × '
    + WIRE_SOL_MA + ' mA ≈ ' + (solMa / 1000).toFixed(1) + ' A, size it at ~' + solSize
    + ' A). Never power actuators from the ESP32 5 V pin.');
  tips.push('<b>One family per board.</b> A PCA9685 has a single V+ terminal and a single PWM '
    + 'frequency, so a board is either fully servo or fully solenoid — that is what the board '
    + 'badges above show.');
  tips.push('<b>Star ground.</b> ESP32 GND, both supply grounds and every PCA9685 GND meet at one '
    + 'point (the GND spine in the diagram). Daisy-chaining grounds through the actuator wiring '
    + 'is what causes phantom triggers.');
  tips.push('<b>Bulk capacitance.</b> 470–1000 µF low-ESR across V+ / GND at each PCA9685, plus a '
    + '100 nF ceramic close to the board — servo inrush is what causes random ESP32 resets.');
  tips.push('<b>Fuse each rail</b> at roughly 1.5 × its expected draw, and keep the V+ / GND wiring '
    + 'thick (the PCA9685 screw terminal carries the full board current, not the traces).');
  tips.push('<b>Solenoid flyback.</b> Each solenoid needs its own flyback diode across the coil, '
    + 'and a MOSFET / Darlington driver board between the PCA9685 output and the coil — a PCA '
    + 'output cannot sink the collapse current.');
  tips.push('<b>/OE is the kill switch.</b> Wire it through the emergency stop so opening the E-stop '
    + 'pulls /OE HIGH and cuts every output in hardware, independently of the firmware.');
  tips.push('<b>Logic level.</b> The PCA9685 accepts 3.3 V logic directly; power its VCC (logic) '
    + 'from 3.3 V and keep the actuator V+ terminal fully separate.');
  let html = '<ul class="wire-tips">';
  for (const t of tips) html += '<li>' + t + '</li>';
  html += '</ul>';
  host.innerHTML = html;
}

// Serialise the generated diagram to a standalone .svg file.
function downloadWiringSVG() {
  const svg = document.querySelector('#wiring-diagram svg');
  if (!svg) { toast('Nothing to export yet', 'error'); return; }
  const clone = svg.cloneNode(true);
  clone.setAttribute('xmlns', WIRE_NS);
  clone.removeAttribute('class');
  const src = '<?xml version="1.0" encoding="UTF-8"?>\n'
            + new XMLSerializer().serializeToString(clone);
  const blob = new Blob([src], { type: 'image/svg+xml' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url; a.download = 'playmode-wiring.svg';
  document.body.appendChild(a); a.click(); document.body.removeChild(a);
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}

// ============================================================================
// Microphone detection for calibration
// ============================================================================
async function checkMicStatus() {
  const navBtn = document.getElementById('nav-cal');
  const micDiv = document.getElementById('mic-status');
  try {
    const d = await api('/api/calibrate/status');
    if (d && d.available) {
      if (navBtn) navBtn.style.display = '';
      if (micDiv) {
        micDiv.className = 'mic-status ok';
        micDiv.innerHTML = '&#9679; Microphone detected \u2014 Ready for calibration';
      }
    } else {
      if (navBtn) navBtn.style.display = 'none';
    }
  } catch(e) {
    if (navBtn) navBtn.style.display = 'none';
  }
}

// ============================================================================
// Init
// ============================================================================
// Update count badges in collapsible sections
function updateCountBadges() {
  const actBadge = document.getElementById('act-count-badge');
  if (actBadge) actBadge.textContent = actuators ? '(' + actuators.length + ')' : '';
  const ccBadge = document.getElementById('cc-count-badge');
  if (ccBadge && routing) {
    let ccCount = 0;
    for (const r of routing) { if (r.ccs) ccCount += r.ccs.length; }
    ccBadge.textContent = ccCount > 0 ? '(' + ccCount + ')' : '';
  }
}

window.addEventListener('load', async () => {
  // AUDIT FIX (P0.9): obtain the AP auth token before any write can happen.
  await fetchAuthToken();
  connectWS();
  // Preload data then decide which page to show
  await loadActuators();
  instruments = await api('/api/instruments') || [];
  routing = await api('/api/routing') || [];
  loadInstrumentSelects();
  loadHomeInstruments_render();
  buildAllPianos();
  updateCountBadges();
  checkMicStatus();

  // First-run detection: show welcome only if truly no instruments
  if (!instruments || instruments.length === 0) {
    showPage('welcome');
  } else {
    showPage('instrument');
  }
});
</script>
</body>
</html>
)rawhtml";

#endif // WEB_UI_H
