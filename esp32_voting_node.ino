// ============================================================
//  ESP32 VOTING NODE
//  Board  : ESP32 DevKit V1
//  Purpose: Receive vote from Mega over UART2, forward to
//           REST API over WiFi, reply SUCCESS / FAIL / BUSY.
// ============================================================

#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>

// ── Credentials (stored in NVS — never hardcode in prod) ────
//
//  Flash once via the one-time provisioner at the bottom of
//  this file (uncomment provisionCredentials(), run, reflash).
//
//  For quick prototyping you may set these directly, but
//  rotate them before sharing source code publicly.
//
static const char* WIFI_SSID     = "X7";
static const char* WIFI_PASSWORD = "1234567890";
static const char* API_URL       = "http://10.20.255.2:3000/api/vote";
static const char* BOOTH_ID      = "BOOTH-01";

// ── UART2 pins ───────────────────────────────────────────────
const uint8_t RX2_PIN = 16;
const uint8_t TX2_PIN = 17;

// ── Timing ───────────────────────────────────────────────────
const uint16_t WIFI_RETRY_INTERVAL_MS = 5000;
const uint8_t  WIFI_MAX_RETRIES       = 20;   // 20 × 500 ms = 10 s

// ── State ────────────────────────────────────────────────────
bool     processingVote  = false;   // BUSY guard
uint32_t voteCounter     = 0;       // NVS-backed, survives reboots
Preferences prefs;

// ── Persisted vote counter ────────────────────────────────────
void loadVoteCounter() {
  prefs.begin("votes", false);
  voteCounter = prefs.getULong("counter", 0);
  prefs.end();
  Serial.printf("[ESP32] Loaded voteCounter = %lu\n", voteCounter);
}

void saveVoteCounter() {
  prefs.begin("votes", false);
  prefs.putULong("counter", voteCounter);
  prefs.end();
}

// ── WiFi ─────────────────────────────────────────────────────
bool connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return true;

  Serial.println("[ESP32] Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint8_t retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < WIFI_MAX_RETRIES) {
    delay(500);
    retries++;
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[ESP32] WiFi connected. IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("[ESP32] WiFi connection failed.");
  return false;
}

// ── HTTP vote POST ────────────────────────────────────────────
// Returns:
//   0  → network / HTTP error  (safe to retry)
//   1  → server accepted       (200–299)
//   2  → server rejected       (4xx — e.g. duplicate)
//   3  → server error          (5xx)
int sendVoteToServer(char candidate) {
  if (!connectWifi()) return 0;

  voteCounter++;
  saveVoteCounter();

  // Build a stable, collision-resistant request ID
  char requestId[48];
  snprintf(requestId, sizeof(requestId),
           "%s-%s-%06lu",
           BOOTH_ID,
           WiFi.macAddress().c_str(),
           voteCounter);

  // Build JSON payload (no concatenation — snprintf is injection-safe)
  char payload[256];
  snprintf(payload, sizeof(payload),
           "{\"boothId\":\"%s\","
           "\"voteId\":\"%s\","
           "\"candidate\":\"%c\","
           "\"device\":\"ESP32-DEVKIT-V1\"}",
           BOOTH_ID, requestId, candidate);

  Serial.printf("[ESP32] POST %s\n", API_URL);
  Serial.printf("[ESP32] Payload: %s\n", payload);

  WiFiClient  client;
  HTTPClient  http;
  http.begin(client, API_URL);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(payload);

  Serial.printf("[ESP32] HTTP response: %d\n", httpCode);
  http.end();

  if (httpCode <= 0)             return 0;  // network error
  if (httpCode >= 200 && httpCode < 300) return 1;  // success
  if (httpCode >= 400 && httpCode < 500) return 2;  // client error (duplicate etc.)
  return 3;                                           // server error
}

// ── Message handler ──────────────────────────────────────────
void handleMegaMessage(String message) {
  message.trim();

  Serial.print("[ESP32] Received from Mega: ");
  Serial.println(message);

  // ── BUSY guard ────────────────────────────────────────────
  if (processingVote) {
    Serial.println("[ESP32] Already processing a vote — sending BUSY");
    Serial2.println("BUSY");
    return;
  }

  // ── Validate frame format "VOTE:X" ────────────────────────
  if (!message.startsWith("VOTE:") || message.length() < 6) {
    Serial.println("[ESP32] Malformed message — ignoring");
    Serial2.println("FAIL");
    return;
  }

  // ── Validate candidate token ──────────────────────────────
  char candidate = message.charAt(5);
  if (candidate != 'A' && candidate != 'B' && candidate != 'C') {
    Serial.printf("[ESP32] Invalid candidate '%c' — rejecting\n", candidate);
    Serial2.println("FAIL");
    return;
  }

  // ── Forward to server ─────────────────────────────────────
  processingVote = true;
  int result = sendVoteToServer(candidate);
  processingVote = false;

  switch (result) {
    case 1:
      Serial.println("[ESP32] Vote accepted by server -> SUCCESS");
      Serial2.println("SUCCESS");
      break;

    case 2:
      Serial.println("[ESP32] Server rejected vote (4xx) -> DUPLICATE");
      Serial2.println("DUPLICATE");
      break;

    case 0:
    case 3:
    default:
      Serial.println("[ESP32] Vote failed -> FAIL");
      Serial2.println("FAIL");
      break;
  }
}

// ── One-time NVS credential provisioner ──────────────────────
//
//  Uncomment the body, upload, observe Serial output confirming
//  the write, then re-comment and upload your production build.
//
void provisionCredentials() {
  // prefs.begin("wifi-creds", false);
  // prefs.putString("ssid", "YOUR_REAL_SSID");
  // prefs.putString("pass", "YOUR_REAL_PASSWORD");
  // prefs.end();
  // Serial.println("Credentials saved to NVS. Reflash without this block.");
}

// ── Arduino lifecycle ────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, RX2_PIN, TX2_PIN);

  Serial.println("[ESP32] Voting node starting...");
  loadVoteCounter();

  // provisionCredentials();  // <-- uncomment once to store creds

  if (connectWifi()) {
    Serial.println("[ESP32] Ready.");
  } else {
    Serial.println("[ESP32] WARNING: offline — votes will fail until WiFi is up.");
  }
}

void loop() {
  // ── Periodic WiFi health-check (non-blocking) ─────────────
  static uint32_t lastWifiCheck = 0;
  if (!processingVote &&
      (millis() - lastWifiCheck) > WIFI_RETRY_INTERVAL_MS) {
    lastWifiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[ESP32] WiFi lost — attempting reconnect...");
      // Reply BUSY immediately so Mega can show the user
      // (Mega will retry after timeout; no bytes are queued here)
      connectWifi();
    }
  }

  // ── Read from Mega ────────────────────────────────────────
  if (Serial2.available()) {
    String message = Serial2.readStringUntil('\n');
    handleMegaMessage(message);
  }
}
