#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

// === Pin Definitions ===
#define red_light R0_2
#define green_light R0_3
#define switch_reverse I0_5
#define switch_forward I0_6
#define e_stop I0_7
#define reflex_start I0_8
#define reflex_end I0_9
#define mode_switch I0_4 // Switch for local/remote modes

// === Control Logic Functions ===
void conveyor_fwd() {
  digitalWrite(R0_0, 0);
  digitalWrite(R0_1, 1);
  digitalWrite(green_light, 1);
}

void conveyor_rev() {
  digitalWrite(R0_0, 1);
  digitalWrite(R0_1, 0);
  digitalWrite(green_light, 1);
}

void conveyor_stop() {
  digitalWrite(R0_0, 0);
  digitalWrite(R0_1, 0);
  digitalWrite(green_light, 0);
}

void switch_forward_func() {
  if (digitalRead(reflex_start) == 1 && digitalRead(reflex_end) == 0) {
    conveyor_fwd();
  } else if (digitalRead(reflex_end) == 1) {
    conveyor_stop();
  } else {
    conveyor_fwd();
  }
}

void switch_reverse_func() {
  if (digitalRead(reflex_end) == 1 && digitalRead(reflex_start) == 0) {
    conveyor_rev();
  } else if (digitalRead(reflex_start) == 1) {
    conveyor_stop();
  } else {
    conveyor_rev();
  }
}

unsigned long previousMillis = 0;
long interval = 100;
void redblink() {
  if (millis() - previousMillis >= interval) {
    previousMillis = millis();
    digitalWrite(red_light, !digitalRead(red_light));
  }
}

// === Wi-Fi Access Point & Credentials ===
const char* apSSID = "ESP32_Config";
const char* apPassword = "1234";

String ssid, passwordStr;
bool wifiConnected = false;
IPAddress staIP;

// === Async Web Server & WebSocket ===
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// === Edge-detection State Trackers ===
bool prev_switch_fwd, prev_switch_rev, prev_estop, prev_start, prev_end;
bool prev_red, prev_green, prev_dir_fwd, prev_dir_rev, prev_mode;
bool objectDetectMode = false;
bool objectDetectFwd = true;
bool alternateMode = false;
bool objectMoving = false;
bool targetEnd = false; // false: toward reflex_start, true: toward reflex_end

// === Helper: Broadcast State via WebSocket ===
void sendUpdate(const String& id, bool state) {
  ws.textAll(id + ":" + (state ? "1" : "0"));
}

// === Edge Detection Functions ===
void checkInput(const String& id, int pin, bool& prev) {
  bool curr = digitalRead(pin);
  if (curr != prev) {
    prev = curr;
    sendUpdate(id, curr);
  }
}

void checkOutput(const String& id, int pin, bool& prev) {
  bool curr = digitalRead(pin);
  if (curr != prev) {
    prev = curr;
    sendUpdate(id, curr);
  }
}

// === WebSocket Handler ===
void onWsEvent(AsyncWebSocket* s, AsyncWebSocketClient* client,
               AwsEventType type, void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    // Send full initial state
    sendUpdate("switch_forward", digitalRead(switch_forward));
    sendUpdate("switch_reverse", digitalRead(switch_reverse));
    sendUpdate("e_stop", digitalRead(e_stop));
    sendUpdate("reflex_start", digitalRead(reflex_start));
    sendUpdate("reflex_end", digitalRead(reflex_end));
    sendUpdate("red_light", digitalRead(red_light));
    sendUpdate("green_light", digitalRead(green_light));
    sendUpdate("mode_switch", digitalRead(mode_switch));
    sendUpdate("objectDetectMode", objectDetectMode);
    sendUpdate("objectDetectFwd", objectDetectFwd);
    sendUpdate("alternateMode", alternateMode);
    bool fwd = digitalRead(R0_1) == 1 && digitalRead(R0_0) == 0;
    bool rev = digitalRead(R0_0) == 1 && digitalRead(R0_1) == 0;
    sendUpdate("dirFwd", fwd);
    sendUpdate("dirRev", rev);
  } else if (type == WS_EVT_DATA) {
    String msg = "";
    for (size_t i = 0; i < len; i++) {
      msg += (char)data[i];
    }
    if (digitalRead(mode_switch) == 1) { // Remote mode
      if (msg == "toggleObjectDetect") {
        objectDetectMode = !objectDetectMode;
        sendUpdate("objectDetectMode", objectDetectMode);
        if (!objectDetectMode && !alternateMode) {
          conveyor_stop();
          objectMoving = false;
        }
      } else if (msg == "toggleDetectFwd") {
        objectDetectFwd = !objectDetectFwd;
        sendUpdate("objectDetectFwd", objectDetectFwd);
      } else if (msg == "toggleAlternate") {
        alternateMode = !alternateMode;
        sendUpdate("alternateMode", alternateMode);
        if (!alternateMode && !objectDetectMode) {
          conveyor_stop();
          objectMoving = false;
        }
      } else if (msg == "fwd") {
        conveyor_fwd();
        objectDetectMode = false;
        alternateMode = false;
        sendUpdate("objectDetectMode", objectDetectMode);
        sendUpdate("alternateMode", alternateMode);
      } else if (msg == "rev") {
        conveyor_rev();
        objectDetectMode = false;
        alternateMode = false;
        sendUpdate("objectDetectMode", objectDetectMode);
        sendUpdate("alternateMode", alternateMode);
      } else if (msg == "stop") {
        conveyor_stop();
        objectDetectMode = false;
        alternateMode = false;
        sendUpdate("objectDetectMode", objectDetectMode);
        sendUpdate("alternateMode", alternateMode);
        objectMoving = false;
      }
    }
  }
}

// === Attempt Wi-Fi STA Connection ===
void connectToWiFi() {
  WiFi.begin(ssid.c_str(), passwordStr.c_str());
  for (int i = 0; i < 20; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      staIP = WiFi.localIP();
      break;
    }
    delay(500);
  }
}

// === HTML Templates ===
const char* setupPage = R"rawliteral(
<!DOCTYPE html><html><body>
<h3>Wi-Fi Setup</h3>
<form action="/connect" method="POST">
SSID: <input name="ssid"><br>
Password: <input name="pass" type="password"><br>
<input type="submit" value="Connect"></form>
</body></html>
)rawliteral";

String connectedPage() {
  return "<html><body><body><h3>Connected!</h3><p>IP: " + staIP.toString() +
         "</p><p><a href='/control'>Go to dashboard</a></p></body></html>";
}

const char* dashboardPage = R"rawliteral(
<!DOCTYPE html><html><head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  table { border-collapse: collapse; margin: 20px 0; }
  th, td { padding: 8px 12px; border: 1px solid #777; }
  .light, .sensor { width: 16px; height: 16px; border-radius: 50%; display: inline-block; margin-right: 5px; opacity: 0.2; }
  .on { opacity: 1; }
  .red { background: red; }
  .green { background: green; }
  .sensor { background: gray; }
  .sensor.on { background: blue; }
  button { padding: 8px 16px; margin: 5px; }
</style><title>Conveyor Dashboard</title></head><body>
<h2>Conveyor Dashboard</h2>
<table>
<tr><th>Component</th><th>Status</th></tr>
<tr><td>E-Stop</td><td id="e_stop">—</td></tr>
<tr><td>Mode</td><td id="mode_switch">—</td></tr>
<tr><td>Switch Position</td><td id="switch">—</td></tr>
<tr><td>Start Sensor</td><td><span id="reflex_start" class="sensor"></span></td></tr>
<tr><td>End Sensor</td><td><span id="reflex_end" class="sensor"></span></td></tr>
<tr><td>Red Light</td><td><span id="red_light" class="light red"></span></td></tr>
<tr><td>Green Light</td><td><span id="green_light" class="light green"></span></td></tr>
<tr><td>Conveyor Direction</td><td id="direction">—</td></tr>
</table>
<div id="controls" style="display:none;">
<h3>Remote Control</h3>
<button onclick="ws.send('fwd')">Conveyor Forward</button>
<button onclick="ws.send('rev')">Conveyor Reverse</button>
<button onclick="ws.send('stop')">Conveyor Stop</button><br>
<button onclick="ws.send('toggleObjectDetect')">Toggle Object Detect Mode</button>
<span id="objectDetectMode">Off</span><br>
<button onclick="ws.send('toggleDetectFwd')">Toggle Direction</button>
<span id="objectDetectFwd">Forward</span><br>
<button onclick="ws.send('toggleAlternate')">Toggle Alternate Mode</button>
<span id="alternateMode">Off</span>
</div>
<script>
let state_forward = false;
let state_reverse = false;
let state_dirFwd = false;
let state_dirRev = false;
let objectDetectMode = false;
let objectDetectFwd = true;
let alternateMode = false;

var ws = new WebSocket("ws://"+location.host+"/ws");
ws.onmessage = function(e) {
  let [key, val] = e.data.split(":");
  let state = val === "1";

  switch (key) {
    case "e_stop":
      document.getElementById("e_stop").innerText = state ? "Activated" : "Clear";
      document.getElementById("e_stop").style.color = state ? "red" : "green";
      break;
    case "mode_switch":
      document.getElementById("mode_switch").innerText = state ? "Remote" : "Local";
      document.getElementById("controls").style.display = state ? "block" : "none";
      break;
    case "switch_forward":
      state_forward = state;
      break;
    case "switch_reverse":
      state_reverse = state;
      break;
    case "reflex_start":
    case "reflex_end":
      document.getElementById(key).classList.toggle("on", state);
      break;
    case "red_light":
    case "green_light":
      document.getElementById(key).classList.toggle("on", state);
      break;
    case "dirFwd":
      state_dirFwd = state;
      break;
    case "dirRev":
      state_dirRev = state;
      break;
    case "objectDetectMode":
      objectDetectMode = state;
      document.getElementById("objectDetectMode").innerText = state ? "On" : "Off";
      break;
    case "objectDetectFwd":
      objectDetectFwd = state;
      document.getElementById("objectDetectFwd").innerText = state ? "Forward" : "Reverse";
      break;
    case "alternateMode":
      alternateMode = state;
      document.getElementById("alternateMode").innerText = state ? "On" : "Off";
      break;
  }

  let switchEl = document.getElementById("switch");
  if (state_forward) {
    switchEl.innerText = "Forward";
  } else if (state_reverse) {
    switchEl.innerText = "Reverse";
  } else {
    switchEl.innerText = "Off";
  }

  let dir = document.getElementById("direction");
  if (state_dirFwd) {
    dir.innerText = "Forward";
  } else if (state_dirRev) {
    dir.innerText = "Reverse";
  } else {
    dir.innerText = "Stopped";
  }
};
</script>
</body></html>
)rawliteral";

// === Setup ===
void setup() {
  Serial.begin(115200);
  // I/O setup
  pinMode(R0_0, OUTPUT); pinMode(R0_1, OUTPUT);
  pinMode(red_light, OUTPUT); pinMode(green_light, OUTPUT);
  pinMode(switch_reverse, INPUT); pinMode(switch_forward, INPUT);
  pinMode(e_stop, INPUT); pinMode(reflex_start, INPUT); pinMode(reflex_end, INPUT);
  pinMode(mode_switch, INPUT);

  // Initialize prev-states
  prev_switch_fwd = digitalRead(switch_forward);
  prev_switch_rev = digitalRead(switch_reverse);
  prev_estop = digitalRead(e_stop);
  prev_start = digitalRead(reflex_start);
  prev_end = digitalRead(reflex_end);
  prev_red = digitalRead(red_light);
  prev_green = digitalRead(green_light);
  prev_dir_fwd = prev_dir_rev = false;
  prev_mode = digitalRead(mode_switch);

  WiFi.softAP(apSSID, apPassword);
  Serial.println("AP IP: " + WiFi.softAPIP().toString());

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* r) {
    r->send(200, "text/html", wifiConnected ? dashboardPage : setupPage);
  });
  server.on("/connect", HTTP_POST, [](AsyncWebServerRequest* r) {
    ssid = r->arg("ssid");
    passwordStr = r->arg("pass");
    connectToWiFi();
    r->send(200, "text/html", wifiConnected ? connectedPage() : String("<p>Failed!</p>") + setupPage);
  });
  server.on("/control", HTTP_GET, [](AsyncWebServerRequest* r) {
    r->send(200, "text/html", dashboardPage);
  });
  server.begin();
}

// === Main Loop ===
void loop() {
  if (digitalRead(e_stop) == 1) {
    conveyor_stop();
    redblink();
    objectMoving = false;
  } else if (digitalRead(mode_switch) == 0) { // Local mode
    if (digitalRead(switch_forward) == 1) {
      switch_forward_func();
    } else if (digitalRead(switch_reverse) == 1) {
      switch_reverse_func();
    } else {
      conveyor_stop();
    }
  } else { // Remote mode
    if (objectDetectMode) {
      if (!objectMoving) {
        if (objectDetectFwd && digitalRead(reflex_start) == 1) {
          conveyor_fwd();
          objectMoving = true;
        } else if (!objectDetectFwd && digitalRead(reflex_end) == 1) {
          conveyor_rev();
          objectMoving = true;
        }
      }
      if (objectMoving) {
        if (objectDetectFwd && digitalRead(reflex_end) == 1) {
          conveyor_stop();
          objectMoving = false;
        } else if (!objectDetectFwd && digitalRead(reflex_start) == 1) {
          conveyor_stop();
          objectMoving = false;
        }
      }
    } else if (alternateMode) {
      if (!objectMoving) {
        if (digitalRead(reflex_start) == 1) {
          conveyor_fwd(); // Move toward reflex_end
          objectMoving = true;
          targetEnd = true;
        } else if (digitalRead(reflex_end) == 1) {
          conveyor_rev(); // Move toward reflex_start
          objectMoving = true;
          targetEnd = false;
        }
      } else {
        if (targetEnd && digitalRead(reflex_end) == 1) {
          conveyor_rev(); // Reached reflex_end, now move toward reflex_start
          objectMoving = true;
          targetEnd = false;
        } else if (!targetEnd && digitalRead(reflex_start) == 1) {
          conveyor_fwd(); // Reached reflex_start, now move toward reflex_end
          objectMoving = true;
          targetEnd = true;
        }
      }
    }
  }

  // Red light logic for sensors (same in all modes)
  if ((digitalRead(reflex_end) == 1 || digitalRead(reflex_start) == 1) && digitalRead(e_stop) == 0) {
    digitalWrite(red_light, 1);
  } else if (digitalRead(e_stop) == 0) {
    digitalWrite(red_light, 0);
  }

  // Edge detection updates
  checkInput("switch_forward", switch_forward, prev_switch_fwd);
  checkInput("switch_reverse", switch_reverse, prev_switch_rev);
  checkInput("e_stop", e_stop, prev_estop);
  checkInput("reflex_start", reflex_start, prev_start);
  checkInput("reflex_end", reflex_end, prev_end);
  checkInput("mode_switch", mode_switch, prev_mode);
  checkOutput("red_light", red_light, prev_red);
  checkOutput("green_light", green_light, prev_green);
  bool fwd = digitalRead(R0_1) == 1 && digitalRead(R0_0) == 0;
  bool rev = digitalRead(R0_0) == 1 && digitalRead(R0_1) == 0;
  if (fwd != prev_dir_fwd || rev != prev_dir_rev) {
    prev_dir_fwd = fwd;
    prev_dir_rev = rev;
    sendUpdate("dirFwd", fwd);
    sendUpdate("dirRev", rev);
  }

  delay(10);
}