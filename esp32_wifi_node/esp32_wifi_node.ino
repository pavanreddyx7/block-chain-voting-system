/*
 ============================================================
  BLOCKCHAIN VOTING MACHINE — ESP32 DevKit V1 WiFi NODE
  File   : esp32_wifi_node.ino
  Board  : ESP32 Dev Module  (Tools > Board > ESP32 Dev Module)
  Author : Pavan Reddy
 ============================================================

  WHAT THIS CODE DOES
  -------------------
  1. Connects to WiFi on boot.
  2. Syncs real-world time from NTP servers.
  3. Listens on UART2 (GPIO16/17) for "VOTE:X" from the Arduino Mega.
  4. Builds a JSON payload and HTTP POSTs it to the Node.js backend:
       POST http://<SERVER_IP>:3000/api/vote
       Body: { boothId, voteId, candidate, device, timestamp }
  5. Reads the HTTP response code and replies to the Mega:
       201 → "SUCCESS"
       409 → "DUPLICATE"
       network error → "BUSY"
       anything else → "FAIL"

  ──────────────────────────────────────────────────────────
  BACKEND API CONTRACT  (server.js  POST /api/vote)
  ──────────────────────────────────────────────────────────
  Field       Type    Example
  boothId     string  "BOOTH-01"
  voteId      string  "BOOTH-01-AA:BB:CC:DD:EE:FF-000001"
  candidate   string  "A" | "B" | "C" | "D"
  device      string  "ESP32"
  timestamp   string  "2026-04-26T10:30:00Z"   (ISO 8601 UTC)
  ──────────────────────────────────────────────────────────

  HARDWARE CONNECTIONS
  --------------------
  ESP32 GPIO16 (UART2 RX)  ← Mega pin 18 TX1   (via 1kΩ/2kΩ voltage divider)
  ESP32 GPIO17 (UART2 TX)  → Mega pin 19 RX1   (direct — 3.3 V is safe for Mega RX)
  ESP32 GND                — Mega GND           (common ground is mandatory)
  ESP32 VIN or 3V3         — Power from USB or shared 5 V rail

  Voltage divider on Mega TX1 → ESP32 RX:
    Mega TX1 → 1kΩ → node ──────→ ESP32 GPIO16
                             |
                           2kΩ
                             |
                            GND

  BEFORE FLASHING — update the three constants below:
    WIFI_SSID   your WiFi network name
    WIFI_PASS   your WiFi password
    SERVER_IP   your computer's local IPv4 address  (run `ipconfig` on Windows)

  LIBRARIES  (built-in with ESP32 board package — no extra install needed)
    WiFi, HTTPClient, Preferences, time.h
 ============================================================
*/

#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <time.h>

// ════════════════════════════════════════════════════════════
//  USER CONFIGURATION — edit before every flash
// ════════════════════════════════════════════════════════════
static const char* WIFI_SSID   = "X7";
static const char* WIFI_PASS   = "1234567890";
static const char* SERVER_IP   = "10.20.255.2";           // <── your PC's local IP

// These can also be changed per-booth
static const char* BOOTH_ID    = "BOOTH-01";
static const int   SERVER_PORT = 3000;

// ── UART2 GPIO pins ───────────────────────────────────────────
#define UART2_RX_PIN  16    // receives from Mega TX1 (via voltage divider)
#define UART2_TX_PIN  17    // sends    to  Mega RX1

// ── Timeouts & retries ────────────────────────────────────────
#define WIFI_CONNECT_TIMEOUT_MS   20000
#define HTTP_TIMEOUT_MS           10000
#define NTP_SYNC_TIMEOUT_MS       12000
#define WIFI_RECONNECT_TIMEOUT_MS  8000

// ════════════════════════════════════════════════════════════
//  GLOBALS
// ════════════════════════════════════════════════════════════
HardwareSerial megaSerial(2);   // UART2

Preferences prefs;
uint32_t    voteCounter = 0;    // persisted across reboots
String      macAddress  = "";
String      apiURL      = "";


// ════════════════════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println(F("\n[ESP32] ── Blockchain Voting Node booting ──"));

  // Open UART2 to talk to the Mega
  megaSerial.begin(9600, SERIAL_8N1, UART2_RX_PIN, UART2_TX_PIN);

  // Cache MAC early — available from hardware before WiFi connects
  WiFi.mode(WIFI_STA);
  macAddress = WiFi.macAddress();
  Serial.printf("[ESP32] MAC Address: %s\n", macAddress.c_str());

  // Build the API URL once
  apiURL = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + "/api/vote";
  Serial.printf("[ESP32] API endpoint: %s\n", apiURL.c_str());

  // Load persistent vote counter from flash
  prefs.begin("voting", false);
  voteCounter = prefs.getUInt("counter", 0);
  Serial.printf("[ESP32] Persistent vote counter loaded: %u\n", voteCounter);

  // Connect WiFi
  connectToWiFi();

  // Sync real-world time via NTP
  syncNTPTime();

  Serial.println(F("[ESP32] ── Ready — listening for votes from Mega ──\n"));
}


// ════════════════════════════════════════════════════════════
//  MAIN LOOP
// ════════════════════════════════════════════════════════════
void loop() {
  if (!megaSerial.available()) return;

  // Read one line sent by the Mega
  String incoming = megaSerial.readStringUntil('\n');
  incoming.trim();

  if (incoming.length() == 0) return;

  Serial.printf("[ESP32] Received from Mega: \"%s\"\n", incoming.c_str());

  // Expect exact format: "VOTE:X"  where X ∈ {A, B, C, D}
  if (incoming.length() != 6 || !incoming.startsWith("VOTE:")) {
    Serial.println(F("[ESP32] Malformed message — ignoring."));
    return;
  }

  char candidateChar = incoming.charAt(5);

  if (candidateChar != 'A' && candidateChar != 'B' &&
      candidateChar != 'C' && candidateChar != 'D') {
    Serial.printf("[ESP32] Invalid candidate '%c' — replying FAIL.\n", candidateChar);
    megaSerial.println("FAIL");
    return;
  }

  String candidate = String(candidateChar);

  // Send vote to the Node.js backend and get result
  String result = submitVote(candidate);

  // Reply to Mega: SUCCESS | DUPLICATE | BUSY | FAIL
  megaSerial.println(result);
  Serial.printf("[ESP32] Replied to Mega: %s\n\n", result.c_str());
}


// ════════════════════════════════════════════════════════════
//  VOTE SUBMISSION  (core logic)
// ════════════════════════════════════════════════════════════
String submitVote(const String& candidate) {

  // Ensure WiFi is alive before sending
  if (!ensureWiFiConnected()) {
    Serial.println(F("[ESP32] WiFi unavailable — cannot submit vote."));
    return "BUSY";
  }

  // Build unique vote ID  →  BOOTH-01-AA:BB:CC:DD:EE:FF-000001
  voteCounter++;
  prefs.putUInt("counter", voteCounter);   // persist immediately

  char voteId[72];
  snprintf(voteId, sizeof(voteId), "%s-%s-%06u",
           BOOTH_ID, macAddress.c_str(), voteCounter);

  // ISO 8601 UTC timestamp from synced NTP clock
  String timestamp = buildISO8601Timestamp();

  // Build JSON body — matches backend normalizeVote() schema exactly
  String jsonBody;
  jsonBody.reserve(220);
  jsonBody  = "{";
  jsonBody += "\"boothId\":\""   + String(BOOTH_ID)  + "\",";
  jsonBody += "\"voteId\":\""    + String(voteId)    + "\",";
  jsonBody += "\"candidate\":\"" + candidate         + "\",";
  jsonBody += "\"device\":\"ESP32\",";
  jsonBody += "\"timestamp\":\"" + timestamp         + "\"";
  jsonBody += "}";

  Serial.printf("[ESP32] Posting to %s\n", apiURL.c_str());
  Serial.printf("[ESP32] Payload: %s\n", jsonBody.c_str());

  // ── HTTP POST ──────────────────────────────────────────────
  WiFiClient wifiClient;
  HTTPClient http;
  http.begin(wifiClient, apiURL);          // WiFiClient form — works on all ESP32 core versions
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");
  http.setTimeout(HTTP_TIMEOUT_MS);

  int  httpCode    = http.POST(jsonBody);
  String httpBody  = http.getString();
  http.end();

  Serial.printf("[ESP32] HTTP %d — %s\n", httpCode, httpBody.c_str());

  // ── Map HTTP status to the four tokens the Mega understands ─
  if (httpCode == 201) return "SUCCESS";    // vote stored + blockchain tx
  if (httpCode == 409) return "DUPLICATE";  // contract or in-memory duplicate
  if (httpCode <= 0)   return "BUSY";       // network/connection failure
  return "FAIL";                            // 400, 500, etc.
}


// ════════════════════════════════════════════════════════════
//  WiFi HELPERS
// ════════════════════════════════════════════════════════════
void connectToWiFi() {
  // WiFi.mode(WIFI_STA) already called in setup() before MAC read
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.printf("[ESP32] Connecting to WiFi \"%s\"", WIFI_SSID);
  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
      Serial.println(F("\n[ESP32] WiFi connect timeout — continuing without WiFi."));
      Serial.println(F("[ESP32] Votes will return FAIL until WiFi is available."));
      return;
    }
    delay(500);
    Serial.print(".");
  }

  Serial.printf("\n[ESP32] WiFi connected!\n");
  Serial.printf("[ESP32] IP  : %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("[ESP32] RSSI: %d dBm\n", WiFi.RSSI());
}

bool ensureWiFiConnected() {
  if (WiFi.status() == WL_CONNECTED) return true;

  Serial.println(F("[ESP32] WiFi dropped — attempting reconnect..."));
  WiFi.reconnect();

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > WIFI_RECONNECT_TIMEOUT_MS) return false;
    delay(400);
  }

  Serial.printf("[ESP32] Reconnected. IP: %s\n", WiFi.localIP().toString().c_str());
  return true;
}


// ════════════════════════════════════════════════════════════
//  NTP TIME HELPERS
// ════════════════════════════════════════════════════════════
void syncNTPTime() {
  // UTC offset = 0 (timestamps stored as UTC in the backend)
  configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");

  Serial.print(F("[ESP32] Waiting for NTP time sync"));
  struct tm timeinfo;
  unsigned long start = millis();

  while (!getLocalTime(&timeinfo)) {
    if (millis() - start > NTP_SYNC_TIMEOUT_MS) {
      Serial.println(F("\n[ESP32] NTP sync timed out — timestamps will be epoch-based."));
      return;
    }
    delay(500);
    Serial.print(".");
  }

  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", &timeinfo);
  Serial.printf("\n[ESP32] Time synced: %s\n", buf);
}

// Returns an ISO 8601 UTC string, e.g. "2026-04-26T10:30:00Z"
String buildISO8601Timestamp() {
  struct tm ti;
  if (!getLocalTime(&ti)) {
    // Fallback: use milliseconds since boot as a rough marker
    char fallback[28];
    snprintf(fallback, sizeof(fallback), "1970-01-01T%02lu:%02lu:%02luZ",
             (millis() / 3600000UL) % 24,
             (millis() /   60000UL) % 60,
             (millis() /    1000UL) % 60);
    return String(fallback);
  }
  char buf[28];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &ti);
  return String(buf);
}
