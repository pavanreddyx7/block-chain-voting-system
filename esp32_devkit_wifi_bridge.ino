#include <WiFi.h>
#include <HTTPClient.h>

const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* API_URL = "http://YOUR_SERVER_IP:3000/api/vote";

WiFiClient client;
unsigned long voteCounter = 0;

// ESP32 DevKit V1 UART2 pins for Mega communication.
const int ESP32_RX2_PIN = 16;
const int ESP32_TX2_PIN = 17;

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Connecting to WiFi...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());
}

void ensureWifiConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  connectWifi();
}

bool sendVoteToServer(char candidate) {
  ensureWifiConnected();

  HTTPClient http;
  http.begin(client, API_URL);
  http.addHeader("Content-Type", "application/json");

  voteCounter++;

  String requestId = "REQ-";
  requestId += String(millis());
  requestId += "-";
  requestId += String(voteCounter);

  String payload = "{\"boothId\":\"BOOTH-01\",\"voteId\":\"";
  payload += requestId;
  payload += "\",\"candidate\":\"";
  payload += candidate;
  payload += "\",\"device\":\"ESP32-DEVKIT-V1\"}";

  Serial.println("Sending payload to server:");
  Serial.println(payload);

  int httpCode = http.POST(payload);
  String responseBody = http.getString();
  Serial.print("HTTP code: ");
  Serial.println(httpCode);
  Serial.print("Server response: ");
  Serial.println(responseBody);
  bool ok = httpCode > 0 && httpCode < 300;
  http.end();

  return ok;
}

void handleMegaMessage(String message) {
  message.trim();
  Serial.print("Received from Mega: ");
  Serial.println(message);

  if (!message.startsWith("VOTE:")) {
    Serial.println("Ignored invalid message");
    return;
  }

  if (message.length() < 6) {
    Serial.println("Message too short");
    Serial2.println("FAIL");
    return;
  }

  char candidate = message.charAt(5);
  bool success = sendVoteToServer(candidate);

  if (success) {
    Serial2.println("SUCCESS");
  } else {
    Serial2.println("FAIL");
  }
}

void setup() {
  Serial.begin(9600);
  delay(1000);
  Serial.println("ESP32 booted");
  Serial2.begin(9600, SERIAL_8N1, ESP32_RX2_PIN, ESP32_TX2_PIN);
  Serial.print("UART2 RX pin: ");
  Serial.println(ESP32_RX2_PIN);
  Serial.print("UART2 TX pin: ");
  Serial.println(ESP32_TX2_PIN);
  connectWifi();
  Serial.println("ESP32 DevKit V1 ready");
}

void loop() {
  ensureWifiConnected();

  if (Serial2.available()) {
    String message = Serial2.readStringUntil('\n');
    handleMegaMessage(message);
  }
}
