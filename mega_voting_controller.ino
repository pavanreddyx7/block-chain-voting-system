#include <LiquidCrystal.h>

const int BUZZER_PIN = 6;
const int LCD_RS_PIN = 7;
const int LCD_EN_PIN = 8;
const int LCD_D4_PIN = 9;
const int LCD_D5_PIN = 10;
const int LCD_D6_PIN = 11;
const int LCD_D7_PIN = 12;
const int BUTTON_A_PIN = 2;
const int BUTTON_B_PIN = 3;
const int BUTTON_C_PIN = 4;

LiquidCrystal lcd(LCD_RS_PIN, LCD_EN_PIN, LCD_D4_PIN, LCD_D5_PIN, LCD_D6_PIN, LCD_D7_PIN);

char selectedCandidate = '\0';
unsigned long lastButtonTime = 0;
const unsigned long debounceMs = 220;

void beepOnce(int durationMs) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(durationMs);
  digitalWrite(BUZZER_PIN, LOW);
}

void beepSuccess() {
  beepOnce(90);
  delay(80);
  beepOnce(90);
}

void beepError() {
  beepOnce(320);
}

void showMessage(const char* line1, const char* line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

bool buttonPressed(int pin) {
  if (digitalRead(pin) == LOW && millis() - lastButtonTime > debounceMs) {
    lastButtonTime = millis();
    return true;
  }
  return false;
}

void selectCandidate(char code) {
  selectedCandidate = code;
  beepOnce(70);
  Serial.print("Button pressed: ");
  Serial.println(code);

  if (code == 'A') {
    Serial.println("VOTED TO 1");
    showMessage("CANDIDATE A", "SENDING VOTE");
  } else if (code == 'B') {
    Serial.println("VOTED TO 2");
    showMessage("CANDIDATE B", "SENDING VOTE");
  } else if (code == 'C') {
    Serial.println("VOTED TO 3");
    showMessage("CANDIDATE C", "SENDING VOTE");
  }

  sendVote();
}

void sendVote() {
  if (selectedCandidate == '\0') {
    showMessage("NO CANDIDATE", "SELECT FIRST");
    beepError();
    Serial.println("Mega error: no candidate selected");
    return;
  }

  showMessage("SENDING VOTE", "PLEASE WAIT...");
  Serial.print("Mega sent: VOTE:");
  Serial.println(selectedCandidate);

  Serial1.print("VOTE:");
  Serial1.println(selectedCandidate);
}

void processNodeResponse(String response) {
  response.trim();
  Serial.print("Mega received from ESP32: ");
  Serial.println(response);

  if (response == "SUCCESS") {
    showMessage("VOTE STORED", "THANK YOU");
    beepSuccess();
    selectedCandidate = '\0';
  } else if (response == "FAIL") {
    showMessage("SEND FAILED", "TRY AGAIN");
    beepError();
  } else if (response == "BUSY") {
    showMessage("NODE BUSY", "WAIT...");
    beepError();
  }
}

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_A_PIN, INPUT_PULLUP);
  pinMode(BUTTON_B_PIN, INPUT_PULLUP);
  pinMode(BUTTON_C_PIN, INPUT_PULLUP);

  Serial.begin(9600);
  Serial1.begin(9600);

  digitalWrite(BUZZER_PIN, LOW);
  lcd.begin(16, 2);
  showMessage("READY TO VOTE", "SELECT BUTTON");
  beepOnce(100);

  Serial.println("Mega voting controller started.");
  Serial.println("Waiting for button press...");
}

void loop() {
  if (buttonPressed(BUTTON_A_PIN)) {
    selectCandidate('A');
  }

  if (buttonPressed(BUTTON_B_PIN)) {
    selectCandidate('B');
  }

  if (buttonPressed(BUTTON_C_PIN)) {
    selectCandidate('C');
  }

  if (Serial1.available()) {
    String response = Serial1.readStringUntil('\n');
    processNodeResponse(response);
  }
}
