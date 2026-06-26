/*
 * ws_server.cpp – HTTP webserver implementace
 */

#include "ws_server.h"
#include <ETH.h>

// ─── Globální pointery ────────────────────────────────────────────────────────
static WebServer*     gServer = nullptr;
static SensorData*    gData   = nullptr;
static SensorConfig*  gCfg    = nullptr;
static Preferences*   gPrefs  = nullptr;
static ModbusClient*  gMb     = nullptr;

// ─── Pomocné ─────────────────────────────────────────────────────────────────
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

static String fmtFloat(float v, int dec = 1) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%.*f", dec, v);
  return String(buf);
}

// ─── CSS ─────────────────────────────────────────────────────────────────────
static const char CSS[] PROGMEM = R"CSS(
:root{
  --bg:#0d1117;--bg2:#161b22;--bg3:#21262d;
  --border:#30363d;--border2:#484f58;
  --text:#e6edf3;--text2:#8b949e;--text3:#6e7681;
  --accent:#58a6ff;--accent2:#1f6feb;
  --green:#3fb950;--red:#f85149;--yellow:#d29922;
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
  display:flex;align-items:center;justify-content:center;flex-shrink:0}
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
.val{font-family:'Courier New',monospace;font-size:13px;color:var(--text)}
.unit{color:var(--text3);font-size:12px}
.badge-yes{display:inline-flex;align-items:center;gap:6px;color:var(--green);font-size:12px}
.badge-no{display:inline-flex;align-items:center;gap:6px;color:var(--text3);font-size:12px}
.dot{width:8px;height:8px;border-radius:50%;flex-shrink:0}
.dot-g{background:var(--green);box-shadow:0 0 6px var(--green);
  animation:pulse 2s ease-in-out infinite}
.dot-x{background:var(--text3)}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.4}}
.toolbar{display:flex;align-items:center;gap:8px;margin-bottom:16px;flex-wrap:wrap}
.smeta{margin-left:auto;font-size:12px;color:var(--text3);font-family:'Courier New',monospace}
.err-banner{background:rgba(248,81,73,.1);border:1px solid var(--red);
  border-radius:8px;padding:10px 16px;color:var(--red);font-size:13px;
  margin-bottom:16px;display:flex;align-items:center;gap:8px}
.ip-bar{display:flex;align-items:center;gap:20px;padding:10px 16px;
  background:var(--bg3);border:1px solid var(--border);border-radius:8px;
  font-family:'Courier New',monospace;font-size:12px;color:var(--text2);
  margin-bottom:20px;flex-wrap:wrap}
.ip-lbl{color:var(--text3)}
.btn{display:inline-flex;align-items:center;gap:6px;padding:7px 14px;
  border-radius:6px;font-size:13px;cursor:pointer;border:1px solid transparent;
  font-family:inherit;transition:background .15s;text-decoration:none}
.btn:active{opacity:.8}
.btn-p{background:var(--accent2);color:#fff;border-color:var(--accent2)}
.btn-p:hover{background:#388bfd;text-decoration:none;color:#fff}
.btn-d{background:var(--bg3);color:var(--text);border-color:var(--border2)}
.btn-d:hover{background:var(--border);text-decoration:none}
.btn-w{background:transparent;color:var(--yellow);border-color:var(--yellow)}
.btn-w:hover{background:rgba(210,153,34,.1);text-decoration:none}
.btn-sm{padding:4px 10px;font-size:12px}
.fg{margin-bottom:18px}
label{display:block;font-size:13px;font-weight:500;color:var(--text2);margin-bottom:5px}
.ht{font-size:11px;color:var(--text3);margin-top:4px}
input[type=number],input[type=text],select{
  background:var(--bg3);border:1px solid var(--border2);border-radius:6px;
  padding:7px 10px;color:var(--text);font-size:13px;width:100%;
  max-width:280px;outline:none;transition:border-color .15s;font-family:inherit}
input:focus,select:focus{border-color:var(--accent)}
select option{background:var(--bg3)}
.frow{display:flex;gap:16px;flex-wrap:wrap}
.frow .fg{flex:1;min-width:200px}
.act-row{display:flex;gap:16px;flex-wrap:wrap;align-items:flex-start}
.act-item{flex:1;min-width:200px}
.live-dot{width:8px;height:8px;border-radius:50%;background:var(--green);
  box-shadow:0 0 6px var(--green);animation:pulse 2s ease-in-out infinite;
  flex-shrink:0;margin-left:auto}
.val.flash{animation:valflash .4s ease-out}
@keyframes valflash{0%{color:var(--accent)}100%{color:var(--text)}}
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
  th,td{padding:7px 8px}
  input,select{max-width:100%}
  .smeta{margin-left:0}
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
           "<div class='logo-icon'>"
           "<svg width='18' height='18' viewBox='0 0 18 18' fill='none'>"
           "<circle cx='9' cy='9' r='3' stroke='#58a6ff' stroke-width='1.5'/>"
           "<line x1='9' y1='2' x2='9' y2='5' stroke='#58a6ff' stroke-width='1.5' stroke-linecap='round'/>"
           "<line x1='9' y1='13' x2='9' y2='16' stroke='#58a6ff' stroke-width='1.5' stroke-linecap='round'/>"
           "<line x1='2' y1='9' x2='5' y2='9' stroke='#58a6ff' stroke-width='1.5' stroke-linecap='round'/>"
           "<line x1='13' y1='9' x2='16' y2='9' stroke='#58a6ff' stroke-width='1.5' stroke-linecap='round'/>"
           "</svg></div>");
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

// ─── Stránka: Stav ────────────────────────────────────────────────────────────
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
  html += F("</span><span><span class='ip-lbl'>ETH: </span>");
  html += ETH.linkSpeed();
  html += F(" Mbit/s</span></div>");

  if (gData->commError) {
    html += F("<div class='err-banner'>&#9888; ");
    html += s.commError;
    html += F(": ");
    html += gData->errorMsg;
    html += F("</div>");
  }

  // Toolbar
  html += F("<div class='toolbar'>"
            "<form method='POST' action='/detect' style='display:inline'>"
            "<button type='submit' class='btn btn-d btn-sm'>");
  html += s.btnDetect;
  html += F("</button></form>"
            "<span class='live-dot'></span>"
            "<span id='updInfo' class='smeta'></span>");
  html += F("</div>");

  // Tabulka detekce
  html += F("<h2>");
  html += s.sectionSensors;
  html += F("</h2><div class='card'><table>"
            "<thead><tr><th>");
  html += s.colSensor;
  html += F("</th><th>");
  html += s.colPresent;
  html += F("</th></tr></thead><tbody>");

  auto row = [&](const char* name, bool present) {
    html += F("<tr><td>");
    html += name;
    html += F("</td><td>");
    if (present) {
      html += F("<span class='badge-yes'><span class='dot dot-g'></span>");
      html += s.yes;
    } else {
      html += F("<span class='badge-no'><span class='dot dot-x'></span>");
      html += s.no;
    }
    html += F("</span></td></tr>");
  };

  const SensorPresence& p = gData->present;
  row(s.snsWind,     p.wind);
  row(s.snsTempHum,  p.tempHum);
  row(s.snsPressure, p.pressure);
  row(s.snsNoise,    p.noise);
  row(s.snsPM25,     p.pm25);
  row(s.snsPM10,     p.pm10);
  row(s.snsCO2,      p.co2);
  row(s.snsLight,    p.light);
  row(s.snsRainfall, p.rainfall);
  row(s.snsCompass,  p.compass);
  row(s.snsSolar,    p.solar);
  html += F("</tbody></table></div>");

  // Tabulka hodnot
  html += F("<h2>");
  html += s.sectionValues;
  html += F("</h2><div class='card'><table><thead><tr><th>");
  html += s.colSensor;
  html += F("</th><th>");
  html += s.colValue;
  html += F("</th><th>");
  html += s.colUnit;
  html += F("</th></tr></thead><tbody>");

  auto valRow = [&](const char* id, const char* name, bool present,
                    const String& val, const char* unit) {
    html += F("<tr><td>");
    html += name;
    html += F("</td><td class='val' id='v_");
    html += id;
    html += F("'>");
    html += present ? val : s.noData;
    html += F("</td><td class='unit' id='u_");
    html += id;
    html += F("'>");
    html += present ? unit : "";
    html += F("</td></tr>");
  };

  const SensorData& d = *gData;
  if (p.wind) {
    valRow("ws", s.snsWindSpeed,  true, fmtFloat(d.windSpeed),   "m/s");
    valRow("wf", s.snsWindForce,  true, String(d.windForce),      "Bft");
    String dirTxt = String(d.windDir8) + " (" +
                    s.windDirNames[d.windDir8 < 8 ? d.windDir8 : 0] + ")";
    valRow("wd", s.snsWindDir,    true, dirTxt,                   "");
    valRow("w3", s.snsWindDir360, true, String(d.windDir360),     "&deg;");
  }
  if (p.tempHum) {
    valRow("te", s.snsTemp, true, fmtFloat(d.temperature), "&deg;C");
    valRow("hu", s.snsHum,  true, fmtFloat(d.humidity),    "%RH");
  }
  if (p.pressure)  valRow("pr", s.snsPressure, true, fmtFloat(d.pressure,2), "kPa");
  if (p.noise)     valRow("no", s.snsNoise,    true, fmtFloat(d.noise),      "dB");
  if (p.pm25)      valRow("p2", s.snsPM25,     true, String(d.pm25),         "&micro;g/m&sup3;");
  if (p.pm10)      valRow("p1", s.snsPM10,     true, String(d.pm10),         "&micro;g/m&sup3;");
  if (p.co2)       valRow("co", s.snsCO2,      true, String(d.co2),          "ppm");
  if (p.light)     valRow("lx", s.snsLight,    true, String(d.luxFull),      "lux");
  if (p.rainfall)  valRow("rn", s.snsRainfall, true, fmtFloat(d.rainfall),   "mm");
  if (p.compass)   valRow("cp", s.snsCompass,  true, fmtFloat(d.compass,1),  "&deg;");
  if (p.solar)     valRow("sl", s.snsSolar,    true, String(d.solar),        "W/m&sup2;");

  html += F("</tbody></table></div>");

  // ── Raw RS485 panel ────────────────────────────────────────────────────
  html += F("<h2>");
  html += s.sectionRaw;
  html += F("</h2><div class='raw-panel'>"
            "<div><span class='raw-label'>TX:</span> <span class='raw-tx' id='rawTx'>...</span>"
            " <span class='raw-status' id='rawSt'></span></div>"
            "<div><span class='raw-label'>RX:</span> <span class='raw-rx' id='rawRx'>...</span></div>"
            "</div>");

  // Inline JavaScript – AJAX auto-refresh
  html += F("<script>"
    "var dn=['N','NE','E','SE','S','SW','W','NW'];"
    "function u(id,v){"
      "var e=document.getElementById(id);"
      "if(!e)return;"
      "var s=String(v);"
      "if(e.textContent!==s){"
        "e.textContent=s;"
        "e.classList.remove('flash');"
        "void e.offsetWidth;"
        "e.classList.add('flash');"
      "}"
    "}"
    "function updRaw(){"
      "fetch('/api/raw').then(function(r){return r.json()}).then(function(d){"
        "var tx=document.getElementById('rawTx');"
        "var rx=document.getElementById('rawRx');"
        "var st=document.getElementById('rawSt');"
        "if(tx)tx.textContent=d.tx||'—';"
        "if(rx){rx.textContent=d.rx||'—';"
          "rx.className=(d.s==='OK')?'raw-rx':'raw-err';}"
        "if(st){st.textContent=d.s;"
          "st.className='raw-status '+(d.s==='OK'?'ok':'err');}"
      "}).catch(function(){});"
    "}"
    "function upd(){"
      "fetch('/api/data').then(function(r){return r.json()}).then(function(d){"
        "if(d.err){"
          "document.getElementById('updInfo').textContent='\\u26A0 error';"
          "return;"
        "}"
        "if(d.windSpeed!==undefined){"
          "u('v_ws',d.windSpeed.toFixed(1));"
          "u('v_wf',d.windForce);"
          "u('v_wd',d.windDir8+' ('+(dn[d.windDir8]||'?')+')');"
          "u('v_w3',d.windDir360);"
        "}"
        "if(d.temp!==undefined) u('v_te',d.temp.toFixed(1));"
        "if(d.hum!==undefined) u('v_hu',d.hum.toFixed(1));"
        "if(d.pres!==undefined) u('v_pr',d.pres.toFixed(2));"
        "if(d.noise!==undefined) u('v_no',d.noise.toFixed(1));"
        "if(d.pm25!==undefined) u('v_p2',d.pm25);"
        "if(d.pm10!==undefined) u('v_p1',d.pm10);"
        "if(d.co2!==undefined) u('v_co',d.co2);"
        "if(d.lux!==undefined) u('v_lx',d.lux);"
        "if(d.rain!==undefined) u('v_rn',d.rain.toFixed(1));"
        "if(d.compass!==undefined) u('v_cp',d.compass.toFixed(1));"
        "if(d.solar!==undefined) u('v_sl',d.solar);"
        "document.getElementById('updInfo').textContent="
          "new Date().toLocaleTimeString();"
      "}).catch(function(){"
        "document.getElementById('updInfo').textContent='\\u26A0 offline';"
      "});"
    "}"
    "function tick(){upd();updRaw();}"
    "tick();setInterval(tick,5000);"
  "</script>");

  html += htmlFoot();
  gServer->send(200, "text/html; charset=UTF-8", html);
}

// ─── POST /detect ─────────────────────────────────────────────────────────────
static void handleDetect() {
  detectSensors(*gMb, *gData, *gCfg);
  bool cz = isLangCz();
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
  html += F("</h1><form method='POST' action='/config'>");
  html += F("<input type='hidden' name='lang' value='");
  html += cz ? "cs" : "en";
  html += F("'>");

  // Komunikace
  html += F("<h2>");
  html += s.sectionComm;
  html += F("</h2><div class='card'><div class='frow'>");

  html += F("<div class='fg'><label>");
  html += s.cfgAddr;
  html += F("</label><input type='number' name='addr' min='1' max='247' value='");
  html += gCfg->modbusAddr;
  html += F("'><div class='ht'>");
  html += s.cfgAddrHelp;
  html += F("</div></div>");

  html += F("<div class='fg'><label>");
  html += s.cfgBaud;
  html += F("</label><select name='baud'>");
  const uint32_t bauds[] = {1200,2400,4800,9600,19200,38400,57600,115200};
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
  html += s.cfgPoll;
  html += F("</label><input type='number' name='poll' min='1' max='60' value='");
  html += gCfg->pollInterval;
  html += F("'><div class='ht'>");
  html += s.cfgPollHelp;
  html += F("</div></div></div></div>");

  // Kalibrace
  html += F("<h2>");
  html += s.sectionCalib;
  html += F("</h2><div class='card'><div class='frow'>");

  html += F("<div class='fg'><label>");
  html += s.cfgWindDirOffset;
  html += F("</label><select name='wdOffset'>"
            "<option value='0'");
  if (gCfg->windDirOffset == 0) html += F(" selected");
  html += F(">");
  html += s.cfgWindDirOff0;
  html += F("</option><option value='1'");
  if (gCfg->windDirOffset == 1) html += F(" selected");
  html += F(">");
  html += s.cfgWindDirOff1;
  html += F("</option></select><div class='ht'>");
  html += s.cfgWindDirOffsetHelp;
  html += F("</div></div>");

  char rainHex[8];
  snprintf(rainHex, sizeof(rainHex), "0x%02X", gCfg->rainSensitivity);
  html += F("<div class='fg'><label>");
  html += s.cfgRainSens;
  html += F("</label><input type='text' name='rainSens' value='");
  html += rainHex;
  html += F("' maxlength='4'><div class='ht'>");
  html += s.cfgRainSensHelp;
  html += F("</div></div>");

  html += F("<div class='fg'><label>");
  html += s.cfgSlot507;
  html += F("</label><select name='slot507'>"
            "<option value='0'");
  if (gCfg->slot507Mode==0) html += F(" selected");
  html += F(">");
  html += s.cfgSlotAuto;
  html += F("</option><option value='1'");
  if (gCfg->slot507Mode==1) html += F(" selected");
  html += F(">");
  html += s.cfgSlotPM;
  html += F("</option><option value='2'");
  if (gCfg->slot507Mode==2) html += F(" selected");
  html += F(">");
  html += s.cfgSlotCO2;
  html += F("</option></select><div class='ht'>");
  html += s.cfgSlot507Help;
  html += F("</div></div></div></div>");

  html += F("<div style='margin-top:8px'>"
            "<button type='submit' class='btn btn-p'>");
  html += s.btnSave;
  html += F("</button></div></form>");

  // Akce
  html += F("<h2>");
  html += s.sectionActions;
  html += F("</h2><div class='card'><div class='act-row'>");

  html += F("<div class='act-item'>"
            "<form method='POST' action='/action'>"
            "<input type='hidden' name='act' value='wind_zero'>"
            "<input type='hidden' name='lang' value='");
  html += cz ? "cs" : "en";
  html += F("'><button type='submit' class='btn btn-w'>");
  html += s.btnWindZero;
  html += F("</button></form><div class='ht' style='margin-top:6px'>");
  html += s.btnWindZeroHelp;
  html += F("</div></div>");

  html += F("<div class='act-item'>"
            "<form method='POST' action='/action'>"
            "<input type='hidden' name='act' value='rain_zero'>"
            "<input type='hidden' name='lang' value='");
  html += cz ? "cs" : "en";
  html += F("'><button type='submit' class='btn btn-w'>");
  html += s.btnRainZero;
  html += F("</button></form><div class='ht' style='margin-top:6px'>");
  html += s.btnRainZeroHelp;
  html += F("</div></div></div></div>");

  html += htmlFoot();
  gServer->send(200, "text/html; charset=UTF-8", html);
}

// ─── POST /config ─────────────────────────────────────────────────────────────
static void handleConfigPost() {
  bool cz = gServer->hasArg("lang") ? (gServer->arg("lang") != "en") : true;

  if (gServer->hasArg("addr")) {
    gCfg->modbusAddr = (uint8_t)constrain(gServer->arg("addr").toInt(), 1, 247);
  }
  if (gServer->hasArg("baud")) {
    uint32_t b = (uint32_t)gServer->arg("baud").toInt();
    if (b != gCfg->baudRate) { gCfg->baudRate = b; gMb->setBaudRate(b); }
  }
  if (gServer->hasArg("poll")) {
    gCfg->pollInterval = (uint8_t)constrain(gServer->arg("poll").toInt(), 1, 60);
  }
  if (gServer->hasArg("wdOffset")) {
    uint8_t v = (uint8_t)(gServer->arg("wdOffset").toInt() & 0x01);
    if (v != gCfg->windDirOffset) {
      gCfg->windDirOffset = v;
      gMb->writeRegister(gCfg->modbusAddr, REG_WIND_DIR_OFFSET, v);
    }
  }
  if (gServer->hasArg("rainSens")) {
    uint8_t v = (uint8_t)strtol(gServer->arg("rainSens").c_str(), nullptr, 16);
    if (v != gCfg->rainSensitivity) {
      gCfg->rainSensitivity = v;
      gMb->writeRegister(gCfg->modbusAddr, REG_RAIN_SENS, v);
    }
  }
  if (gServer->hasArg("slot507")) {
    gCfg->slot507Mode = (uint8_t)constrain(gServer->arg("slot507").toInt(), 0, 2);
  }

  gPrefs->begin("weather", false);
  saveConfig(*gPrefs, *gCfg);
  gPrefs->end();
  Serial.println(F("[CFG] Konfigurace ulozena."));

  sendLangCookie(cz);
  gServer->sendHeader("Location", cz ? "/config?lang=cs" : "/config?lang=en");
  gServer->send(303);
}

// ─── POST /action ─────────────────────────────────────────────────────────────
static void handleAction() {
  bool cz = gServer->hasArg("lang") ? (gServer->arg("lang") != "en") : true;
  String act = gServer->arg("act");

  if (act == "wind_zero") {
    ModbusStatus st = gMb->writeRegister(gCfg->modbusAddr, REG_WIND_ZERO, 0xAA);
    Serial.printf("[ACT] Wind zero: %s\n", gMb->statusStr(st));
  } else if (act == "rain_zero") {
    ModbusStatus st = gMb->writeRegister(gCfg->modbusAddr, REG_RAIN_ZERO, 0x5A);
    Serial.printf("[ACT] Rain zero: %s\n", gMb->statusStr(st));
  }

  sendLangCookie(cz);
  gServer->sendHeader("Location", cz ? "/config?lang=cs" : "/config?lang=en");
  gServer->send(303);
}

// ─── GET /api/data ────────────────────────────────────────────────────────────
static void handleApiData() {
  const SensorData& d = *gData;
  String json = "{";
  json += "\"ts\":" + String(d.lastUpdateMs) + ",";
  json += "\"err\":" + String(d.commError ? "true" : "false");

  if (d.present.wind) {
    json += ",\"windSpeed\":" + fmtFloat(d.windSpeed);
    json += ",\"windForce\":" + String(d.windForce);
    json += ",\"windDir8\":"  + String(d.windDir8);
    json += ",\"windDir360\":" + String(d.windDir360);
  }
  if (d.present.tempHum) {
    json += ",\"temp\":" + fmtFloat(d.temperature);
    json += ",\"hum\":"  + fmtFloat(d.humidity);
  }
  if (d.present.pressure) json += ",\"pres\":"   + fmtFloat(d.pressure,2);
  if (d.present.noise)    json += ",\"noise\":"  + fmtFloat(d.noise);
  if (d.present.pm25)     json += ",\"pm25\":"   + String(d.pm25);
  if (d.present.pm10)     json += ",\"pm10\":"   + String(d.pm10);
  if (d.present.co2)      json += ",\"co2\":"    + String(d.co2);
  if (d.present.light)    json += ",\"lux\":"    + String(d.luxFull);
  if (d.present.rainfall) json += ",\"rain\":"   + fmtFloat(d.rainfall);
  if (d.present.compass)  json += ",\"compass\":" + fmtFloat(d.compass);
  if (d.present.solar)    json += ",\"solar\":"  + String(d.solar);
  json += "}";

  gServer->sendHeader("Access-Control-Allow-Origin", "*");
  gServer->send(200, "application/json", json);
}

// ─── GET /api/status ──────────────────────────────────────────────────────────
static void handleApiStatus() {
  const SensorPresence& p = gData->present;
  String json = "{";
  json += "\"wind\":"     + String(p.wind     ?"true":"false");
  json += ",\"tempHum\":" + String(p.tempHum  ?"true":"false");
  json += ",\"pressure\":" + String(p.pressure?"true":"false");
  json += ",\"noise\":"   + String(p.noise    ?"true":"false");
  json += ",\"pm25\":"    + String(p.pm25     ?"true":"false");
  json += ",\"pm10\":"    + String(p.pm10     ?"true":"false");
  json += ",\"co2\":"     + String(p.co2      ?"true":"false");
  json += ",\"light\":"   + String(p.light    ?"true":"false");
  json += ",\"rainfall\":" + String(p.rainfall?"true":"false");
  json += ",\"compass\":" + String(p.compass  ?"true":"false");
  json += ",\"solar\":"   + String(p.solar    ?"true":"false");
  json += "}";

  gServer->sendHeader("Access-Control-Allow-Origin", "*");
  gServer->send(200, "application/json", json);
}

// ─── GET /api/raw – Poslední TX/RX frame ─────────────────────────────────────
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
void webServerBegin(SensorData& data, SensorConfig& cfg,
                    Preferences& prefs, ModbusClient& mb) {
  gData  = &data;
  gCfg   = &cfg;
  gPrefs = &prefs;
  gMb    = &mb;

  gServer = new WebServer(80);
  static const char* hdrs[] = {"Cookie"};
  gServer->collectHeaders(hdrs, 1);

  gServer->on("/",             HTTP_GET,  handleRoot);
  gServer->on("/detect",       HTTP_POST, handleDetect);
  gServer->on("/config",       HTTP_GET,  handleConfig);
  gServer->on("/config",       HTTP_POST, handleConfigPost);
  gServer->on("/action",       HTTP_POST, handleAction);
  gServer->on("/api/data",     HTTP_GET,  handleApiData);
  gServer->on("/api/status",   HTTP_GET,  handleApiStatus);
  gServer->on("/api/raw",      HTTP_GET,  handleApiRaw);
  gServer->onNotFound([]() {
    gServer->send(404, "text/plain", "Not found");
  });

  gServer->begin();
  Serial.println(F("[WEB] HTTP server spusten na portu 80"));
}

void webServerHandle() {
  if (gServer) gServer->handleClient();
}
