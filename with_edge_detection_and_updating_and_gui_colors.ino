// esp32_conveyor_dashboard.ino

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// === STATE TRACKING ===
bool prev_switch_forward = LOW;
bool prev_switch_reverse = LOW;
bool prev_e_stop = LOW;
bool prev_reflex_start = LOW;
bool prev_reflex_end = LOW;

// === WIFI AP AND SERVER ===
const char* ap_ssid = "ESP32_AP";
const char* ap_password = "123456789";
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

String ipStr = "";
String sta_ssid = "";
String sta_pass = "";
bool connectedToSTA = false;

void notifyClients(String key, String value) {
  ws.textAll(key + ":" + value);
}

void checkInputChange(String key, int pin, bool &prevVal) {
  bool newVal = digitalRead(pin);
  if (newVal != prevVal) {
    prevVal = newVal;
    notifyClients(key, String(newVal));
  }
}

void initWebSocket() {
  ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client,
                AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_DATA) {
      String msg = String((char*)data).substring(0, len);
      int sep = msg.indexOf(':');
      if (sep != -1) {
        String key = msg.substring(0, sep);
        String value = msg.substring(sep + 1);
        if (key == "red_light") {
          digitalWrite(red_light, value == "1" ? HIGH : LOW);
          notifyClients(key, value);
        }
        // === ADD OUTPUT CONTROLS HERE ===
        // Example:
        // if (key == "lightGreen") {
        //   digitalWrite(lightGreen, value == "1" ? HIGH : LOW);
        //   notifyClients(key, value);
        // }
      }
    }
  });
  server.addHandler(&ws);
}

String processor(const String& var) {
  if (var == "IP_ADDRESS") return ipStr;
  return String();
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html>
<head><title>ESP32 Dashboard</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  .light { width: 20px; height: 20px; border-radius: 50%; display: inline-block; margin: 5px; }
  .light.off { opacity: 0.2; }
  .red { background: red; } .green { background: green; }
  .sensor { width: 12px; height: 12px; border-radius: 50%; background: gray; display: inline-block; }
  .sensor.on { background: blue; }
</style></head>
<body>
<h2>ESP32 Conveyor Dashboard</h2>
<p>Connect to: <b>%IP_ADDRESS%</b></p>
<div id="sw3posState">Switch: Unknown</div>
<div id="e_stopState">E-Stop: Unknown</div>
<div>Lights: <span id="lightRed" class="light off red"></span><span id="lightGreen" class="light off green"></span></div>
<div id="directionArrow">Direction: Stopped</div>
<div>Laser A: <span id="reflex_start" class="sensor"></span> | Laser B: <span id="reflex_end" class="sensor"></span></div>
<script>
let ws = new WebSocket("ws://" + location.host + "/ws");
ws.onmessage = (e) => {
  let [k,v] = e.data.split(":");
  if(k === "switch_forward" || k === "switch_reverse") {
    let s = document.getElementById("sw3posState");
    if(k=="switch_forward"&&v=="1") s.innerText = "Switch: Forward";
    else if(k=="switch_reverse"&&v=="1") s.innerText = "Switch: Reverse";
    else if(v=="0") s.innerText = "Switch: Stop";
  }
  if(k === "e_stop") {
    let e = document.getElementById("e_stopState");
    e.innerText = "E-Stop: " + (v=="1" ? "Activated" : "Clear");
    e.style.color = v=="1" ? "red" : "green";
  }
  if(k === "lightRed") document.getElementById("lightRed").className = v=="1" ? "light red" : "light off red";
  if(k === "lightGreen") document.getElementById("lightGreen").className = v=="1" ? "light green" : "light off green";
  if(k === "dirFwd" || k === "dirRev") {
    let d = document.getElementById("directionArrow");
    if(k=="dirFwd"&&v=="1") d.innerText = "Direction: → Forward";
    else if(k=="dirRev"&&v=="1") d.innerText = "Direction: ← Reverse";
    else d.innerText = "Direction: Stopped";
  }
  if(k === "reflex_start") document.getElementById("reflex_start").className = v=="1" ? "sensor on" : "sensor";
  if(k === "reflex_end") document.getElementById("reflex_end").className = v=="1" ? "sensor on" : "sensor";
};
</script></body></html>
)rawliteral";

void setup() {
  Serial.begin(115200);

  pinMode(R0_0, OUTPUT); // conveyor relay 1
  pinMode(R0_1, OUTPUT); // conveyor relay 2
  pinMode(R0_2, OUTPUT); // Red light
  pinMode(R0_3, OUTPUT); // Green light

  pinMode(I0_5, INPUT); // 3 position toggle switch reverse
  pinMode(I0_6, INPUT); // 3 position toggle switch forward
  pinMode(I0_7, INPUT); // e-stop button
  pinMode(I0_8, INPUT); // Start sensor
  pinMode(I0_9, INPUT); // End sensor

  #define red_light R0_2
  #define green_light R0_3

  #define switch_reverse I0_5
  #define switch_forward I0_6
  #define e_stop I0_7
  #define reflex_start I0_8
  #define reflex_end I0_9

  WiFi.softAP(ap_ssid, ap_password);
  ipStr = WiFi.softAPIP().toString();

  initWebSocket();
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });
  server.begin();
}

void loop() {
  checkInputChange("switch_forward", switch_forward, prev_switch_forward);
  checkInputChange("switch_reverse", switch_reverse, prev_switch_reverse);
  checkInputChange("e_stop", e_stop, prev_e_stop);
  checkInputChange("reflex_start", reflex_start, prev_reflex_start);
  checkInputChange("reflex_end", reflex_end, prev_reflex_end);
  ws.cleanupClients();
  delay(10);
} // end
