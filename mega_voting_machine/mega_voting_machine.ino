/*
 ============================================================
  BLOCKCHAIN VOTING MACHINE — ARDUINO MEGA 2560
  File   : mega_voting_machine.ino
  Board  : Arduino Mega 2560  (Tools > Board > Arduino Mega)
  Author : Pavan Reddy
 ============================================================

  WHAT THIS CODE DOES
  -------------------
  1. Shows "READY TO VOTE / PRESS A B C D" on a 16×2 I2C LCD.
  2. Waits for one of 4 candidate buttons to be pressed.
  3. Confirms the selection on the LCD and beeps.
  4. Sends   "VOTE:A"  (or B / C / D)  to the ESP32 over Serial1.
  5. Waits up to 15 seconds for a reply:
       SUCCESS   → "VOTE RECORDED / THANK YOU!"
       DUPLICATE → "ALREADY VOTED / ONE VOTE ONLY"
       BUSY      → "NODE IS BUSY  / TRY AGAIN…"
       FAIL      → "SEND FAILED!  / TRY AGAIN"
       (none)    → "NO RESPONSE!  / TRY AGAIN"
  6. Returns to ready state after 3 s.

  HARDWARE CONNECTIONS
  --------------------
  Component            Mega pin
  ─────────────────────────────────────────────────────────
  Button A (to GND)    Digital  2
  Button B (to GND)    Digital  3
  Button C (to GND)    Digital  4
  Button D (to GND)    Digital  5
  Buzzer  +            Digital  6
  Buzzer  –            GND

  I2C LCD 16×2
    VCC                5V
    GND                GND
    SDA                Pin 20 (Mega hardware I2C SDA)
    SCL                Pin 21 (Mega hardware I2C SCL)
    Default address    0x27  (try 0x3F if LCD stays blank)

  ESP32 DevKit V1 ↔ Mega  (Serial1 UART)
    Mega pin 18  (TX1) → ESP32 GPIO16 (RX2)  ← use 1kΩ/2kΩ voltage divider
    Mega pin 19  (RX1) ← ESP32 GPIO17 (TX2)  ← direct (3.3 V is safe)
    Mega GND           — ESP32 GND            (shared ground is mandatory)

  NOTE: Mega TX1 is 5 V logic; ESP32 RX is 3.3 V logic.
        Voltage divider: Mega TX → 1kΩ → junction → ESP32 RX
                                              ↓
                                            2kΩ → GND

  LIBRARIES NEEDED  (Arduino IDE → Sketch → Include Library → Manage Libraries)
    • LiquidCrystal_I2C  by Frank de Brabander
    • Wire  (built-in)
 ============================================================
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ── Pin definitions ──────────────────────────────────────────
#define PIN_BTN_A      2
#define PIN_BTN_B      3
#define PIN_BTN_C      4
#define PIN_BTN_D      5
#define PIN_BUZZER     6

// Serial1 TX=18 → ESP32 GPIO16, Serial1 RX=19 ← ESP32 GPIO17

// ── Timing ───────────────────────────────────────────────────
#define DEBOUNCE_MS       30
#define ESP_TIMEOUT_MS    15000    // how long to wait for ESP32 reply
#define RESULT_DISPLAY_MS  3000   // how long to show outcome on LCD

// ── LCD ───────────────────────────────────────────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2);   // change 0x27 → 0x3F if blank

// ── State machine ─────────────────────────────────────────────
enum VoteState {
  STATE_READY,
  STATE_CONFIRMED,
  STATE_SENDING,
  STATE_RESULT
};

VoteState state      = STATE_READY;
uint32_t  totalVotes = 0;    // session counter (resets on power cycle)


// ════════════════════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════════════════════
void setup() {
  // USB debug port (open Serial Monitor at 115200)
  Serial.begin(115200);

  // Serial1 = hardware UART to ESP32
  Serial1.begin(9600);

  // LCD
  lcd.init();
  lcd.backlight();

  // Buttons — no external resistors needed with INPUT_PULLUP
  pinMode(PIN_BTN_A, INPUT_PULLUP);
  pinMode(PIN_BTN_B, INPUT_PULLUP);
  pinMode(PIN_BTN_C, INPUT_PULLUP);
  pinMode(PIN_BTN_D, INPUT_PULLUP);

  // Buzzer
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  // Welcome
  lcdShow("BLOCKCHAIN VOTE", "  SYSTEM READY  ");
  beepShort();
  delay(1500);

  lcdShowReady();
  Serial.println(F("[Mega] Blockchain Voting Machine online."));
  Serial.println(F("[Mega] Serial commands: RESET | STATUS"));
}


// ════════════════════════════════════════════════════════════
//  MAIN LOOP
// ════════════════════════════════════════════════════════════
void loop() {
  // ── Serial Monitor admin commands ──────────────────────────
  handleSerialAdmin();

  // ── Only scan buttons when in READY state ──────────────────
  if (state != STATE_READY) return;

  if      (digitalRead(PIN_BTN_A) == LOW)  processButtonPress('A', PIN_BTN_A);
  else if (digitalRead(PIN_BTN_B) == LOW)  processButtonPress('B', PIN_BTN_B);
  else if (digitalRead(PIN_BTN_C) == LOW)  processButtonPress('C', PIN_BTN_C);
  else if (digitalRead(PIN_BTN_D) == LOW)  processButtonPress('D', PIN_BTN_D);
}


// ════════════════════════════════════════════════════════════
//  BUTTON PRESS HANDLER
// ════════════════════════════════════════════════════════════
void processButtonPress(char candidate, int pin) {
  delay(DEBOUNCE_MS);
  if (digitalRead(pin) != LOW) return;    // reject bounce

  state = STATE_CONFIRMED;

  // Step 1 — Confirm on LCD
  lcdShow("CANDIDATE  " + String(candidate), "   CONFIRMED!   ");
  beepShort();
  waitButtonRelease(pin);
  delay(1000);

  // Step 2 — Sending state
  state = STATE_SENDING;
  lcdShow("SENDING VOTE    ", "PLEASE WAIT...  ");

  // Step 3 — Send to ESP32
  String cmd = "VOTE:" + String(candidate);
  Serial1.println(cmd);
  Serial.print(F("[Mega] Sent to ESP32: ")); Serial.println(cmd);

  // Step 4 — Wait for reply
  String reply = waitSerial1Response(ESP_TIMEOUT_MS);
  Serial.print(F("[Mega] ESP32 replied: '")); Serial.print(reply); Serial.println("'");

  // Step 5 — Show result
  state = STATE_RESULT;
  showVoteResult(reply, candidate);
  delay(RESULT_DISPLAY_MS);

  // Step 6 — Return to ready
  state = STATE_READY;
  lcdShowReady();
}


// ════════════════════════════════════════════════════════════
//  SHOW VOTE RESULT
// ════════════════════════════════════════════════════════════
void showVoteResult(const String& reply, char candidate) {
  if (reply == "SUCCESS") {
    totalVotes++;
    lcdShow("VOTE RECORDED   ", "  THANK YOU!    ");
    beepSuccess();
    Serial.print(F("[Mega] Vote for ")); Serial.print(candidate);
    Serial.print(F(" recorded. Session total: ")); Serial.println(totalVotes);

  } else if (reply == "DUPLICATE") {
    lcdShow("ALREADY VOTED   ", "ONE VOTE ONLY   ");
    beepError();
    Serial.println(F("[Mega] Duplicate vote blocked by backend."));

  } else if (reply == "BUSY") {
    lcdShow("NODE IS BUSY    ", "TRY AGAIN...    ");
    beepError();
    Serial.println(F("[Mega] ESP32 node busy or WiFi down."));

  } else if (reply == "FAIL") {
    lcdShow("SEND FAILED!    ", "TRY AGAIN       ");
    beepError();
    Serial.println(F("[Mega] Backend returned an error."));

  } else {
    lcdShow("NO RESPONSE!    ", "TRY AGAIN       ");
    beepError();
    Serial.println(F("[Mega] No reply from ESP32 within timeout."));
  }
}


// ════════════════════════════════════════════════════════════
//  ADMIN SERIAL COMMANDS  (USB Serial Monitor)
// ════════════════════════════════════════════════════════════
void handleSerialAdmin() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toUpperCase();

  if (cmd == "RESET") {
    state      = STATE_READY;
    totalVotes = 0;
    lcdShowReady();
    Serial.println(F("[Mega] Reset OK — voting re-enabled, counter cleared."));

  } else if (cmd == "STATUS") {
    Serial.print(F("[Mega] State: ")); Serial.print((int)state);
    Serial.print(F(" | Session votes: ")); Serial.println(totalVotes);

  } else if (cmd.length() > 0) {
    Serial.println(F("[Mega] Unknown command. Use: RESET | STATUS"));
  }
}


// ════════════════════════════════════════════════════════════
//  HELPERS
// ════════════════════════════════════════════════════════════

// LCD shortcuts
void lcdShowReady() {
  lcdShow("READY TO VOTE   ", "PRESS A  B  C  D");
}

void lcdShow(String row0, String row1) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(row0.substring(0, 16));
  lcd.setCursor(0, 1);
  lcd.print(row1.substring(0, 16));
}

// Wait for Serial1 reply, return trimmed string or "" on timeout
String waitSerial1Response(unsigned long timeoutMs) {
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (Serial1.available()) {
      String s = Serial1.readStringUntil('\n');
      s.trim();
      if (s.length() > 0) return s;
    }
  }
  return "";
}

// Wait until button is released (avoid repeat triggers)
void waitButtonRelease(int pin) {
  while (digitalRead(pin) == LOW) delay(10);
  delay(50);
}

// ── Buzzer patterns ──────────────────────────────────────────
void beepShort() {
  tone(PIN_BUZZER, 1000, 80);
  delay(100);
}

void beepSuccess() {
  tone(PIN_BUZZER, 1200, 120); delay(180);
  tone(PIN_BUZZER, 1600, 200); delay(220);
}

void beepError() {
  tone(PIN_BUZZER, 380, 600);
  delay(650);
}
