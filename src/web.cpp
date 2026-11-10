#include "web.h"
#include <WebServer.h>
#include "modes.h"
#include "settings.h"

static WebServer server(80);
static void (*modeChanged)() = nullptr;

static String esc(const char *s) {
  String o;
  for (; *s; s++) {
    switch (*s) {
      case '&': o += F("&amp;"); break;
      case '<': o += F("&lt;"); break;
      case '>': o += F("&gt;"); break;
      case '"': o += F("&quot;"); break;
      case '\'': o += F("&#39;"); break;
      default: o += *s;
    }
  }
  return o;
}

static String hhmm(uint16_t m) {
  char b[6];
  snprintf(b, sizeof(b), "%02u:%02u", m / 60, m % 60);
  return b;
}

static uint16_t parseHhmm(const String &s, uint16_t fallback) {
  int h, m;
  if (sscanf(s.c_str(), "%d:%d", &h, &m) != 2) return fallback;
  if (h < 0 || h > 23 || m < 0 || m > 59) return fallback;
  return h * 60 + m;
}

static const char STYLE[] PROGMEM = R"CSS(
body{font-family:system-ui,sans-serif;background:#111;color:#eee;margin:0;padding:16px;max-width:520px}
h1{color:#ff8c00;font-size:1.4em}h2{font-size:1.05em;margin-top:1.6em;color:#ffb347}
.modes{display:grid;gap:8px}
.modes button{padding:14px;font-size:1em;border-radius:8px;border:1px solid #444;background:#222;color:#eee;text-align:left}
.modes button.on{background:#ff8c00;color:#111;border-color:#ff8c00;font-weight:600}
.modes button.secret{opacity:.35;font-size:.8em;padding:8px}
label{display:block;margin:10px 0 4px}
input[type=text],input[type=password],select,input[type=time]{width:100%;box-sizing:border-box;padding:9px;background:#222;color:#eee;border:1px solid #444;border-radius:6px;font-size:1em}
input[type=range]{width:100%}
.row{display:flex;gap:10px}.row>div{flex:1}
.save{margin-top:20px;padding:14px;width:100%;font-size:1.05em;background:#ff8c00;color:#111;border:0;border-radius:8px;font-weight:600}
.ok{background:#264d26;padding:8px 12px;border-radius:6px}.hint{color:#999;font-size:.85em}
)CSS";

static void handleRoot() {
  const Settings &s = settings;
  String h;
  h.reserve(7000);
  h += F("<!doctype html><html><head><meta charset=utf-8>"
         "<meta name=viewport content='width=device-width,initial-scale=1'>"
         "<title>Departure board</title><style>");
  h += FPSTR(STYLE);
  h += F("</style></head><body><h1>Departure board</h1>");
  if (server.hasArg("saved")) h += F("<p class=ok>Saved.</p>");

  h += F("<h2>Mode</h2><form method=post action=/mode class=modes>");
  for (int i = 0; i < MODE_COUNT; i++) {
    h += F("<button name=m value=");
    h += i;
    h += F(" class='");
    if (i == s.mode) h += F("on ");
    if (i == MODE_BUMS) h += F("secret");
    h += F("'>");
    h += MODES[i].button;
    h += F("</button>");
  }
  h += F("</form>");

  char col[8];
  snprintf(col, sizeof(col), "#%06lX", (unsigned long)s.color);

  h += F("<form method=post action=/save><h2>Display</h2>"
         "<label>Brightness: <output id=bo>");
  h += s.brightness;
  h += F("</output>%</label><input type=range name=bright min=5 max=100 value=");
  h += s.brightness;
  h += F(" oninput='bo.value=this.value'>"
         "<label>Colour</label><input type=color name=color value='");
  h += col;
  h += F("'><label><input type=checkbox name=showMsg");
  if (s.showMessage) h += F(" checked");
  h += F("> Show scrolling message</label>"
         "<input type=text name=msg maxlength=100 value='");
  h += esc(s.message);
  h += F("'>");

  h += F("<h2>Night schedule</h2><label><input type=checkbox name=night");
  if (s.nightEnabled) h += F(" checked");
  h += F("> Enabled</label><div class=row><div><label>From</label>"
         "<input type=time name=nStart value=");
  h += hhmm(s.nightStart);
  h += F("></div><div><label>Until</label><input type=time name=nEnd value=");
  h += hhmm(s.nightEnd);
  h += F("></div></div><label>At night</label><select name=nOff>"
         "<option value=0");
  if (!s.nightOff) h += F(" selected");
  h += F(">Dim</option><option value=1");
  if (s.nightOff) h += F(" selected");
  h += F(">Turn off</option></select>"
         "<label>Night brightness: <output id=no>");
  h += s.nightBrightness;
  h += F("</output>%</label><input type=range name=nBright min=5 max=100 value=");
  h += s.nightBrightness;
  h += F(" oninput='no.value=this.value'>");

  h += F("<h2>Bus stops (TfL)</h2>"
         "<label>Conyers Road northbound stop ID</label>"
         "<input type=text name=stopN maxlength=15 value='");
  h += esc(s.busStopNorth);
  h += F("'><label>Conyers Road southbound stop ID</label>"
         "<input type=text name=stopS maxlength=15 value='");
  h += esc(s.busStopSouth);
  h += F("'><label>TfL app key</label><input type=password name=tflKey "
         "autocomplete=off placeholder='");
  h += s.tflKey[0] ? F("Saved - leave blank to keep") : F("Not set");
  h += F("'><p class=hint>The key is stored on the board only.</p>");

  h += F("<h2>Trains (proxy)</h2><label>Proxy URL</label>"
         "<input type=text name=proxyUrl maxlength=95 value='");
  h += esc(s.proxyUrl);
  h += F("'><label>Proxy token</label><input type=password name=proxyTok "
         "autocomplete=off placeholder='");
  h += s.proxyToken[0] ? F("Saved - leave blank to keep") : F("Not set");
  h += F("'><p class=hint>PROXY_TOKEN from the Railway service's "
         "Variables tab.</p>"
         "<button class=save>Save settings</button></form></body></html>");

  server.send(200, "text/html; charset=utf-8", h);
}

static void redirectHome() {
  server.sendHeader("Location", "/?saved=1");
  server.send(303);
}

static void handleMode() {
  const int m = server.arg("m").toInt();
  if (server.hasArg("m") && m >= 0 && m < MODE_COUNT) {
    settings.mode = m;
    settingsSave();
    if (modeChanged) modeChanged();
  }
  redirectHome();
}

static void handleSave() {
  Settings &s = settings;
  s.brightness = constrain(server.arg("bright").toInt(), 5, 100);
  const String c = server.arg("color");
  if (c.length() == 7 && c[0] == '#') s.color = strtoul(c.c_str() + 1, nullptr, 16);
  s.showMessage = server.hasArg("showMsg");
  server.arg("msg").toCharArray(s.message, sizeof(s.message));
  s.nightEnabled = server.hasArg("night");
  s.nightStart = parseHhmm(server.arg("nStart"), s.nightStart);
  s.nightEnd = parseHhmm(server.arg("nEnd"), s.nightEnd);
  s.nightOff = server.arg("nOff") == "1";
  s.nightBrightness = constrain(server.arg("nBright").toInt(), 5, 100);
  server.arg("stopN").toCharArray(s.busStopNorth, sizeof(s.busStopNorth));
  server.arg("stopS").toCharArray(s.busStopSouth, sizeof(s.busStopSouth));
  const String key = server.arg("tflKey");
  if (key.length()) key.toCharArray(s.tflKey, sizeof(s.tflKey));
  String proxyUrl = server.arg("proxyUrl");
  proxyUrl.trim();
  proxyUrl.toCharArray(s.proxyUrl, sizeof(s.proxyUrl));
  String tok = server.arg("proxyTok");
  tok.trim();
  if (tok.length()) tok.toCharArray(s.proxyToken, sizeof(s.proxyToken));
  settingsSave();
  redirectHome();
}

void webBegin(void (*onModeChange)()) {
  modeChanged = onModeChange;
  server.on("/", HTTP_GET, handleRoot);
  server.on("/mode", HTTP_POST, handleMode);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound([] { server.send(404, "text/plain", "Not found"); });
  server.begin();
}

void webLoop() { server.handleClient(); }
