# Blockchain Voting Project Guide

This version uses these parts:

- Arduino Uno
- ESP32 DevKit V1
- 16x2 I2C LCD
- 4 Push buttons (one per candidate)
- Buzzer
- 5V power supply

## Project idea

The Arduino Uno is the main booth controller. It reads 4 push buttons for candidate selection and shows messages on the I2C LCD.

The ESP32 DevKit V1 works as a Wi-Fi communication module. It receives the selected candidate from the Uno via Serial2 (UART2) and sends the vote to a backend server. The backend validates the vote, stores an audit log, and writes the final vote record to a blockchain smart contract.

## Working flow

1. LCD shows `READY TO VOTE  PRESS A B C D`.
2. Voter presses one of the 4 candidate buttons.
3. Uno shows the selected candidate on LCD and sends the vote immediately.
4. Uno sends `VOTE:A`, `VOTE:B`, `VOTE:C`, or `VOTE:D` to ESP32 via SoftwareSerial.
5. ESP32 adds a unique `voteId` and sends the vote to the backend API over Wi-Fi.
6. Backend validates the candidate (A/B/C/D) and blocks duplicate request IDs.
7. Backend forwards the valid vote to the blockchain smart contract.
8. ESP32 returns `SUCCESS`, `FAIL`, `DUPLICATE`, or `BUSY` to Uno.
9. Uno shows final result on LCD and beeps the buzzer.

## Connections

### Push buttons to Arduino Uno

- Button A → Pin 2
- Button B → Pin 3
- Button C → Pin 4
- Button D → Pin 5
- Other side of every button → GND

Use `INPUT_PULLUP` in code so you do not need external resistors.

### I2C LCD 16x2 to Arduino Uno

- LCD VCC → 5V
- LCD GND → GND
- LCD SDA → A4
- LCD SCL → A5

Default I2C address is `0x27`. Try `0x3F` if the LCD stays blank.

### ESP32 DevKit V1 to Arduino Uno

- Uno Pin 10 (SW RX) ← ESP32 GPIO17 (TX2)
- Uno Pin 11 (SW TX) → ESP32 GPIO16 (RX2)  [use voltage divider]
- Uno GND → ESP32 GND

Important:

- ESP32 DevKit V1 can be powered via its own USB or from Uno 5V → ESP32 VIN pin.
- Uno TX is 5V logic, ESP32 RX is 3.3V logic — use a voltage divider or logic level shifter on the Uno TX (pin 11) → ESP32 GPIO16 line.
- ESP32 TX (3.3V) → Uno RX (pin 10) works directly without a level shifter.

### Buzzer to Arduino Uno

- Buzzer + → Pin 6
- Buzzer – → GND

### Power

- Power Uno with stable 5V USB adapter
- Power LCD from Uno 5V pin
- Power ESP32 via its own USB or Uno 5V → ESP32 VIN pin
- Keep all grounds common (shared GND rail)

## Suggested backend request

ESP8266 sends a request like this:

```json
{
  "boothId": "BOOTH-01",
  "voteId": "BOOTH-01-AA:BB:CC:DD:EE:FF-000001",
  "candidate": "A",
  "device": "ESP8266",
  "timestamp": "2026-04-15T12:00:00Z"
}
```

Valid candidate values: `"A"`, `"B"`, `"C"`, `"D"`

## LCD messages

- `READY TO VOTE` / `PRESS A B C D`
- `CANDIDATE  A` / `CONFIRMED!`
- `SENDING VOTE` / `PLEASE WAIT...`
- `VOTE RECORDED` / `THANK YOU!`
- `ALREADY VOTED` / `ONE VOTE ONLY`
- `SEND FAILED!` / `TRY AGAIN`
- `NODE IS BUSY` / `TRY AGAIN...`
- `NO RESPONSE!` / `TRY AGAIN`

## Recommended libraries

### Arduino Uno

Install via Arduino IDE Library Manager:

- `Wire` (built-in)
- `LiquidCrystal_I2C` by Frank de Brabander
- `SoftwareSerial` (built-in)

### ESP32 DevKit V1

Select board: **ESP32 Dev Module**

Libraries (built-in with ESP32 board package):

- `WiFi`
- `HTTPClient`
- `Preferences`

## Files in this project

- `mega_code.ino` — Arduino Uno firmware (buttons, I2C LCD, SoftwareSerial to ESP8266)
- `esp32_voting_node.ino` — ESP8266 firmware (WiFi, HTTP POST, EEPROM vote counter)
- `blockchain_voting_full_system_diagram.svg` — Full system block diagram
- `wiring_connections_diagram.svg` — Pin-by-pin wiring reference
- `blockchain_voting_app.html` — Project presentation page
- `software_backend/server.js` — Node.js backend (Express, validates A/B/C/D)
- `software_backend/contracts/Voting.sol` — Solidity smart contract (4 candidates)

## Presentation summary

`Voter presses one of 4 buttons on Arduino Uno. Uno shows the candidate on I2C LCD and sends the vote to ESP32 DevKit V1 via SoftwareSerial. ESP32 connects to Wi-Fi and posts the vote to a Node.js backend. Backend validates the candidate, logs it, and records the vote on an Ethereum smart contract. Result is shown on the LCD with a buzzer beep.`
