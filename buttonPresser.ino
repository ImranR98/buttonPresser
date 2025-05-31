#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <EEPROM.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#define EEPROM_SIZE 512
#define SSID_ADDR 0
#define PASS_ADDR 32
#define API_URL_ADDR 96
#define SERVO_CONFIG_ADDR 224
#define LOOP_CONFIG_ADDR 248
#define INIT_FLAG_ADDR 256

IPAddress AP_LOCAL_IP(192, 168, 4, 1);
IPAddress AP_GATEWAY(192, 168, 4, 1);
IPAddress AP_SUBNET(255, 255, 255, 0);

const int SERVO_PIN = D3;

const char* AP_SSID = "ButtonPresser";
WebServer server(80);

const unsigned long API_POLL_INTERVAL = 1000;
const unsigned long STATUS_UPDATE_INTERVAL = 500;

void setLED(int r, int g, int b);
void updateStatusLED();

const char* AP_CONFIG_PAGE = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ButtonPresser Wi-Fi Configuration</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    * { box-sizing: border-box; }
    body { 
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; 
      max-width: 500px; 
      margin: 0 auto; 
      padding: 20px;
      background: #f5f7fa;
      color: #333;
    }
    .card {
      background: white;
      border-radius: 12px;
      box-shadow: 0 4px 16px rgba(0,0,0,0.1);
      padding: 30px;
      margin-top: 20px;
    }
    h1 {
      color: #2c3e50;
      text-align: center;
      margin-bottom: 25px;
    }
    label {
      display: block;
      margin-bottom: 8px;
      font-weight: 600;
      color: #34495e;
    }
    input[type="text"], 
    input[type="password"],
    textarea {
      width: 100%;
      padding: 14px;
      margin-bottom: 20px;
      border: 1px solid #ddd;
      border-radius: 8px;
      font-size: 16px;
      transition: border 0.3s;
    }
    input:focus, textarea:focus {
      border-color: #3498db;
      outline: none;
      box-shadow: 0 0 0 2px rgba(52, 152, 219, 0.2);
    }
    .btn {
      display: block;
      width: 100%;
      padding: 14px;
      background: #3498db;
      color: white;
      border: none;
      border-radius: 8px;
      font-size: 16px;
      font-weight: 600;
      cursor: pointer;
      transition: background 0.3s;
    }
    .btn:hover {
      background: #2980b9;
    }
    .btn-clear {
      background: #e74c3c;
      margin-top: 10px;
    }
    .btn-clear:hover {
      background: #c0392b;
    }
    .message {
      padding: 15px;
      margin-top: 20px;
      border-radius: 8px;
      text-align: center;
    }
    .success {
      background: #d4edda;
      color: #155724;
    }
    .error {
      background: #f8d7da;
      color: #721c24;
    }
    .info {
      background: #d1ecf1;
      color: #0c5460;
      padding: 15px;
      border-radius: 8px;
      margin: 20px 0;
    }
    .status-indicator {
      display: inline-block;
      width: 12px;
      height: 12px;
      border-radius: 50%;
      margin-right: 8px;
    }
    .connected { background: #2ecc71; }
    .disconnected { background: #e74c3c; }
    .flex-row {
      display: flex;
      gap: 15px;
      margin-bottom: 20px;
    }
    .flex-row > div {
      flex: 1;
      padding: 15px;
      background: #f8f9fa;
      border-radius: 8px;
      text-align: center;
    }
    pre {
      white-space: pre-wrap;
    }
  </style>
</head>
<body>
  <div class="card">
    <h1>Connect ButtonPresser to Wi-Fi</h1>
    
    <form id="configForm">
      <label for="ssid">Wi-Fi Network (SSID):</label>
      <input type="text" id="ssid" name="ssid" placeholder="Your Wi-Fi name" required>
      
      <label for="password">Password:</label>
      <input type="password" id="password" name="password" placeholder="Your Wi-Fi password">
      
      <button type="submit" class="btn">Save Configuration</button>
      <button type="button" id="clearBtn" class="btn btn-clear">Clear Credentials</button>
    </form>
    
    <div id="message" class="message"></div>
  </div>
  
  <script>
    document.getElementById('configForm').addEventListener('submit', async (e) => {
      e.preventDefault();
      const form = e.target;
      const messageDiv = document.getElementById('message');
      messageDiv.textContent = 'Saving configuration...';
      messageDiv.className = 'message info';
      
      const formData = new FormData(form);
      await sendFormData(formData, messageDiv, 'Credentials saved! Device will restart shortly...');
    });
    
    document.getElementById('clearBtn').addEventListener('click', () => {
      document.getElementById('ssid').value = '';
      document.getElementById('password').value = '';
      document.getElementById('configForm').dispatchEvent(new Event('submit'));
    });
    
    async function sendFormData(formData, messageDiv, successMsg) {
      let attempts = 0;
      const maxAttempts = 5;
      
      while (attempts < maxAttempts) {
        try {
          const response = await fetch('/', {
            method: 'POST',
            body: formData
          });
          
          if (response.ok) {
            messageDiv.textContent = successMsg;
            messageDiv.className = 'message success';
            setTimeout(() => { window.location.reload(); }, 3000);
            return;
          }
          throw new Error('Network error');
        } catch (error) {
          attempts++;
          if (attempts >= maxAttempts) {
            messageDiv.textContent = `Failed after ${maxAttempts} attempts. Please try again.`;
            messageDiv.className = 'message error';
          } else {
            messageDiv.textContent = `Saving (attempt ${attempts}/${maxAttempts})...`;
            await new Promise(resolve => setTimeout(resolve, 1000));
          }
        }
      }
    }
  </script>
</body>
</html>
)rawliteral";

struct ServoConfig {
  int angleMin = 0;
  int angleMax = 188;
  int pwmMin = 500;
  int pwmMax = 2500;
  int normalPos = 87;
  int pressPos = 130;
};

struct LoopConfig {
  int interval = 21600;
  int pressDuration = 300;
};

String apiUrl = "";
unsigned long lastApiCall = 0;
unsigned long lastLoopPress = 0;
unsigned long lastStatusUpdate = 0;
bool isInAPMode = false;

ServoConfig servoConfig;
LoopConfig loopConfig;

enum DeviceState {
  STATE_NO_CREDENTIALS,
  STATE_WIFI_FAIL,
  STATE_AP_MODE,
  STATE_API_FAIL,
  STATE_ALL_OK
};

DeviceState currentState = STATE_NO_CREDENTIALS;

void setup() {
  Serial.begin(115200);
  delay(5000);
  Serial.println("Setup starting");

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  setLED(255, 255, 255);

  EEPROM.begin(EEPROM_SIZE);

  pinMode(SERVO_PIN, OUTPUT);

  uint16_t initFlag;
  EEPROM.get(INIT_FLAG_ADDR, initFlag);

  if (initFlag != 0x55AA) {
    initializeEEPROM();
  }

  loadConfigurations();

  moveServoToAngle(servoConfig.normalPos);

  if (tryConnectToWiFi()) {
    startWebServerClientMode();
    currentState = (apiUrl != "") ? STATE_ALL_OK : STATE_API_FAIL;
  } else {
    startAPMode();
    currentState = STATE_AP_MODE;
  }

  updateStatusLED();
  Serial.println("Setup complete");
}

void loop() {
  server.handleClient();
  handleScheduledPresses();

  if (WiFi.status() == WL_CONNECTED && apiUrl != "") {
    if (millis() - lastApiCall >= API_POLL_INTERVAL) {
      fetchAndParseApi();
      lastApiCall = millis();
    }
  }

  if (millis() - lastStatusUpdate >= STATUS_UPDATE_INTERVAL) {
    updateStatusLED();
    lastStatusUpdate = millis();
  }

  delay(100);
}

void initializeEEPROM() {
  Serial.println("Initializing EEPROM with default values");
  writeStringToEEPROM(SSID_ADDR, "");
  writeStringToEEPROM(PASS_ADDR, "");
  writeStringToEEPROM(API_URL_ADDR, "");
  EEPROM.put(SERVO_CONFIG_ADDR, servoConfig);
  LoopConfig defaultLoop;
  EEPROM.put(LOOP_CONFIG_ADDR, defaultLoop);
  uint16_t initFlag = 0x55AA;
  EEPROM.put(INIT_FLAG_ADDR, initFlag);
  EEPROM.commit();
}

void loadConfigurations() {
  Serial.println("Loading configurations");
  
  EEPROM.get(SERVO_CONFIG_ADDR, servoConfig);
  EEPROM.get(LOOP_CONFIG_ADDR, loopConfig);

  if (loopConfig.interval < 0) loopConfig.interval = 0;
  
  apiUrl = readStringFromEEPROM(API_URL_ADDR, 128);
  
  Serial.printf("Loaded API URL: %s\n", apiUrl.c_str());
  Serial.printf("Loaded loop interval: %ds, press duration: %dms\n", 
                loopConfig.interval, loopConfig.pressDuration);
}

void writeStringToEEPROM(int addr, const String& str) {
  int len = str.length();
  EEPROM.write(addr, len);
  for (int i = 0; i < len; i++) {
    EEPROM.write(addr + 1 + i, str[i]);
  }
  EEPROM.commit();
}

String readStringFromEEPROM(int addr, int maxLen) {
  String result = "";
  int len = EEPROM.read(addr);

  if (len > 0 && len < maxLen) {
    for (int i = 0; i < len; i++) {
      result += (char)EEPROM.read(addr + 1 + i);
    }
  }
  return result;
}

void saveLoopConfig() {
  EEPROM.put(LOOP_CONFIG_ADDR, loopConfig);
  EEPROM.commit();
}

bool tryConnectToWiFi() {
  currentState = STATE_WIFI_FAIL;
  String ssid = readStringFromEEPROM(SSID_ADDR, 32);
  String pass = readStringFromEEPROM(PASS_ADDR, 64);

  if (ssid == "") {
    currentState = STATE_NO_CREDENTIALS;
    return false;
  }

  Serial.printf("Connecting to: %s\n", ssid.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());

  for (int i = 0; i < 30; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("Connected! IP: ");
      Serial.println(WiFi.localIP());
      return true;
    }
    delay(500);
  }

  Serial.println("\nConnection failed");
  currentState = STATE_WIFI_FAIL;
  return false;
}

void startAPMode() {
  Serial.println("Starting AP mode");
  isInAPMode = true;
  WiFi.softAPConfig(AP_LOCAL_IP, AP_GATEWAY, AP_SUBNET);
  WiFi.softAP(AP_SSID);

  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  currentState = STATE_AP_MODE;

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", AP_CONFIG_PAGE);
  });

  server.on("/", HTTP_POST, []() {
    String newSSID = server.arg("ssid");
    String newPass = server.arg("password");

    if (newSSID == "") {
      writeStringToEEPROM(SSID_ADDR, "");
      writeStringToEEPROM(PASS_ADDR, "");
      server.send(200, "text/html", 
        "<div class='card'><h1>Credentials Cleared!</h1><p>Device will restart in AP mode.</p></div>");
    } else {
      writeStringToEEPROM(SSID_ADDR, newSSID);
      writeStringToEEPROM(PASS_ADDR, newPass);
      server.send(200, "text/html", 
        "<div class='card'><h1>Credentials Saved!</h1><p>Device will attempt connection.</p></div>");
    }

    delay(2000);
    ESP.restart();
  });

  server.begin();
}

//----------- Motor Control -----------
void moveServoToAngle(int angle) {
  angle = constrain(angle, servoConfig.angleMin, servoConfig.angleMax);
  int pulseWidth = map(angle, servoConfig.angleMin, servoConfig.angleMax,
                       servoConfig.pwmMin, servoConfig.pwmMax);

  digitalWrite(SERVO_PIN, HIGH);
  delayMicroseconds(pulseWidth);
  digitalWrite(SERVO_PIN, LOW);
  delay(30);
}

void pressButton(int pressDuration, bool isSinglePress) {
  Serial.printf("Pressing button for %dms\n", pressDuration);
  
  setLED(isSinglePress ? 255 : 0, 0, 255);

  unsigned long startTime = millis();
  while (millis() - startTime < pressDuration) {
    moveServoToAngle(servoConfig.pressPos);
    delay(20);
  }

  moveServoToAngle(servoConfig.normalPos);
  updateStatusLED();
}

void handleScheduledPresses() {
  if (loopConfig.interval > 0) {
    unsigned long timeSinceLast = millis() - lastLoopPress;
    if (timeSinceLast >= loopConfig.interval * 1000UL) {
      pressButton(loopConfig.pressDuration, false);
      lastLoopPress = millis();
    }
  }
}

void fetchAndParseApi() {
  static WiFiClient normalClient;
  static WiFiClientSecure secureClient;
  HTTPClient http;  // Declare HTTPClient inside the function

  bool isHttps = apiUrl.startsWith("https://");
  bool httpBeginSuccess = false;

  if (isHttps) {
    secureClient.setInsecure(); // Skip certificate verification
    httpBeginSuccess = http.begin(secureClient, apiUrl);
  } else {
    httpBeginSuccess = http.begin(normalClient, apiUrl);
  }

  if (httpBeginSuccess) {
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      processJsonCommand(payload);
      currentState = STATE_ALL_OK;
    } else {
      Serial.printf("API request failed: %s\n", http.errorToString(httpCode).c_str());
      currentState = STATE_API_FAIL;
    }
    http.end();
  } else {
    Serial.println("Failed to connect to API server");
    currentState = STATE_API_FAIL;
  }
}

void processJsonCommand(String payload) {
  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.print("JSON parse failed: ");
    Serial.println(error.c_str());
    return;
  }

  if (doc.containsKey("command")) {
    String command = doc["command"];

    if (command == "loop") {
      if (doc.containsKey("seconds")) {
        loopConfig.interval = doc["seconds"];
      }
      if (doc.containsKey("pressDuration")) {
        loopConfig.pressDuration = doc["pressDuration"];
      }
      saveLoopConfig();
    } 
    else if (command == "single") {
      int customDuration = loopConfig.pressDuration;
      if (doc.containsKey("pressDuration")) {
        customDuration = doc["pressDuration"];
      }
      pressButton(customDuration, true);
    }
  }

  bool configChanged = false;
  if (doc.containsKey("angleMin")) {
    servoConfig.angleMin = doc["angleMin"];
    configChanged = true;
  }
  if (doc.containsKey("angleMax")) {
    servoConfig.angleMax = doc["angleMax"];
    configChanged = true;
  }
  if (doc.containsKey("pwmMin")) {
    servoConfig.pwmMin = doc["pwmMin"];
    configChanged = true;
  }
  if (doc.containsKey("pwmMax")) {
    servoConfig.pwmMax = doc["pwmMax"];
    configChanged = true;
  }
  if (doc.containsKey("normalPos")) {
    servoConfig.normalPos = doc["normalPos"];
    configChanged = true;
  }
  if (doc.containsKey("pressPos")) {
    servoConfig.pressPos = doc["pressPos"];
    configChanged = true;
  }

  if (configChanged) {
    EEPROM.put(SERVO_CONFIG_ADDR, servoConfig);
    EEPROM.commit();
    Serial.println("Servo config updated");
  }
}

void startWebServerClientMode() {
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", CLIENT_CONFIG_PAGE());
  });

  server.on("/seturl", HTTP_POST, []() {
    apiUrl = server.arg("apiurl");
    writeStringToEEPROM(API_URL_ADDR, apiUrl);
    currentState = (apiUrl != "") ? STATE_ALL_OK : STATE_API_FAIL;
    server.send(200, "text/plain", "API URL saved!");
  });

  server.on("/command", HTTP_POST, []() {
    String jsonCommand = server.arg("json");
    if (jsonCommand != "") {
      processJsonCommand(jsonCommand);
      server.send(200, "text/plain", "Command processed");
    } else {
      server.send(400, "text/plain", "Empty command");
    }
  });

  server.begin();
}

String CLIENT_CONFIG_PAGE() {
  String wifiStatus = (WiFi.status() == WL_CONNECTED) ? 
    "<span class='status-indicator connected'></span>Connected to " + WiFi.SSID() : 
    "<span class='status-indicator disconnected'></span>Disconnected";

  String apiStatus = (currentState == STATE_ALL_OK) ? 
    "<span class='status-indicator connected'></span>Operational" : 
    "<span class='status-indicator disconnected'></span>Not configured";

  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ButtonPresser Configuration</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    * { box-sizing: border-box; }
    body { 
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; 
      max-width: 600px; 
      margin: 0 auto; 
      padding: 20px;
      background: #f5f7fa;
      color: #333;
    }
    .card {
      background: white;
      border-radius: 12px;
      box-shadow: 0 4px 16px rgba(0,0,0,0.1);
      padding: 30px;
      margin-bottom: 25px;
    }
    h1, h2 {
      color: #2c3e50;
      margin-top: 0;
    }
    label {
      display: block;
      margin-bottom: 8px;
      font-weight: 600;
      color: #34495e;
    }
    input[type="text"], 
    textarea {
      width: 100%;
      padding: 14px;
      margin-bottom: 20px;
      border: 1px solid #ddd;
      border-radius: 8px;
      font-size: 16px;
      transition: border 0.3s;
    }
    input:focus, textarea:focus {
      border-color: #3498db;
      outline: none;
      box-shadow: 0 0 0 2px rgba(52, 152, 219, 0.2);
    }
    .btn {
      display: block;
      width: 100%;
      padding: 14px;
      background: #3498db;
      color: white;
      border: none;
      border-radius: 8px;
      font-size: 16px;
      font-weight: 600;
      cursor: pointer;
      transition: background 0.3s;
      margin-bottom: 15px;
    }
    .btn:hover {
      background: #2980b9;
    }
    .btn-command {
      background: #9b59b6;
    }
    .btn-command:hover {
      background: #8e44ad;
    }
    .status-container {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 15px;
      margin-bottom: 25px;
    }
    .status-box {
      padding: 20px;
      background: #f8f9fa;
      border-radius: 8px;
      text-align: center;
    }
    .status-indicator {
      display: inline-block;
      width: 12px;
      height: 12px;
      border-radius: 50%;
      margin-right: 8px;
    }
    .connected { background: #2ecc71; }
    .disconnected { background: #e74c3c; }
    .settings-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 15px;
      margin-bottom: 25px;
    }
    .setting-card {
      padding: 15px;
      background: #f8f9fa;
      border-radius: 8px;
    }
    .setting-value {
      font-size: 24px;
      font-weight: bold;
      color: #3498db;
      text-align: center;
      margin: 10px 0;
    }
    .message {
      padding: 15px;
      margin: 20px 0;
      border-radius: 8px;
      text-align: center;
    }
    .success {
      background: #d4edda;
      color: #155724;
    }
    .error {
      background: #f8d7da;
      color: #721c24;
    }
    .info {
      background: #d1ecf1;
      color: #0c5460;
    }
    pre {
      white-space: pre-wrap;
    }
  </style>
</head>
<body>
  <div class="card">
    <h1>ButtonPresser Configuration</h1>
    
    <div class="status-container">
      <div class="status-box">
        <h3>Wi-Fi Status</h3>
        <p>)rawliteral" + wifiStatus + R"rawliteral(</p>
      </div>
      <div class="status-box">
        <h3>Config Endpoint Status</h3>
        <p>)rawliteral" + apiStatus + R"rawliteral(</p>
      </div>
    </div>
    
    <form id="urlForm">
      <label for="apiurl">Config URL:</label>
      <input type="text" id="apiurl" name="apiurl" value=")rawliteral" + apiUrl + R"rawliteral(" 
             placeholder="https://example.com/api">
      <button type="submit" class="btn">Save Config URL</button>
    </form>
    <div id="urlMessage" class="message"></div>
  </div>
  
  <div class="card">
    <h2>Current Settings</h2>
    
    <div class="settings-grid">
      <div class="setting-card">
        <h3>Loop Interval</h3>
        <div class="setting-value">)rawliteral" + String(loopConfig.interval) + R"rawliteral( seconds</div>
        <p>Time between automatic presses</p>
      </div>
      <div class="setting-card">
        <h3>Press Duration</h3>
        <div class="setting-value">)rawliteral" + String(loopConfig.pressDuration) + R"rawliteral( ms</div>
        <p>Duration of each button press</p>
      </div>
    </div>
    
    <h2>Manual Control</h2>
    <form id="commandForm">
      <label for="json">JSON Command:</label>
      <textarea id="json" name="json" rows="4" placeholder='{"command":"single"}'>{ "command":"single", "pressDuration": 100 }</textarea>
      
      <div class="info card">
        <p><strong>Examples:</strong></p>
        <p>Single press: <pre>{ "command":"single" }</pre></p>
        <p>Set loop (will be overridden immediately if a valid config URL is set): <pre>{ "command":"loop", "seconds":10, "pressDuration":50 }</pre></p>
        <p>Adjust servo: <pre>{ "angleMin": 0, "angleMax": 188, "pwmMin": 500, "pwmMax": 2500, "normalPos": 115, "pressPos": 87 }</pre></p>
      </div>
      
      <button type="submit" class="btn btn-command">Send Command</button>
    </form>
    <div id="commandMessage" class="message"></div>
  </div>
  
  <script>
    document.getElementById('urlForm').addEventListener('submit', async (e) => {
      e.preventDefault();
      const messageDiv = document.getElementById('urlMessage');
      messageDiv.textContent = 'Saving API URL...';
      messageDiv.className = 'message info';
      
      const formData = new FormData(e.target);
      await sendRequest('/seturl', formData, messageDiv, 'API URL saved!');
    });
    
    document.getElementById('commandForm').addEventListener('submit', async (e) => {
      e.preventDefault();
      const messageDiv = document.getElementById('commandMessage');
      messageDiv.textContent = 'Sending command...';
      messageDiv.className = 'message info';
      
      const formData = new FormData(e.target);
      await sendRequest('/command', formData, messageDiv, 'Command processed successfully');
    });
    
    async function sendRequest(url, formData, messageDiv, successMsg) {
      try {
        const response = await fetch(url, {
          method: 'POST',
          body: formData
        });
        
        if (response.ok) {
          const text = await response.text();
          messageDiv.textContent = text;
          messageDiv.className = 'message success';
          setTimeout(() => { 
            messageDiv.textContent = successMsg;
            if(url === '/seturl') window.location.reload(); 
          }, 2000);
        } else {
          throw new Error(await response.text());
        }
      } catch (error) {
        messageDiv.textContent = `Error: ${error.message}`;
        messageDiv.className = 'message error';
      }
    }
  </script>
</body>
</html>
)rawliteral";

  return page;
}

void setLED(int r, int g, int b) {
  analogWrite(LED_RED, 255 - r);
  analogWrite(LED_GREEN, 255 - g);
  analogWrite(LED_BLUE, 255 - b);
}

void updateStatusLED() {
  switch (currentState) {
    case STATE_NO_CREDENTIALS: setLED(255, 255, 255); break; // White
    case STATE_WIFI_FAIL:      setLED(255, 0, 0);     break; // Red
    case STATE_AP_MODE:        setLED(255, 255, 0);   break; // Yellow
    case STATE_API_FAIL:       setLED(255, 165, 0);   break; // Orange
    case STATE_ALL_OK:         setLED(0, 255, 0);     break; // Green
  }
}