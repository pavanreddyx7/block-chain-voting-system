# Blockchain Voting Project Guide

This version removes the fingerprint sensor and uses these parts:

- Arduino Mega
- ESP32 DevKit V1
- 16x2 I2C LCD
- Push buttons
- 5V power supply

## Project idea

The Arduino Mega is the main booth controller. It reads push buttons for candidate selection and shows messages on the LCD.

The ESP32 DevKit V1 works as a Wi-Fi communication module. It receives the selected candidate from the Mega and sends the vote to a backend server. The backend can then store the vote in a database and also write the final vote record to a blockchain smart contract.

## Working flow

1. LCD shows `READY TO VOTE`.
2. Voter presses a candidate button.
3. Mega shows the selected candidate and immediately starts sending the vote.
4. Mega sends `VOTE:A`, `VOTE:B`, or `VOTE:C` to ESP32 through serial communication.
5. ESP32 adds a unique `voteId` and sends the vote to backend API using Wi-Fi.
6. Backend validates the candidate and blocks duplicate request IDs.
7. Backend can forward the valid vote to blockchain.
8. ESP32 returns `SUCCESS` or `FAIL` to Mega.
9. Mega shows final result on LCD.

## Connections

### Push buttons to Arduino Mega

- Button A -> D22
- Button B -> D23
- Button C -> D24
- Other side of every button -> GND

Use `INPUT_PULLUP` in code so you do not need external pull-down resistors.

### I2C LCD to Arduino Mega

- LCD VCC -> 5V
- LCD GND -> GND
- LCD SDA -> Mega pin 20
- LCD SCL -> Mega pin 21

### ESP32 DevKit V1 to Arduino Mega

- Mega TX1 pin 18 -> ESP32 RX2 pin 16
- Mega RX1 pin 19 <- ESP32 TX2 pin 17
- Mega GND -> ESP32 GND

Important:

- ESP32 RX is 3.3V logic.
- Arduino Mega TX is 5V logic.
- Use a voltage divider or logic level shifter between Mega TX1 and ESP32 RX2.

### Power

- Power Mega with stable 5V USB adapter
- Power LCD from Mega 5V
- Power ESP32 through its own USB or a stable 5V USB connection to the board
- Keep all grounds common

## Suggested backend request

ESP32 can send a request like this:

```json
{
  "boothId": "BOOTH-01",
  "candidate": "A",
  "device": "NODEMCU-01",
  "timestamp": "2026-04-15T12:00:00Z"
}
```

## LCD messages

Suggested LCD messages:

- `READY TO VOTE`
- `SELECT CANDIDATE`
- `CANDIDATE A`
- `SENDING VOTE`
- `PLEASE WAIT...`
- `VOTE STORED`
- `SEND FAILED`

## Recommended libraries

### Arduino Mega

- `Wire.h`
- `LiquidCrystal_I2C.h`

### ESP32

- `WiFi.h`
- `HTTPClient.h`

## Files added for this project

- `mega_voting_controller.ino`
- `esp32_devkit_wifi_bridge.ino`
- `blockchain_voting_full_system_diagram.svg`
- `blockchain_voting_app.html`

## Presentation summary

You can explain the project in one line like this:

`Push buttons are used for voting, Arduino Mega handles user interaction on LCD, ESP32 sends the vote through Wi-Fi, backend validates it, and blockchain stores the final record.`
