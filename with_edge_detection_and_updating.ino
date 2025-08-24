#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

// === I/O DEFINITIONS ===
bool prev_I0_6 = false;
bool state_R0_2 = false;

// === INPUT TEMPLATE ===
// bool prev_I0_X = false;

// === OUTPUT TEMPLATE ===
// bool state_R0_X = false;

const char* apSSID = "ESP32_Config";
const char* apPassword = "123456789";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

String ssid = "";
String password = "";
bool wifiConnected = false;

// === HTML PAGES ===

const char* configPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head><title>WiFi Setup</title></head>
<body>
  <h3>Connect to Wi-Fi</h3>
  <form action="/connect" method="POST">
    SSID: <input name="ssid"><br>
    Password: <input name="password"><br>
    <input type="submit" value="Connect">
  </form>
</body>
</html>
)rawliteral";

String successPage(String ip) {
  return "<html><body><h3>Connected!</h3><p>Access at: <b>" + ip + "</b></p></body></html>";
}

const char* controlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32 Control</title>
  <script>
    var ws = new WebSocket("ws://" + window.location.host + "/ws");

    function toggle(pin) {
      var val = document.getElementById(pin).innerText === "1" ? "0" : "1";
      ws.send(pin + ":" + val);
    }

    ws.onmessage = function(event) {
      let data = event.data;
      let parts = data.split(":");
      let pin = parts[0];
      let val = parts[1];
      let el = document.getElementById(pin);
      if (el) el.innerText = val;
    };
  </script>
</head>
<body>
  <h2>ESP32 Conveyor Control</h2>
  <table border="1" cellpadding="6">
    <tr><th>Pin</th><th>State</th><th>Control</th></tr>

    <tr>
      <td>I0_6</td>
      <td id="I0_6">?</td>
      <td></td>
    </tr>

    <tr>
      <td>R0_2</td>
      <td id="R0_2">0</td>
      <td><button onclick="toggle('R0_2')">Toggle</button></td>
    </tr>

    <!-- === TEMPLATE ===
    <tr>
      <td>I0_X</td>
      <td id="I0_X">?</td>
      <td></td>
    </tr>
    <tr>
      <td>R0_X</td>
      <td id="R0_X">0</td>
      <td><button onclick="toggle('R0_X')">Toggle</button></td>
    </tr>
    -->
  </table>
</body>
</html>
)rawliteral";

// === FUNCTIONS ===

void checkInputChange(const char* name, int pin, bool &prevState) {
  bool current = digitalRead(pin);
  if (current != prevState) {
    prevState = current;
    String msg = String(name) + ":" + String(current);
    ws.textAll(msg);
  }
}

void handleWebSocket(AsyncWebSocketClient *client, void *arg, uint8_t *data, size_t len) {
  String msg = "";
  for (size_t i = 0; i < len; i++) msg += (char)data[i];

  if (msg.startsWith("R0_2:")) {
    String val = msg.substring(5);
    state_R0_2 = (val == "1");
    digitalWrite(R0_2, state_R0_2);
    ws.textAll("R0_2:" + val);
  }

  // === OUTPUT TEMPLATE ===
  // if (msg.startsWith("R0_X:")) {
  //   String val = msg.substring(5);
  //   state_R0_X = (val == "1");
  //   digitalWrite(R0_X, state_R0_X);
  //   ws.textAll("R0_X:" + val);
  // }
}

void connectToWiFi() {
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.print("Connecting to WiFi");
  for (int i = 0; i < 20; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      Serial.println("\nConnected!");
      break;
    }
    delay(500);
    Serial.print(".");
  }
}

// === SETUP ===

void setup() {
  Serial.begin(115200);

  // === I/O Setup ===
  pinMode(I0_6, INPUT);
  pinMode(R0_2, OUTPUT);
  digitalWrite(R0_2, LOW);

  // === ADDITIONAL I/O SETUP ===
  // pinMode(I0_X, INPUT);
  // pinMode(R0_X, OUTPUT);
  // digitalWrite(R0_X, LOW);

  WiFi.softAP(apSSID, apPassword);
  IPAddress AP_IP = WiFi.softAPIP();
  Serial.println("AP IP: " + AP_IP.toString());

  // WebSocket
  ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client,
                AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_DATA) handleWebSocket(client, arg, data, len);
  });
  server.addHandler(&ws);

  // Routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (wifiConnected) {
      request->send(200, "text/html", controlPage);
    } else {
      request->send(200, "text/html", configPage);
    }
  });

  server.on("/connect", HTTP_POST, [](AsyncWebServerRequest *request) {
    ssid = request->arg("ssid");
    password = request->arg("password");
    connectToWiFi();

    if (wifiConnected) {
      String page = successPage(WiFi.localIP().toString());
      request->send(200, "text/html", page);
    } else {
      request->send(200, "text/html", "<p>Connection failed. Try again.</p>");
    }
  });

  server.begin();
}

// === LOOP ===

void loop() {
  checkInputChange("I0_6", I0_6, prev_I0_6);

  // === ADDITIONAL INPUTS ===
  // checkInputChange("I0_X", I0_X, prev_I0_X);

  ws.cleanupClients();
}
