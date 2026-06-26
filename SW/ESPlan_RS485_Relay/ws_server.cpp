/*
 * ws_server.cpp – HTTP webserver pro RS485 rele modul (8 / 16 kanalu)
 */

#include "ws_server.h"
#include <ETH.h>

// ─── Globalni pointery ────────────────────────────────────────────────────────
static WebServer*     gServer = nullptr;
static RelayState*    gState  = nullptr;
static RelayConfig*   gCfg    = nullptr;
static Preferences*   gPrefs  = nullptr;
static ModbusClient*  gMb     = nullptr;

// ─── Pomocne ─────────────────────────────────────────────────────────────────
static bool isLangCz() {
  if (gServer->hasArg("lang")) {
    String l = gServer->arg("lang");
    return !(l == "en");
  }
  if (gServer->hasHeader("Cookie")) {
    String c = gServer->header("Cookie");
    if (c.indexOf("lang=en") >= 0) return false;
  }
  return true;
}

static void sendLangCookie(bool cz) {
  gServer->sendHeader("Set-Cookie",
    cz ? "lang=cs; Path=/; Max-Age=31536000"
       : "lang=en; Path=/; Max-Age=31536000");
}

// ─── CSS ─────────────────────────────────────────────────────────────────────
static const char CSS[] PROGMEM = R"CSS(
:root{
  --bg:#0d1117;--bg2:#161b22;--bg3:#21262d;
  --border:#30363d;--border2:#484f58;
  --text:#e6edf3;--text2:#8b949e;--text3:#6e7681;
  --accent:#58a6ff;--accent2:#1f6feb;
  --green:#3fb950;--red:#f85149;--yellow:#d29922;--purple:#a371f7;
}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--text);font-family:'Segoe UI',system-ui,sans-serif;
  min-height:100vh;font-size:14px;line-height:1.5}
a{color:var(--accent);text-decoration:none}
a:hover{text-decoration:underline}
.topbar{background:var(--bg2);border-bottom:1px solid var(--border);
  padding:0 24px;display:flex;align-items:center;height:52px;
  position:sticky;top:0;z-index:100}
.logo{display:flex;align-items:center;gap:10px;font-weight:700;font-size:15px;
  color:var(--text);margin-right:32px;white-space:nowrap}
.logo-icon{width:28px;height:28px;background:#1f6feb;border-radius:6px;
  display:flex;align-items:center;justify-content:center;flex-shrink:0;color:#fff;font-weight:700}
.nav-links{display:flex;gap:2px;flex:1}
.nav-link{padding:6px 14px;border-radius:6px;font-size:13px;
  color:var(--text2);transition:background .15s,color .15s;white-space:nowrap}
.nav-link:hover{background:var(--bg3);color:var(--text);text-decoration:none}
.nav-link.active{background:var(--accent2);color:#fff}
.lang-btn{margin-left:auto;padding:5px 12px;border-radius:20px;
  background:var(--bg3);border:1px solid var(--border2);
  color:var(--text2);font-size:12px;white-space:nowrap}
.lang-btn:hover{background:var(--border);color:var(--text);text-decoration:none}
.main{max-width:1100px;margin:0 auto;padding:28px 20px}
h1{font-size:20px;font-weight:600;margin-bottom:20px}
h2{font-size:12px;font-weight:600;color:var(--text3);text-transform:uppercase;
  letter-spacing:.08em;margin:28px 0 12px;padding-bottom:8px;
  border-bottom:1px solid var(--border)}
h2:first-of-type{margin-top:0}
.card{background:var(--bg2);border:1px solid var(--border);
  border-radius:10px;padding:16px 20px;margin-bottom:16px}
table{width:100%;border-collapse:collapse;font-size:13px}
th{padding:8px 12px;text-align:left;font-weight:600;font-size:11px;
  color:var(--text3);text-transform:uppercase;letter-spacing:.06em;
  border-bottom:1px solid var(--border);white-space:nowrap}
td{padding:9px 12px;border-bottom:1px solid var(--border);vertical-align:middle}
tr:last-child td{border-bottom:none}
tr:hover td{background:rgba(255,255,255,.02)}
.ch-num{font-family:'Courier New',monospace;color:var(--text3);width:32px;text-align:center}
.state-on{display:inline-flex;align-items:center;gap:6px;color:var(--green);font-weight:600;font-size:12px}
.state-off{display:inline-flex;align-items:center;gap:6px;color:var(--text3);font-size:12px}
.dot{width:10px;height:10px;border-radius:50%;flex-shrink:0}
.dot-g{background:var(--green);box-shadow:0 0 8px var(--green)}
.dot-x{background:var(--text3)}
.err-banner{background:rgba(248,81,73,.1);border:1px solid var(--red);
  border-radius:8px;padding:10px 16px;color:var(--red);font-size:13px;
  margin-bottom:16px;display:flex;align-items:center;gap:8px}
.info-banner{background:rgba(88,166,255,.08);border:1px solid var(--border);
  border-radius:8px;padding:12px 16px;color:var(--text2);font-size:12px;
  margin-bottom:16px;line-height:1.6}
.ip-bar{display:flex;align-items:center;gap:20px;padding:10px 16px;
  background:var(--bg3);border:1px solid var(--border);border-radius:8px;
  font-family:'Courier New',monospace;font-size:12px;color:var(--text2);
  margin-bottom:20px;flex-wrap:wrap}
.ip-lbl{color:var(--text3)}
.btn{display:inline-flex;align-items:center;gap:6px;padding:6px 12px;
  border-radius:6px;font-size:12px;cursor:pointer;border:1px solid transparent;
  font-family:inherit;transition:background .15s;text-decoration:none}
.btn:active{opacity:.8}
.btn-p{background:var(--accent2);color:#fff;border-color:var(--accent2)}
.btn-p:hover{background:#388bfd;text-decoration:none;color:#fff}
.btn-d{background:var(--bg3);color:var(--text);border-color:var(--border2)}
.btn-d:hover{background:var(--border);text-decoration:none}
.btn-w{background:transparent;color:var(--yellow);border-color:var(--yellow)}
.btn-w:hover{background:rgba(210,153,34,.1);text-decoration:none}
.btn-pp{background:transparent;color:var(--purple);border-color:var(--purple)}
.btn-pp:hover{background:rgba(163,113,247,.1);text-decoration:none;color:var(--purple)}
.btn-g{background:var(--green);color:#fff;border-color:var(--green)}
.btn-g:hover{background:#46c95a;text-decoration:none;color:#fff}
.btn-r{background:var(--red);color:#fff;border-color:var(--red)}
.btn-r:hover{background:#ff5e54;text-decoration:none;color:#fff}
.btn-sm{padding:4px 10px;font-size:11px}
.actions-row{display:flex;gap:5px;flex-wrap:wrap}
.bulk-row{display:flex;gap:12px;flex-wrap:wrap}
.fg{margin-bottom:14px}
label{display:block;font-size:13px;font-weight:500;color:var(--text2);margin-bottom:5px}
.ht{font-size:11px;color:var(--text3);margin-top:4px}
input[type=number],input[type=text],select{
  background:var(--bg3);border:1px solid var(--border2);border-radius:6px;
  padding:7px 10px;color:var(--text);font-size:13px;width:100%;
  max-width:280px;outline:none;transition:border-color .15s;font-family:inherit}
input[type=number]{max-width:120px}
input:focus,select:focus{border-color:var(--accent)}
select option{background:var(--bg3)}
.frow{display:flex;gap:16px;flex-wrap:wrap}
.frow .fg{flex:1;min-width:200px}
.name-input{max-width:180px;font-size:12px;padding:5px 8px}
.adv-row{display:flex;align-items:end;gap:8px;flex-wrap:wrap}
.adv-row .fg{margin-bottom:0;min-width:0}
.adv-inline{display:inline-flex;align-items:center;gap:6px}
.adv-inline input[type=number]{width:80px;max-width:80px;padding:5px 8px;font-size:12px}
.smeta{font-size:12px;color:var(--text3);font-family:'Courier New',monospace}
.raw-panel{background:var(--bg2);border:1px solid var(--border);border-radius:10px;
  padding:14px 20px;margin-bottom:16px;font-family:'Courier New',monospace;
  font-size:12px;line-height:1.8}
.raw-tx{color:var(--yellow)}
.raw-rx{color:var(--green)}
.raw-err{color:var(--red)}
.raw-label{color:var(--text3);display:inline-block;width:28px}
.raw-status{display:inline-block;padding:1px 6px;border-radius:3px;font-size:10px;
  font-weight:600;text-transform:uppercase;margin-left:8px}
.raw-status.ok{background:rgba(63,185,80,.15);color:var(--green)}
.raw-status.err{background:rgba(248,81,73,.15);color:var(--red)}
@media(max-width:640px){
  .topbar{padding:0 12px}
  .logo{margin-right:12px}
  .main{padding:16px 12px}
  th,td{padding:7px 6px}
  .actions-row{gap:3px}
  .btn{padding:5px 8px;font-size:11px}
  input,select{max-width:100%}
}
)CSS";

// ─── HTML head + nav ─────────────────────────────────────────────────────────
static String htmlHead(const I18nStrings& s, bool cz, const char* page) {
  String langUrl = cz ? "?lang=en" : "?lang=cs";
  String out;
  out.reserve(3000);

  out += F("<!DOCTYPE html><html lang='");
  out += cz ? "cs" : "en";
  out += F("'><head><meta charset='UTF-8'>"
           "<meta name='viewport' content='width=device-width,initial-scale=1'>"
           "<title>");
  out += s.title;
  out += F("</title><style>");
  out += FPSTR(CSS);
  out += F("</style></head><body>"
           "<nav class='topbar'>"
           "<div class='logo'>"
           "<div class='logo-icon'>R</div>");
  out += s.title;
  out += F("</div><div class='nav-links'>");
  out += F("<a href='/' class='nav-link");
  if (strcmp(page,"status")==0) out += F(" active");
  out += F("'>");
  out += s.navStatus;
  out += F("</a><a href='/config' class='nav-link");
  if (strcmp(page,"config")==0) out += F(" active");
  out += F("'>");
  out += s.navConfig;
  out += F("</a></div><a href='");
  out += langUrl;
  out += F("' class='lang-btn'>");
  out += s.langSwitch;
  out += F("</a></nav><div class='main'>");
  return out;
}

static String htmlFoot() {
  return F("</div></body></html>");
}

// ─── Stranka: ovladani ────────────────────────────────────────────────────────
static void handleRoot() {
  bool cz = isLangCz();
  sendLangCookie(cz);
  const I18nStrings& s = getStrings(cz);

  String html = htmlHead(s, cz, "status");

  // IP bar
  html += F("<div class='ip-bar'>"
            "<span><span class='ip-lbl'>IP: </span><b>");
  html += ETH.localIP().toString();
  html += F("</b></span><span><span class='ip-lbl'>MAC: </span>");
  html += ETH.macAddress();
  html += F("</span><span><span class='ip-lbl'>Modbus: </span>");
  html += gCfg->modbusAddr;
  html += F(" @ ");
  html += gCfg->baudRate;
  html += F(" bps</span><span><span class='ip-lbl'>CH: </span>");
  html += gCfg->channelCount;
  html += F("</span></div>");

  if (gState->commError) {
    html += F("<div class='err-banner'>&#9888; ");
    html += s.commError;
    html += F(": ");
    html += gState->errorMsg;
    html += F("</div>");
  }

  // Tabulka kanalu
  html += F("<h2>");
  html += s.sectionRelays;
  html += F("</h2><div class='card'><table>"
            "<thead><tr><th class='ch-num'>");
  html += s.colChannel;
  html += F("</th><th>");
  html += s.colName;
  html += F("</th><th>");
  html += s.colState;
  html += F("</th><th>");
  html += s.colCtrl;
  html += F("</th></tr></thead><tbody>");

  String langInp = cz ? "cs" : "en";

  for (uint8_t i = 0; i < gCfg->channelCount; i++) {
    bool on = gState->channels[i];
    html += F("<tr><td class='ch-num'>");
    html += (i + 1);
    html += F("</td><td>");
    html += gCfg->names[i];
    html += F("</td><td id='st_");
    html += i;
    html += F("'>");
    if (on) {
      html += F("<span class='state-on'><span class='dot dot-g'></span>");
      html += s.stateOn;
      html += F("</span>");
    } else {
      html += F("<span class='state-off'><span class='dot dot-x'></span>");
      html += s.stateOff;
      html += F("</span>");
    }
    html += F("</td><td><div class='actions-row'>");

    auto addBtn = [&](const char* act, const char* cls, const char* label) {
      html += F("<form method='POST' action='/relay' style='display:inline'>"
                "<input type='hidden' name='ch' value='");
      html += (i + 1);
      html += F("'><input type='hidden' name='act' value='");
      html += act;
      html += F("'><input type='hidden' name='lang' value='");
      html += langInp;
      html += F("'><button type='submit' class='btn ");
      html += cls;
      html += F(" btn-sm'>");
      html += label;
      html += F("</button></form>");
    };

    addBtn("on",     "btn-g", s.btnOn);
    addBtn("off",    "btn-r", s.btnOff);
    addBtn("toggle", "btn-d", s.btnToggle);

    html += F("</div></td></tr>");
  }
  html += F("</tbody></table></div>");

  // Specialni rezimy (Latch / Momentary / Delay) ako kompaktni panel
  html += F("<h2>");
  html += s.sectionAdvanced;
  html += F("</h2><div class='card'>");
  html += F("<div class='ht' style='margin-bottom:10px'>");
  html += s.labelLatchHelp;
  html += F(" &mdash; ");
  html += s.labelMomentaryHelp;
  html += F(" &mdash; ");
  html += s.labelDelayHelp;
  html += F("</div>");

  // Latch / Momentary / Delay – pro kazdy kanal radek
  html += F("<table><thead><tr><th class='ch-num'>");
  html += s.colChannel;
  html += F("</th><th>");
  html += s.colName;
  html += F("</th><th>");
  html += s.colCtrl;
  html += F("</th></tr></thead><tbody>");
  for (uint8_t i = 0; i < gCfg->channelCount; i++) {
    html += F("<tr><td class='ch-num'>");
    html += (i + 1);
    html += F("</td><td>");
    html += gCfg->names[i];
    html += F("</td><td><div class='actions-row'>");

    // Latch
    html += F("<form method='POST' action='/relay' style='display:inline'>"
              "<input type='hidden' name='ch' value='");
    html += (i + 1);
    html += F("'><input type='hidden' name='act' value='latch'>"
              "<input type='hidden' name='lang' value='");
    html += langInp;
    html += F("'><button type='submit' class='btn btn-w btn-sm'>");
    html += s.btnLatch;
    html += F("</button></form>");

    // Momentary (Pulse)
    html += F("<form method='POST' action='/relay' style='display:inline'>"
              "<input type='hidden' name='ch' value='");
    html += (i + 1);
    html += F("'><input type='hidden' name='act' value='momentary'>"
              "<input type='hidden' name='lang' value='");
    html += langInp;
    html += F("'><button type='submit' class='btn btn-pp btn-sm'>");
    html += s.btnMomentary;
    html += F("</button></form>");

    // Delay s inputem sekund
    html += F("<form method='POST' action='/relay' style='display:inline'>"
              "<input type='hidden' name='ch' value='");
    html += (i + 1);
    html += F("'><input type='hidden' name='act' value='delay'>"
              "<input type='hidden' name='lang' value='");
    html += langInp;
    html += F("'><span class='adv-inline'>"
              "<input type='number' name='sec' min='0' max='255' value='");
    html += gCfg->defaultDelay;
    html += F("'><button type='submit' class='btn btn-w btn-sm'>");
    html += s.btnDelay;
    html += F(" ");
    html += s.labelDelaySec;
    html += F("</button></span></form>");

    html += F("</div></td></tr>");
  }
  html += F("</tbody></table>");
  html += F("</div>");

  // Hromadne ovladani
  html += F("<h2>");
  html += s.sectionActions;
  html += F("</h2><div class='card'><div class='bulk-row'>"
            "<form method='POST' action='/relay'>"
            "<input type='hidden' name='act' value='all_on'>"
            "<input type='hidden' name='lang' value='");
  html += langInp;
  html += F("'><button type='submit' class='btn btn-g'>");
  html += s.btnAllOn;
  html += F("</button></form>"
            "<form method='POST' action='/relay'>"
            "<input type='hidden' name='act' value='all_off'>"
            "<input type='hidden' name='lang' value='");
  html += langInp;
  html += F("'><button type='submit' class='btn btn-r'>");
  html += s.btnAllOff;
  html += F("</button></form>"
            "</div>");
  if (gState->lastAction.length() > 0) {
    html += F("<div class='ht' style='margin-top:10px'>");
    html += s.lastAction;
    html += F(": <span class='smeta'>");
    html += gState->lastAction;
    html += F("</span></div>");
  }
  html += F("</div>");

  // Raw RS485 panel
  html += F("<h2>");
  html += s.sectionRaw;
  html += F("</h2><div class='raw-panel'>"
            "<div><span class='raw-label'>TX:</span> <span class='raw-tx' id='rawTx'>...</span>"
            " <span class='raw-status' id='rawSt'></span></div>"
            "<div><span class='raw-label'>RX:</span> <span class='raw-rx' id='rawRx'>...</span></div>"
            "</div>");

  // JS: refresh stavu + raw panelu
  html += F("<script>"
    "var SON='");
  html += s.stateOn;
  html += F("',SOFF='");
  html += s.stateOff;
  html += F("';"
    "function setSt(i,on){"
      "var e=document.getElementById('st_'+i);"
      "if(!e)return;"
      "e.innerHTML=on?"
        "(\"<span class='state-on'><span class='dot dot-g'></span>\"+SON+\"</span>\"):"
        "(\"<span class='state-off'><span class='dot dot-x'></span>\"+SOFF+\"</span>\");"
    "}"
    "function upd(){"
      "fetch('/api/state').then(function(r){return r.json()}).then(function(d){"
        "if(d.ch){for(var i=0;i<d.ch.length;i++)setSt(i,d.ch[i]);}"
      "}).catch(function(){});"
      "fetch('/api/raw').then(function(r){return r.json()}).then(function(d){"
        "var tx=document.getElementById('rawTx');"
        "var rx=document.getElementById('rawRx');"
        "var st=document.getElementById('rawSt');"
        "if(tx)tx.textContent=d.tx||'-';"
        "if(rx){rx.textContent=d.rx||'-';"
          "rx.className=(d.s==='OK')?'raw-rx':'raw-err';}"
        "if(st){st.textContent=d.s;"
          "st.className='raw-status '+(d.s==='OK'?'ok':'err');}"
      "}).catch(function(){});"
    "}"
    "upd();setInterval(upd,3000);"
  "</script>");

  html += htmlFoot();
  gServer->send(200, "text/html; charset=UTF-8", html);
}

// ─── POST /relay ─────────────────────────────────────────────────────────────
static void handleRelay() {
  bool cz = gServer->hasArg("lang") ? (gServer->arg("lang") != "en") : true;
  String act = gServer->arg("act");
  uint8_t ch = (uint8_t)gServer->arg("ch").toInt();

  if (act == "on" && ch >= 1 && ch <= gCfg->channelCount) {
    relaySet(*gMb, *gState, *gCfg, ch, true);
  } else if (act == "off" && ch >= 1 && ch <= gCfg->channelCount) {
    relaySet(*gMb, *gState, *gCfg, ch, false);
  } else if (act == "toggle" && ch >= 1 && ch <= gCfg->channelCount) {
    relayToggle(*gMb, *gState, *gCfg, ch);
  } else if (act == "latch" && ch >= 1 && ch <= gCfg->channelCount) {
    relayLatch(*gMb, *gState, *gCfg, ch);
  } else if (act == "momentary" && ch >= 1 && ch <= gCfg->channelCount) {
    relayMomentary(*gMb, *gState, *gCfg, ch);
  } else if (act == "delay" && ch >= 1 && ch <= gCfg->channelCount) {
    uint8_t sec = (uint8_t)constrain(gServer->arg("sec").toInt(), 0, 255);
    relayDelay(*gMb, *gState, *gCfg, ch, sec);
  } else if (act == "all_on") {
    relayAll(*gMb, *gState, *gCfg, true);
  } else if (act == "all_off") {
    relayAll(*gMb, *gState, *gCfg, false);
  }

  sendLangCookie(cz);
  gServer->sendHeader("Location", cz ? "/?lang=cs" : "/?lang=en");
  gServer->send(303);
}

// ─── GET /config ──────────────────────────────────────────────────────────────
static void handleConfig() {
  bool cz = isLangCz();
  sendLangCookie(cz);
  const I18nStrings& s = getStrings(cz);

  String html = htmlHead(s, cz, "config");
  html += F("<h1>");
  html += s.pageConfigTitle;
  html += F("</h1>");

  // Info banner o DIP
  html += F("<div class='info-banner'>");
  html += s.infoDip;
  html += F("</div>");

  html += F("<form method='POST' action='/config'>");
  html += F("<input type='hidden' name='lang' value='");
  html += cz ? "cs" : "en";
  html += F("'>");

  // Komunikace
  html += F("<h2>");
  html += s.sectionComm;
  html += F("</h2><div class='card'><div class='frow'>");

  html += F("<div class='fg'><label>");
  html += s.cfgAddr;
  html += F("</label><input type='number' name='addr' min='0' max='47' value='");
  html += gCfg->modbusAddr;
  html += F("'><div class='ht'>");
  html += s.cfgAddrHelp;
  html += F("</div></div>");

  html += F("<div class='fg'><label>");
  html += s.cfgBaud;
  html += F("</label><select name='baud'>");
  const uint32_t bauds[] = {2400, 4800, 9600, 19200};
  for (auto b : bauds) {
    html += F("<option value='");
    html += b;
    html += F("'");
    if (b == gCfg->baudRate) html += F(" selected");
    html += F(">");
    html += b;
    html += F(" bps</option>");
  }
  html += F("</select><div class='ht'>");
  html += s.cfgBaudHelp;
  html += F("</div></div>");

  html += F("<div class='fg'><label>");
  html += s.cfgChannels;
  html += F("</label><select name='chCount'>"
            "<option value='8'");
  if (gCfg->channelCount == 8) html += F(" selected");
  html += F(">8</option><option value='16'");
  if (gCfg->channelCount == 16) html += F(" selected");
  html += F(">16</option></select><div class='ht'>");
  html += s.cfgChannelsHelp;
  html += F("</div></div>");

  html += F("<div class='fg'><label>");
  html += s.cfgDefDelay;
  html += F("</label><input type='number' name='defDelay' min='0' max='255' value='");
  html += gCfg->defaultDelay;
  html += F("'><div class='ht'>");
  html += s.cfgDefDelayHelp;
  html += F("</div></div>");

  html += F("</div></div>");

  // Nazvy kanalu
  html += F("<h2>");
  html += s.sectionNames;
  html += F("</h2><div class='card'><div class='ht' style='margin-bottom:10px'>");
  html += s.cfgNameHelp;
  html += F("</div><div class='frow'>");
  for (uint8_t i = 0; i < gCfg->channelCount; i++) {
    html += F("<div class='fg' style='min-width:170px'><label>#");
    html += (i + 1);
    html += F("</label><input class='name-input' type='text' name='n");
    html += i;
    html += F("' maxlength='15' value='");
    html += gCfg->names[i];
    html += F("'></div>");
  }
  html += F("</div></div>");

  html += F("<div style='margin-top:8px'>"
            "<button type='submit' class='btn btn-p'>");
  html += s.btnSave;
  html += F("</button></div></form>");

  html += htmlFoot();
  gServer->send(200, "text/html; charset=UTF-8", html);
}

// ─── POST /config ─────────────────────────────────────────────────────────────
static void handleConfigPost() {
  bool cz = gServer->hasArg("lang") ? (gServer->arg("lang") != "en") : true;

  if (gServer->hasArg("addr")) {
    gCfg->modbusAddr = (uint8_t)constrain(gServer->arg("addr").toInt(),
                                          MODBUS_ADDR_MIN, MODBUS_ADDR_MAX);
  }
  if (gServer->hasArg("baud")) {
    uint32_t b = (uint32_t)gServer->arg("baud").toInt();
    if (b != gCfg->baudRate) { gCfg->baudRate = b; gMb->setBaudRate(b); }
  }
  if (gServer->hasArg("chCount")) {
    uint8_t c = (uint8_t)gServer->arg("chCount").toInt();
    gCfg->channelCount = (c == 16) ? 16 : 8;
  }
  if (gServer->hasArg("defDelay")) {
    gCfg->defaultDelay = (uint8_t)constrain(gServer->arg("defDelay").toInt(), 0, 255);
  }
  for (uint8_t i = 0; i < MAX_CHANNEL_COUNT; i++) {
    String key = String("n") + i;
    if (gServer->hasArg(key.c_str())) {
      String v = gServer->arg(key.c_str());
      strncpy(gCfg->names[i], v.c_str(), 15);
      gCfg->names[i][15] = 0;
    }
  }

  gPrefs->begin("rs485rel", false);
  saveConfig(*gPrefs, *gCfg);
  gPrefs->end();
  Serial.println(F("[CFG] Konfigurace ulozena."));

  sendLangCookie(cz);
  gServer->sendHeader("Location", cz ? "/config?lang=cs" : "/config?lang=en");
  gServer->send(303);
}

// ─── GET /api/state ──────────────────────────────────────────────────────────
static void handleApiState() {
  String json = "{";
  json += "\"ts\":" + String(gState->lastUpdateMs);
  json += ",\"err\":" + String(gState->commError ? "true" : "false");
  json += ",\"ch\":[";
  for (uint8_t i = 0; i < gCfg->channelCount; i++) {
    if (i > 0) json += ",";
    json += gState->channels[i] ? "true" : "false";
  }
  json += "],\"last\":\"" + gState->lastAction + "\"";
  json += "}";

  gServer->sendHeader("Access-Control-Allow-Origin", "*");
  gServer->send(200, "application/json", json);
}

// ─── GET /api/raw ────────────────────────────────────────────────────────────
static String hexBuf(const uint8_t* buf, uint8_t len) {
  String s;
  s.reserve(len * 3);
  for (uint8_t i = 0; i < len; i++) {
    char h[4];
    snprintf(h, sizeof(h), "%02X ", buf[i]);
    s += h;
  }
  if (s.length() > 0) s.remove(s.length() - 1);
  return s;
}

static void handleApiRaw() {
  String json = "{";
  json += "\"tx\":\"" + hexBuf(gMb->lastTx, gMb->lastTxLen) + "\"";
  json += ",\"rx\":\"" + hexBuf(gMb->lastRx, gMb->lastRxLen) + "\"";
  json += ",\"s\":\"" + String(gMb->statusStr(gMb->lastResult)) + "\"";
  json += "}";
  gServer->sendHeader("Access-Control-Allow-Origin", "*");
  gServer->send(200, "application/json", json);
}

// ─── Inicializace ─────────────────────────────────────────────────────────────
void webServerBegin(RelayState& state, RelayConfig& cfg,
                    Preferences& prefs, ModbusClient& mb) {
  gState = &state;
  gCfg   = &cfg;
  gPrefs = &prefs;
  gMb    = &mb;

  gServer = new WebServer(80);
  static const char* hdrs[] = {"Cookie"};
  gServer->collectHeaders(hdrs, 1);

  gServer->on("/",           HTTP_GET,  handleRoot);
  gServer->on("/relay",      HTTP_POST, handleRelay);
  gServer->on("/config",     HTTP_GET,  handleConfig);
  gServer->on("/config",     HTTP_POST, handleConfigPost);
  gServer->on("/api/state",  HTTP_GET,  handleApiState);
  gServer->on("/api/raw",    HTTP_GET,  handleApiRaw);
  gServer->onNotFound([]() {
    gServer->send(404, "text/plain", "Not found");
  });

  gServer->begin();
  Serial.println(F("[WEB] HTTP server spusten na portu 80"));
}

void webServerHandle() {
  if (gServer) gServer->handleClient();
}
