# Software Backend

This folder contains the software side of the project for:

- Arduino Mega
- ESP32 DevKit V1
- Push-button voting unit
- LCD status display

## What this backend does

- Receives vote data from ESP32
- Validates candidate code
- Blocks duplicate requests using a unique vote ID
- Stores vote count for demo use
- Returns a transaction-like ID
- Provides result and audit APIs
- Includes admin and public result pages

## API endpoints

### `POST /api/vote`

Example request:

```json
{
  "boothId": "BOOTH-01",
  "voteId": "REQ-45120-1",
  "candidate": "A",
  "device": "NODEMCU-01",
  "timestamp": "2026-04-15T12:00:00Z"
}
```

Example success response:

```json
{
  "ok": true,
  "message": "Vote stored successfully",
  "candidate": "A",
  "txId": "0x123abc456def"
}
```

### `GET /api/results`

Returns total votes and per-candidate counts.

### `GET /api/audit`

Returns the full audit log for the demo.

### `POST /api/publish`

Publishes the current results so they appear on the public page.

### `GET /api/published-results`

Returns the last published result snapshot.

### `POST /api/reset`

Resets all demo data.

## How to run

1. Open terminal in `software_backend`
2. Install dependencies:

```powershell
npm install
```

3. Start the backend:

```powershell
npm start
```

4. Update `API_URL` in `esp32_devkit_wifi_bridge.ino` to:

```text
http://YOUR_COMPUTER_IP:3000/api/vote
```

Example:

```text
http://192.168.1.10:3000/api/vote
```

## Pages

- Admin page: `http://localhost:3000/admin.html`
- Public published results page: `http://localhost:3000/published.html`

## Notes

- This backend uses in-memory storage for easy demo use.
- For a full project, you can connect this backend to a database and blockchain transaction sender.
