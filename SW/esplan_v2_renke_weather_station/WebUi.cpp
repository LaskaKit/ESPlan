#include "WebUi.h"
#include "logo_web.h"

String htmlEscape(const String& in) {
  String out;
  out.reserve(in.length() + 16);
  for (size_t i = 0; i < in.length(); i++) {
    switch (in[i]) {
      case '&': out += F("&amp;"); break;
      case '<': out += F("&lt;"); break;
      case '>': out += F("&gt;"); break;
      case '"': out += F("&quot;"); break;
      case '\'': out += F("&#39;"); break;
      default: out += in[i]; break;
    }
  }
  return out;
}

String fmtFloat(float v, uint8_t decimals) {
  if (isnan(v) || !isfinite(v)) return F("-");
  return String(static_cast<double>(v), static_cast<unsigned int>(decimals));
}

static String jsonFloat(float v, uint8_t decimals = 1) {
  if (isnan(v) || !isfinite(v)) return F("null");
  return String(static_cast<double>(v), static_cast<unsigned int>(decimals));
}

String jsonString(const String& in) {
  String out;
  out.reserve(in.length() + 8);

  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    switch (c) {
      case '\"': out += F("\\\""); break;
      case '\\': out += F("\\\\"); break;
      case '\n': out += F("\\n"); break;
      case '\r': out += F("\\r"); break;
      case '\t': out += F("\\t"); break;
      case '\b': out += F("\\b"); break;
      case '\f': out += F("\\f"); break;
      default:
        if ((uint8_t)c < 32) {
          out += '?';
        } else {
          out += c;
        }
    }
  }
  return out;
}

static String hex16(uint16_t value) {
  char buf[5];
  snprintf(buf, sizeof(buf), "%04X", value);
  return String(buf);
}

static String statusBadge(bool present, const Texts& t) {
  String s;
  s.reserve(64);
  s += F("<span class='");
  s += present ? F("ok'>") : F("bad'>");
  s += present ? t.present : t.absent;
  s += F("</span>");
  return s;
}

String buildStyle(const Texts&, UiLang) {
  return F(
    "<style>"
    "body{font-family:Arial,sans-serif;background:#f4f7fb;color:#222;margin:0;padding:20px;}"
    ".wrap{max-width:1800px;margin:0 auto;}"
    ".grid{display:grid;grid-template-columns:repeat(4,1fr);gap:16px;align-items:stretch;}"
    ".card{background:#fff;border-radius:14px;padding:16px;box-shadow:0 2px 12px rgba(0,0,0,.08);min-width:0;height:100%;box-sizing:border-box;display:flex;flex-direction:column;}"
    ".card-wide{grid-column:1 / -1;}"
    ".top{display:flex;justify-content:space-between;align-items:center;gap:12px;flex-wrap:wrap;margin-bottom:20px;}"
    ".title{display:flex;align-items:center;gap:18px;flex-wrap:wrap}"
    ".brand-logo{display:block;max-width:180px;width:100%;height:auto}"
    ".nav{display:flex;gap:10px;flex-wrap:wrap;margin:10px 0 18px 0}"
    ".nav a{display:inline-block;padding:10px 14px;border-radius:10px;background:#e8eef8;color:#1f6feb;text-decoration:none;font-weight:bold}"
    ".nav a.active{background:#1f6feb;color:#fff}"
    "table{width:100%;border-collapse:collapse;table-layout:fixed}"
    "th,td{padding:8px;border-bottom:1px solid #ddd;text-align:left;font-size:14px;vertical-align:top;word-break:break-word}"
    ".ok{color:#0a7a2f;font-weight:bold}.bad{color:#b00020;font-weight:bold}"
    ".sub{color:#637186;font-size:13px}"
    ".mono{font-family:monospace;white-space:pre-wrap;background:#eef2f7;padding:10px;border-radius:10px;overflow:auto}"
    ".mono-frames{max-height:140px;font-size:12px;line-height:1.3}"
    ".mono-raw{max-height:420px;font-size:12px;line-height:1.3}"
    ".cfg-table td:first-child{width:22%;font-weight:bold}"
    ".cfg-table td:nth-child(2){width:28%}"
    ".cfg-table td:nth-child(3){width:50%}"
    ".cfg-inline-form{margin:0}"
    ".cfg-inline-form input,.cfg-inline-form select,.cfg-inline-form textarea{margin:0}"
    "label{display:block;margin:8px 0 4px;font-weight:bold}"
    "input[type=text],input[type=number],select,textarea{width:100%;padding:10px;border:1px solid #ccc;border-radius:8px;box-sizing:border-box}"
    "textarea{resize:vertical;min-height:88px;font-family:monospace}"
    "button{padding:10px 14px;border:0;border-radius:10px;background:#1f6feb;color:#fff;cursor:pointer;margin-top:8px}"
    "button:hover{opacity:.92}"
    ".row2{display:grid;grid-template-columns:1fr 1fr;gap:12px}"
    ".msg{padding:10px 12px;border-radius:10px;margin-bottom:12px}"
    ".note{font-size:13px;color:#5e6a7a}"
    "@media (max-width:1400px){.grid{grid-template-columns:repeat(2,1fr);}}"
    "@media (max-width:800px){.grid{grid-template-columns:1fr}.row2{grid-template-columns:1fr}.brand-logo{max-width:240px;}}"
    "</style>");
}

static void appendTop(String& html, UiLang lang, const Texts& t, const char* active) {
  html += F("<div class='top'><div class='title'><img src='/logo.png' alt='logo' class='brand-logo'><div><h1>");
  html += t.appTitle;
  html += F("</h1></div></div>");

  html += F("<form method='GET' action='");
  html += (strcmp(active, "main") == 0) ? F("/") : F("/config");
  html += F("' style='display:flex;gap:8px;align-items:center;margin:0;'>");
  html += F("<label style='margin:0;font-weight:bold;'>");
  html += t.language;
  html += F("</label><select name='lang' onchange='this.form.submit()'>");
  html += F("<option value='cz'");
  if (lang == UiLang::CZ) html += F(" selected");
  html += F(">CZ</option><option value='en'");
  if (lang == UiLang::EN) html += F(" selected");
  html += F(">ENG</option></select></form></div>");

  html += F("<div class='nav'>");
  html += F("<a href='/?lang="); html += langCode(lang); html += F("' class='");
  html += (strcmp(active, "main") == 0) ? F("active") : F("");
  html += F("'>"); html += t.tabMain; html += F("</a>");
  html += F("<a href='/config?lang="); html += langCode(lang); html += F("' class='");
  html += (strcmp(active, "config") == 0) ? F("active") : F("");
  html += F("'>"); html += t.tabConfig; html += F("</a>");
  html += F("</div>");
}

static void appendRow(String& html, const String& id, const String& label, const String& value) {
  html += F("<tr><td>");
  html += label;
  html += F("</td><td id='");
  html += id;
  html += F("'>");
  html += value;
  html += F("</td></tr>");
}

static String buildConfigHistoryBox(const Texts& t, const WeatherStation& ws) {
  String html;
  html.reserve(1800);

  html += F("<div class='card card-wide' style='margin-bottom:16px;'><h2>");
  html += t.modbusLog;
  html += F("</h2>");

  if (ws.configLogCount() == 0) {
    html += F("<div class='note'>");
    html += t.noResult;
    html += F("</div></div>");
    return html;
  }

  const CommLog& c = ws.configLogAt(0);

  html += F("<table>");
  appendRow(html, F("cfg-log-state"), t.modbusLastStatus, c.lastOk ? String(F("<span class='ok'>")) + t.statusOkShort + F("</span>") : String(F("<span class='bad'>")) + t.statusErrorShort + F("</span>"));
  appendRow(html, F("cfg-log-action"), t.modbusAction, htmlEscape(c.lastAction));
  appendRow(html, F("cfg-log-duration"), t.modbusDuration, String(c.lastDurationMs) + F(" ms"));
  appendRow(html, F("cfg-log-error"), t.modbusError, c.lastError.length() ? htmlEscape(c.lastError) : F("-"));
  html += F("</table>");

  html += F("<div class='sub' style='margin-top:10px'>");
  html += t.modbusTx;
  html += F("</div><div class='mono'>");
  html += htmlEscape(c.lastRequestHex);
  html += F("</div>");

  html += F("<div class='sub' style='margin-top:10px'>");
  html += t.modbusRx;
  html += F("</div><div class='mono'>");
  html += htmlEscape(c.lastResponseHex);
  html += F("</div>");

  html += F("</div>");
  return html;
}

static void appendMainCards(String& html, const Texts& t, const WeatherStation& ws, bool ethUp, const String& ip, const String& mac) {
  (void)ethUp;
  (void)ip;
  (void)mac;

  const auto& p = ws.presence();
  const auto& v = ws.values();
  const auto& c = ws.commLog();

  html += F("<div class='grid'>");

  html += F("<div class='card'><h2>");
  html += t.sensorStatus;
  html += F("</h2><table>");
  appendRow(html, F("st-wind"), t.sensorWind, statusBadge(p.wind, t));
  appendRow(html, F("st-th"), t.sensorHumidityTemp, statusBadge(p.humidityTemp, t));
  appendRow(html, F("st-noise"), t.sensorNoise, statusBadge(p.noise, t));
  appendRow(html, F("st-air"), t.sensorAir, statusBadge(p.air, t));
  appendRow(html, F("st-pressure"), t.sensorPressure, statusBadge(p.pressure, t));
  appendRow(html, F("st-light"), t.sensorLight, statusBadge(p.light, t));
  appendRow(html, F("st-rain"), t.sensorRain, statusBadge(p.rain, t));
  appendRow(html, F("st-compass"), t.sensorCompass, statusBadge(p.compass, t));
  appendRow(html, F("st-solar"), t.sensorSolar, statusBadge(p.solar, t));
  html += F("</table></div>");

  html += F("<div class='card'><h2>");
  html += t.values;
  html += F("</h2><table>");
  appendRow(html, F("v-wind-short"), t.windSpeed, fmtFloat(v.windSpeed, 1));
  appendRow(html, F("v-beaufort-short"), t.beaufort, (v.windForce >= 0 ? String(v.windForce) : F("-")));
  appendRow(html, F("v-dir8-short"), t.windDir8, (v.windDir8 >= 0 ? String(v.windDir8) : F("-")));
  appendRow(html, F("v-dirdeg-short"), t.windDirDeg, (v.windDirDeg >= 0 ? String(v.windDirDeg) : F("-")));
  appendRow(html, F("v-hum-short"), t.humidity, fmtFloat(v.humidity, 1));
  appendRow(html, F("v-temp-short"), t.temperature, fmtFloat(v.temperature, 1));
  appendRow(html, F("v-noise-short"), t.noise, fmtFloat(v.noise, 1));
  appendRow(html, F("v-air1-short"), ws.airLabel1(), (v.air1 >= 0 ? String(v.air1) : F("-")));
  appendRow(html, F("v-air2-short"), ws.airLabel2(), (v.air2 >= 0 ? String(v.air2) : F("-")));
  appendRow(html, F("v-pressure-short"), t.pressure, fmtFloat(v.pressureKpa, 1));
  appendRow(html, F("v-luxh-short"), t.lightHigh, String(v.lightHigh));
  appendRow(html, F("v-luxl-short"), t.lightLow, String(v.lightLow));
  appendRow(html, F("v-lux-short"), t.lightLux100, String(v.lightLux));
  appendRow(html, F("v-rain-short"), t.rain, fmtFloat(v.rain, 1));
  appendRow(html, F("v-compass-short"), t.compass, fmtFloat(v.compassDeg, 2));
  appendRow(html, F("v-solar-short"), t.solarRadiation, (v.solarRadiation >= 0 ? String(v.solarRadiation) : F("-")));
  appendRow(html, F("data-age"), t.pollAge, String((millis() - v.lastReadMs) / 1000UL) + String(F(" ")) + t.seconds);
  html += F("</table></div>");

  html += F("<div class='card'><h2>");
  html += t.modbusLog;
  html += F("</h2><table>");
  appendRow(html, F("mb-state"), t.modbusLastStatus, c.lastOk ? String(F("<span class='ok'>")) + t.statusOkShort + F("</span>") : String(F("<span class='bad'>")) + t.statusErrorShort + F("</span>"));
  appendRow(html, F("mb-action"), t.modbusAction, htmlEscape(c.lastAction));
  appendRow(html, F("mb-dur"), t.modbusDuration, String(c.lastDurationMs) + F(" ms"));
  appendRow(html, F("mb-stats"), t.modbusCounters, String(c.successCount) + F(" / ") + String(c.errorCount));
  appendRow(html, F("mb-err"), t.modbusError, c.lastError.length() ? htmlEscape(c.lastError) : F("-"));
  html += F("</table></div>");

  html += F("<div class='card'><h2>");
  html += t.modbusFrames;
  html += F("</h2>");
  html += F("<div class='sub'>");
  html += t.modbusTx;
  html += F("</div><div class='mono mono-frames' id='mb-tx'>");
  html += htmlEscape(c.lastRequestHex);
  html += F("</div>");
  html += F("<div class='sub' style='margin-top:10px'>");
  html += t.modbusRx;
  html += F("</div><div class='mono mono-frames' id='mb-rx'>");
  html += htmlEscape(c.lastResponseHex);
  html += F("</div>");

  html += F("<div class='sub' style='margin-top:12px'>");
  html += t.rawRegisters;
  html += F("</div><div class='mono mono-raw' id='raw-regs'>");
  for (uint8_t i = 0; i < REG_DATA_COUNT; i++) {
    html += F("R");
    html += String(REG_DATA_START + i);
    html += F(" = ");
    html += String(v.raw[i]);
    if (i + 1 < REG_DATA_COUNT) html += '\n';
  }
  html += F("</div></div>");

  html += F("</div>");
}

static String buildMainScript(UiLang lang, const Texts& t) {
  String s;
  s.reserve(9000);

  s += F("<script>");
  s += F("const L={present:\""); s += t.present;
  s += F("\",absent:\""); s += t.absent;
  s += F("\",seconds:\""); s += t.seconds;
  s += F("\",ok:\""); s += t.statusOkShort;
  s += F("\",err:\""); s += t.statusErrorShort;
  s += F("\"};");

  s += F("function badge(v){return \"<span class='\"+(v?\"ok\":\"bad\")+\"'>\"+(v?L.present:L.absent)+\"</span>\";}");
  s += F("function setHtml(id,v){const e=document.getElementById(id);if(e)e.innerHTML=v;}");
  s += F("function setText(id,v){const e=document.getElementById(id);if(e)e.textContent=v;}");
  s += F("function num(v,d){if(v===null||v===undefined||v==='-'||Number.isNaN(Number(v)))return '-';return Number(v).toFixed(d);}");

  s += F("async function tick(){");
  s += F("try{");
  s += F("const url='/data.json?lang="); s += langCode(lang); s += F("&_=' + Date.now();");
  s += F("const r=await fetch(url,{cache:'no-store'});");
  s += F("if(!r.ok)return;");
  s += F("const d=await r.json();");

  s += F("setHtml('st-wind',badge(d.pwind));");
  s += F("setHtml('st-th',badge(d.pth));");
  s += F("setHtml('st-noise',badge(d.pnoise));");
  s += F("setHtml('st-air',badge(d.pair));");
  s += F("setHtml('st-pressure',badge(d.ppress));");
  s += F("setHtml('st-light',badge(d.plight));");
  s += F("setHtml('st-rain',badge(d.prain));");
  s += F("setHtml('st-compass',badge(d.pcomp));");
  s += F("setHtml('st-solar',badge(d.psolar));");

  s += F("setText('v-wind-short',num(d.wind,1));");
  s += F("setText('v-beaufort-short',(d.beaufort<0)?'-':String(d.beaufort));");
  s += F("setText('v-dir8-short',(d.dir8<0)?'-':String(d.dir8));");
  s += F("setText('v-dirdeg-short',(d.dirdeg<0)?'-':String(d.dirdeg));");
  s += F("setText('v-hum-short',num(d.hum,1));");
  s += F("setText('v-temp-short',num(d.temp,1));");
  s += F("setText('v-noise-short',num(d.noise,1));");
  s += F("setText('v-air1-short',(d.air1<0?'-':String(d.air1)));");
  s += F("setText('v-air2-short',(d.air2<0?'-':String(d.air2)));");
  s += F("setText('v-pressure-short',num(d.press,1));");
  s += F("setText('v-luxh-short',String(d.lightHigh));");
  s += F("setText('v-luxl-short',String(d.lightLow));");
  s += F("setText('v-lux-short',String(d.light));");
  s += F("setText('v-rain-short',num(d.rain,1));");
  s += F("setText('v-compass-short',num(d.compass,2));");
  s += F("setText('v-solar-short',(d.solar<0)?'-':String(d.solar));");
  s += F("setText('data-age',String(d.age)+' '+L.seconds);");

  s += F("setHtml('mb-state',d.mbOk?\"<span class='ok'>\"+L.ok+\"</span>\":\"<span class='bad'>\"+L.err+\"</span>\");");
  s += F("setText('mb-action',d.mbAction||'-');");
  s += F("setText('mb-dur',String(d.mbDuration)+' ms');");
  s += F("setText('mb-stats',String(d.mbOkCount)+' / '+String(d.mbErrCount));");
  s += F("setText('mb-err',d.mbError||'-');");
  s += F("setText('mb-tx',d.mbTx||'');");
  s += F("setText('mb-rx',d.mbRx||'');");
  s += F("if(Array.isArray(d.raw)){setText('raw-regs',d.raw.map((v,i)=>'R'+(500+i)+' = '+v).join('\\n'));}");

  s += F("}catch(e){}");
  s += F("}");
  s += F("window.addEventListener('load',function(){tick();setInterval(tick,2000);});");
  s += F("</script>");
  return s;
}

static String buildManualCommandScript() {
  String s;
  s.reserve(2400);
  s += F("<script>");
  s += F("function wsCrc16(bytes){let crc=0xFFFF;for(let i=0;i<bytes.length;i++){crc^=bytes[i];for(let j=0;j<8;j++){if(crc&1){crc=(crc>>1)^0xA001;}else{crc>>=1;}}}return crc&0xFFFF;}");
  s += F("function wsHexByte(v){return Number(v).toString(16).toUpperCase().padStart(2,'0');}");
  s += F("function wsHexWordFromInput(id,defv){let s=(document.getElementById(id).value||defv).trim();s=s.replace(/^0x/i,'');let v=parseInt(s,16);if(Number.isNaN(v))v=parseInt(defv,16);if(Number.isNaN(v))v=0;return Math.max(0,Math.min(65535,v));}");
  s += F("function wsUpdateManualPreview(){");
  s += F("const a=Math.max(1,Math.min(247,parseInt(document.getElementById('manual-addr').value||'1',10)));");
  s += F("const r=wsHexWordFromInput('manual-reg','6000');");
  s += F("const v=wsHexWordFromInput('manual-val','0000');");
  s += F("const frame=[a,0x06,(r>>8)&0xFF,r&0xFF,(v>>8)&0xFF,v&0xFF];");
  s += F("const crc=wsCrc16(frame);frame.push(crc&0xFF);frame.push((crc>>8)&0xFF);");
  s += F("const el=document.getElementById('manual-preview');if(el){el.textContent=frame.map(wsHexByte).join(' ');}}");
  s += F("window.addEventListener('load',function(){['manual-addr','manual-reg','manual-val'].forEach(function(id){const e=document.getElementById(id);if(e){e.addEventListener('input',wsUpdateManualPreview);e.addEventListener('change',wsUpdateManualPreview);}});wsUpdateManualPreview();});");
  s += F("</script>");
  return s;
}

String buildMainPage(UiLang lang, const WeatherStation& ws, bool ethUp, const String& ip, const String& mac) {
  const auto& t = T(lang);
  String html;
  html.reserve(24000);
  html += F("<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>");
  html += t.appTitle;
  html += F("</title>");
  html += buildStyle(t, lang);
  html += F("</head><body><div class='wrap'>");
  appendTop(html, lang, t, "main");
  appendMainCards(html, t, ws, ethUp, ip, mac);
  html += buildMainScript(lang, t);
  html += F("</div></body></html>");
  return html;
}

String buildConfigPage(UiLang lang, const WeatherStation& ws, const String& flashMsg) {
  const auto& t = T(lang);
  String html;
  html.reserve(24000);

  (void)flashMsg;

  html += F("<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>");
  html += t.tabConfig;
  html += F("</title>");
  html += buildStyle(t, lang);
  html += F("</head><body><div class='wrap'>");
  appendTop(html, lang, t, "config");

  html += F("<div class='grid'>");
  html += buildConfigHistoryBox(t, ws);

  html += F("<div class='card card-wide'><h2>");
  html += t.manualCommand;
  html += F("</h2><div class='note'>");
  html += t.manualNote;
  html += F("</div>");

  html += F("<form method='POST' action='/cmd-manual?lang=");
  html += langCode(lang);
  html += F("'>");

  html += F("<div class='row2'>");

  html += F("<div><label>");
  html += t.manualAddress;
  html += F("</label><input type='number' id='manual-addr' name='address' min='1' max='247' value='");
  html += String(ws.config().address);
  html += F("'></div>");

  html += F("<div><label>");
  html += t.manualRegisterHex;
  html += F("</label><input type='text' id='manual-reg' name='reg' value='6000' placeholder='6000'></div>");

  html += F("<div><label>");
  html += t.manualValueHex;
  html += F("</label><input type='text' id='manual-val' name='value' value='0000' placeholder='0000'></div>");

  html += F("<div><label>");
  html += t.manualPreview;
  html += F("</label><div class='mono' id='manual-preview'>-</div></div>");

  html += F("</div><button type='submit'>");
  html += t.manualSend;
  html += F("</button></form>");

  html += F("<hr style='margin:16px 0;border:none;border-top:1px solid #ddd'>");

  html += F("<h2>");
  html += t.rawHexTitle;
  html += F("</h2><div class='note'>");
  html += t.rawHexNote;
  html += F("</div>");

  html += F("<form method='POST' action='/cmd-rawhex?lang=");
  html += langCode(lang);
  html += F("'>");
  html += F("<label>");
  html += t.rawHexInput;
  html += F("</label><textarea name='rawhex' placeholder='010660000001560A'></textarea>");
  html += F("<button type='submit'>");
  html += t.rawHexSend;
  html += F("</button></form></div>");

  html += F("<div class='card card-wide'><h2>");
  html += t.configTitle;
  html += F("</h2>");

  html += F("<table class='cfg-table'>");
  html += F("<tr><th>");
  html += t.tableItem;
  html += F("</th><th>");
  html += t.tableSetting;
  html += F("</th><th>");
  html += t.tableNote;
  html += F("</th></tr>");

  html += F("<tr><td>");
  html += t.address;
  html += F("</td><td>");
  html += F("<form class='cfg-inline-form' method='POST' action='/config-save?lang=");
  html += langCode(lang);
  html += F("'>");
  html += F("<input type='hidden' name='baudrate' value='");
  html += String(ws.config().baudrate);
  html += F("'>");
  html += F("<input type='hidden' name='windOffset' value='");
  html += hex16(ws.config().windOffset);
  html += F("'>");
  html += F("<input type='hidden' name='rainSensitivity' value='");
  html += hex16(ws.config().rainSensitivity);
  html += F("'>");
  html += F("<input type='number' name='address' min='1' max='247' value='");
  html += String(ws.config().address);
  html += F("'> <button type='submit'>");
  html += t.save;
  html += F("</button></form></td><td>");
  html += t.noteAddress;
  html += F("</td></tr>");

  html += F("<tr><td>");
  html += t.baudrate;
  html += F("</td><td>");
  html += F("<form class='cfg-inline-form' method='POST' action='/config-save?lang=");
  html += langCode(lang);
  html += F("'>");
  html += F("<input type='hidden' name='address' value='");
  html += String(ws.config().address);
  html += F("'>");
  html += F("<input type='hidden' name='windOffset' value='");
  html += hex16(ws.config().windOffset);
  html += F("'>");
  html += F("<input type='hidden' name='rainSensitivity' value='");
  html += hex16(ws.config().rainSensitivity);
  html += F("'>");
  html += F("<select name='baudrate'>");
  const uint32_t baudrates[] = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
  for (uint8_t i = 0; i < sizeof(baudrates) / sizeof(baudrates[0]); i++) {
    html += F("<option value='");
    html += String(baudrates[i]);
    html += F("'");
    if (baudrates[i] == ws.config().baudrate) html += F(" selected");
    html += F(">");
    html += String(baudrates[i]);
    html += F("</option>");
  }
  html += F("</select> <button type='submit'>");
  html += t.save;
  html += F("</button></form></td><td>");
  html += t.noteBaudrate;
  html += F("</td></tr>");

  html += F("<tr><td>");
  html += t.windOffset;
  html += F("</td><td>");
  html += F("<form class='cfg-inline-form' method='POST' action='/config-save?lang=");
  html += langCode(lang);
  html += F("'>");
  html += F("<input type='hidden' name='address' value='");
  html += String(ws.config().address);
  html += F("'>");
  html += F("<input type='hidden' name='baudrate' value='");
  html += String(ws.config().baudrate);
  html += F("'>");
  html += F("<input type='hidden' name='rainSensitivity' value='");
  html += hex16(ws.config().rainSensitivity);
  html += F("'>");
  html += F("<input type='text' name='windOffset' value='");
  html += hex16(ws.config().windOffset);
  html += F("' placeholder='0000'> <button type='submit'>");
  html += t.save;
  html += F("</button></form></td><td>");
  html += t.noteWindOffset;
  html += F("</td></tr>");

  html += F("<tr><td>");
  html += t.rainSensitivity;
  html += F("</td><td>");
  html += F("<form class='cfg-inline-form' method='POST' action='/config-save?lang=");
  html += langCode(lang);
  html += F("'>");
  html += F("<input type='hidden' name='address' value='");
  html += String(ws.config().address);
  html += F("'>");
  html += F("<input type='hidden' name='baudrate' value='");
  html += String(ws.config().baudrate);
  html += F("'>");
  html += F("<input type='hidden' name='windOffset' value='");
  html += hex16(ws.config().windOffset);
  html += F("'>");
  html += F("<input type='text' name='rainSensitivity' value='");
  html += hex16(ws.config().rainSensitivity);
  html += F("' placeholder='0011'> <button type='submit'>");
  html += t.save;
  html += F("</button></form></td><td>");
  html += t.noteRainSensitivity;
  html += F("</td></tr>");

  html += F("<tr><td>");
  html += t.commandWindZero;
  html += F("</td><td><form class='cfg-inline-form' method='POST' action='/cmd-wind-zero?lang=");
  html += langCode(lang);
  html += F("'><button type='submit'>");
  html += t.execute;
  html += F("</button></form></td><td>");
  html += t.noteWindZero;
  html += F("</td></tr>");

  html += F("<tr><td>");
  html += t.commandRainZero;
  html += F("</td><td><form class='cfg-inline-form' method='POST' action='/cmd-rain-zero?lang=");
  html += langCode(lang);
  html += F("'><button type='submit'>");
  html += t.execute;
  html += F("</button></form></td><td>");
  html += t.noteRainZero;
  html += F("</td></tr>");

  html += F("</table></div>");

  html += F("</div>");
  html += buildManualCommandScript();
  html += F("</div></body></html>");
  return html;
}

String buildMainDataJson(const WeatherStation& ws, bool ethUp, const String& ip, const String& mac) {
  const auto& v = ws.values();
  const auto& p = ws.presence();
  const auto& c = ws.commLog();

  String json;
  json.reserve(2400);

  json += F("{");
  json += F("\"eth\":"); json += ethUp ? F("true") : F("false");
  json += F(",\"ip\":\""); json += jsonString(ip); json += F("\"");
  json += F(",\"mac\":\""); json += jsonString(mac); json += F("\"");
  json += F(",\"age\":"); json += String((millis() - v.lastReadMs) / 1000UL);

  json += F(",\"wind\":"); json += jsonFloat(v.windSpeed);
  json += F(",\"beaufort\":"); json += (v.windForce >= 0 ? String(v.windForce) : F("-1"));
  json += F(",\"dir8\":"); json += (v.windDir8 >= 0 ? String(v.windDir8) : F("-1"));
  json += F(",\"dirdeg\":"); json += (v.windDirDeg >= 0 ? String(v.windDirDeg) : F("-1"));
  json += F(",\"temp\":"); json += jsonFloat(v.temperature);
  json += F(",\"hum\":"); json += jsonFloat(v.humidity);
  json += F(",\"noise\":"); json += jsonFloat(v.noise);
  json += F(",\"air1\":"); json += String(v.air1);
  json += F(",\"air2\":"); json += String(v.air2);
  json += F(",\"press\":"); json += jsonFloat(v.pressureKpa);
  json += F(",\"rain\":"); json += jsonFloat(v.rain);
  json += F(",\"solar\":"); json += String(v.solarRadiation);
  json += F(",\"light\":"); json += String(v.lightLux);
  json += F(",\"lightHigh\":"); json += String(v.lightHigh);
  json += F(",\"lightLow\":"); json += String(v.lightLow);
  json += F(",\"compass\":"); json += jsonFloat(v.compassDeg, 2);

  json += F(",\"pwind\":"); json += p.wind ? F("true") : F("false");
  json += F(",\"pth\":"); json += p.humidityTemp ? F("true") : F("false");
  json += F(",\"pnoise\":"); json += p.noise ? F("true") : F("false");
  json += F(",\"pair\":"); json += p.air ? F("true") : F("false");
  json += F(",\"ppress\":"); json += p.pressure ? F("true") : F("false");
  json += F(",\"plight\":"); json += p.light ? F("true") : F("false");
  json += F(",\"prain\":"); json += p.rain ? F("true") : F("false");
  json += F(",\"pcomp\":"); json += p.compass ? F("true") : F("false");
  json += F(",\"psolar\":"); json += p.solar ? F("true") : F("false");

  json += F(",\"mbOk\":"); json += c.lastOk ? F("true") : F("false");
  json += F(",\"mbDuration\":"); json += String(c.lastDurationMs);
  json += F(",\"mbOkCount\":"); json += String(c.successCount);
  json += F(",\"mbErrCount\":"); json += String(c.errorCount);
  json += F(",\"mbAction\":\""); json += jsonString(c.lastAction); json += F("\"");
  json += F(",\"mbError\":\""); json += jsonString(c.lastError); json += F("\"");
  json += F(",\"mbTx\":\""); json += jsonString(c.lastRequestHex); json += F("\"");
  json += F(",\"mbRx\":\""); json += jsonString(c.lastResponseHex); json += F("\"");

  json += F(",\"raw\":[");
  for (uint8_t i = 0; i < REG_DATA_COUNT; i++) {
    if (i) json += ',';
    json += String(v.raw[i]);
  }
  json += F("]}");

  return json;
}