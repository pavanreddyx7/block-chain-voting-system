// ============================================================
//  MEGA VOTING CONTROLLER
//  Board  : Arduino Mega 2560
//  Purpose: Read 3 candidate buttons, drive LCD + buzzer,
//           send votes to ESP32 over Serial1, receive results.
// ============================================================

#include <LiquidCrystal.h>

// ── Pin assignments ─────────────────────────────────────────
const uint8_t PIN_BUZZER   = 6;
const uint8_t PIN_LCD_RS   = 7;
const uint8_t PIN_LCD_EN   = 8;
const uint8_t PIN_LCD_D4   = 9;
const uint8_t PIN_LCD_D5   = 10;
const uint8_t PIN_LCD_D6   = 11;
const uint8_t PIN_LCD_D7   = 12;
const uint8_t PIN_BTN_A    = 2;
const uint8_t PIN_BTN_B    = 3;
const uint8_t PIN_BTN_C    = 4;

// ── Timing constants ────────────────────────────────────────
const uint16_t DEBOUNCE_MS         = 220;   // per-button lockout
const uint16_t CONFIRM_DISPLAY_MS  = 800;   // "Candidate X" screen hold
const uint32_t RESPONSE_TIMEOUT_MS = 8000;  // max wait for ESP32 reply

// ── State ───────────────────────────────────────────────────
LiquidCrystal lcd(PIN_LCD_RS, PIN_LCD_EN,
                  PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7);

char     pendingCandidate  = '\0';   // candidate selected, not yet acked
bool     votePending       = false;  // true while waiting for ESP32 reply
uint32_t voteSentAt        = 0;      // millis() when vote was transmitted

// Per-button debounce timestamps
uint32_t lastPressMs[3] = {0, 0, 0};

// ── Helpers: buzzer ─────────────────────────────────────────
void beepOnce(uint16_t durationMs) {
  digitalWrite(PIN_BUZZER, HIGH);
  delay(durationMs);
  digitalWrite(PIN_BUZZER, LOW);
}

void beepSelect() {
  beepOnce(70);                  // short blip on button press
}

void beepSuccess() {
  beepOnce(90);
  delay(80);
  beepOnce(90);                  // double chirp = accepted
}

void beepError() {
  beepOnce(320);                 // long tone = rejected / error
}

// ── Helpers: LCD ────────────────────────────────────────────
void showMessage(const char* line1, const char* line2 = "") {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(line1);
  lcd.setCursor(0, 1); lcd.print(line2);
}

// ── Helpers: buttons ────────────────────────────────────────
// Returns true on a LOW edge, after per-button debounce guard.
bool buttonPressed(uint8_t pin, uint8_t idx) {
  if (digitalRead(pin) == LOW &&
      (millis() - lastPressMs[idx]) > DEBOUNCE_MS) {
    lastPressMs[idx] = millis();
    return true;
  }
  return false;
}

// ── Vote flow ────────────────────────────────────────────────
void selectCandidate(char code) {
  // Block new presses while a vote is in-flight
  if (votePending) {
    showMessage("PLEASE WAIT", "VOTE IN FLIGHT");
    beepError();
    delay(1000);
    showMessage("SENDING VOTE", "PLEASE WAIT...");
    return;
  }

  pendingCandidate = code;
  beepSelect();

  // 1. Show confirmation screen briefly
  char line1[17];
  snprintf(line1, sizeof(line1), "CANDIDATE  %c", code);
  showMessage(line1, "CONFIRMED!");
  delay(CONFIRM_DISPLAY_MS);

  // 2. Transmit to ESP32
  showMessage("SENDING VOTE", "PLEASE WAIT...");

  Serial1.print("VOTE:");
  Serial1.println(code);

  Serial.print("[Mega] Vote sent -> VOTE:");
  Serial.println(code);

  votePending = true;
  voteSentAt  = millis();
}

// ── Response handler ─────────────────────────────────────────
void processESP32Response(const String& raw) {
  String response = raw;
  response.trim();

  Serial.print("[Mega] ESP32 says: ");
  Serial.println(response);

  votePending = false;

  if (response == "SUCCESS") {
    showMessage(" VOTE RECORDED", "  THANK YOU!");
    beepSuccess();

  } else if (response == "DUPLICATE") {
    showMessage(" ALREADY VOTED", "ONE VOTE ONLY");
    beepError();

  } else if (response == "BUSY") {
    showMessage(" NODE IS BUSY", " TRY AGAIN...");
    beepError();

  } else if (response == "FAIL") {
    showMessage(" SEND FAILED!", " TRY AGAIN");
    beepError();

  } else {
    Serial.print("[Mega] Unknown response: ");
    Serial.println(response);
    showMessage(" UNKNOWN RESP", " TRY AGAIN");
    beepError();
  }

  pendingCandidate = '\0';
  delay(2000);
  showMessage("READY TO VOTE", " PRESS A B C");
}

// ── Timeout watchdog ─────────────────────────────────────────
void checkResponseTimeout() {
  if (!votePending) return;

  if ((millis() - voteSentAt) > RESPONSE_TIMEOUT_MS) {
    Serial.println("[Mega] Timeout - no reply from ESP32");
    votePending      = false;
    pendingCandidate = '\0';

    showMessage(" NO RESPONSE!", " TRY AGAIN");
    beepError();
    delay(2000);
    showMessage("READY TO VOTE", " PRESS A B C");
  }
}

// ── Arduino lifecycle ────────────────────────────────────────
void setup() {
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  pinMode(PIN_BTN_A, INPUT_PULLUP);
  pinMode(PIN_BTN_B, INPUT_PULLUP);
  pinMode(PIN_BTN_C, INPUT_PULLUP);

  Serial.begin(9600);   // USB debug monitor
  Serial1.begin(9600);  // UART to ESP32 (TX1=18, RX1=19 on Mega)

  lcd.begin(16, 2);
  showMessage("VOTING SYSTEM", "  STARTING...");
  beepOnce(120);
  delay(1200);
  showMessage("READY TO VOTE", " PRESS A B C");

  Serial.println("[Mega] Voting controller ready.");
}

void loop() {
  // ── Button polling (only when no vote is in-flight) ──
  if (!votePending) {
    if (buttonPressed(PIN_BTN_A, 0)) selectCandidate('A');
    if (buttonPressed(PIN_BTN_B, 1)) selectCandidate('B');
    if (buttonPressed(PIN_BTN_C, 2)) selectCandidate('C');
  }

  // ── Read ESP32 reply ──────────────────────────────────
  if (Serial1.available()) {
    String response = Serial1.readStringUntil('\n');
    processESP32Response(response);
  }

  // ── Watchdog: bail out if ESP32 never replies ─────────
  checkResponseTimeout();
}
