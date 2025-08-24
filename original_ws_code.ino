#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

// AP credentials
const char* apSSID = "ESP32_Config";
const char* apPassword = "12345678";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

String staSSID = "";
String staPassword = "";
bool wifiConnected = false;

void setup() {
  Serial.begin(115200);

  pinMode(I0_5, INPUT);
  pinMode(R0_2, OUTPUT);
  digitalWrite(R0_2, LOW);

  // Start AP mode
  WiFi.softAP(apSSID, apPassword);
  Serial.println("AP Started. IP: " + WiFi.softAPIP().toString());

  // Wi-Fi credential input
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request){
    request->send(200, "text/html", R"rawliteral(
      <h3>Wi-Fi Setup</h3>
      <form action="/connect" method="POST">
        SSID: <input name="ssid"><br>
        Password: <input name="password" type="password"><br>
        <input type="submit" value="Connect">
      </form>
      <br><a href='/status'>Check Status</a>
    )rawliteral");
  });

  server.on("/connect", HTTP_POST, [](AsyncWebServerRequest* request){
    staSSID = request->getParam("ssid", true)->value();
    staPassword = request->getParam("password", true)->value();
    WiFi.begin(staSSID.c_str(), staPassword.c_str());
    request->send(200, "text/html", "Attempting connection...<br><a href='/status'>Check Status</a>");
    Serial.println("Connecting to: " + staSSID);
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest* request){
    String html = "<h3>Status</h3>";
    html += "AP IP: " + WiFi.softAPIP().toString() + "<br>";
    if (WiFi.status() == WL_CONNECTED) {
      html += "Connected to: " + staSSID + "<br>";
      html += "STA IP: <b>" + WiFi.localIP().toString() + "</b><br>";
      html += "Go to <a href='http://" + WiFi.localIP().toString() + "/control'>Control Page</a>";
    } else {
      html += "Not connected.";
    }
    request->send(200, "text/html", html);
  });

  server.on("/control", HTTP_GET, [](AsyncWebServerRequest* request){
    request->send(200, "text/html", controlPageHTML());
  });

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.begin();
}

void loop() {
  if (!wifiConnected && WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("Connected! IP: " + WiFi.localIP().toString());
  }
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) {
    String msg = String((char*)data).substring(0, len);
    if (msg == "toggle:R0_2") {
      digitalWrite(R0_2, !digitalRead(R0_2));
    }
    sendIOState();
  }
}

void sendIOState() {
  String json = "{";
  json += "\"R0_2\":" + String(digitalRead(R0_2)) + ",";
  json += "\"I0_5\":" + String(digitalRead(I0_5));
  json += "}";
  ws.textAll(json);
}

String controlPageHTML() {
  return R"rawliteral(
    <!DOCTYPE html><html>
    <head>
      <meta charset="UTF-8">
      <title>ESP32 I/O</title>
      <style>
        table { border-collapse: collapse; }
        td, th { padding: 6px 10px; border: 1px solid #999; }
      </style>
      <script>
        let ws;
        function init() {
          ws = new WebSocket('ws://' + location.host + '/ws');
          ws.onmessage = function(event) {
            const data = JSON.parse(event.data);
            document.getElementById("r0_2_state").innerText = data.R0_2;
            document.getElementById("i0_5_state").innerText = data.I0_5;
          };
        }
        function toggleR0_2() {
          ws.send("toggle:R0_2");
        }
        window.onload = init;
      </script>
    </head>
    <body>
      <h2>ESP32 I/O Control</h2>
      <table>
        <tr><th>Label</th><th>State</th><th>Action</th></tr>
        <tr><td>R0_2</td><td id="r0_2_state">-</td><td><button onclick="toggleR0_2()">Toggle</button></td></tr>
        <tr><td>I0_5</td><td id="i0_5_state">-</td><td>—</td></tr>
      </table>
    </body>
    </html>
  )rawliteral";
}
