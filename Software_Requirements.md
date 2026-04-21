# Software Requirements Specification (SRS)
## Project: ESP32 Voting Node

### 1. Introduction
**Purpose:** 
The ESP32 Voting Node acts as a secure, network-enabled bridge for an external voting interface (an Arduino Mega). It receives incoming votes via serial communication, attaches a collision-resistant unique identifier, forwards the vote payload to a central REST API over WiFi, and returns the transaction status back to the voting interface.

### 2. System Architecture & Interfaces
- **Microcontroller**: ESP32 (Target: ESP32 DevKit V1)
- **Serial Interface**: UART2 (RX: GPIO 16, TX: GPIO 17) at 9600 baud, 8N1. Used for bidirectional communication with the Arduino Mega.
- **Network Interface**: Built-in 802.11 b/g/n WiFi (STA Mode). Used for communicating with the central server.
- **Data Storage**: ESP32 Non-Volatile Storage (NVS) utility for persisting operational state and credentials across reboots.

### 3. Functional Requirements (FR)

#### 3.1. Vote Reception and Validation
* **FR 1.1**: The system must listen for incoming serial messages terminated by a newline character (`\n`) from the Arduino Mega over UART2.
* **FR 1.2**: The system must validate the structure of the incoming message, enforcing the format `VOTE:<Candidate_Token>`.
* **FR 1.3**: The system must validate the candidate token. Valid tokens are strictly limited to `A`, `B`, and `C`.
* **FR 1.4**: If a message is malformed or contains an invalid candidate token, the system must immediately reject it and transmit a `FAIL` message back over UART2.
* **FR 1.5**: The system must prevent concurrent vote processing. If a vote is actively being processed over the network, incoming messages must be rejected with a `BUSY` response.

#### 3.2. Network and API Communications
* **FR 2.1**: The system must attempt to connect to a provisioned WiFi network upon startup.
* **FR 2.2**: The system must forward valid votes as HTTP POST requests to a preconfigured REST API endpoint.
* **FR 2.3**: The system must attach `application/json` headers and structure the POST payload as a JSON object containing:
  * `boothId`: The identifier for the polling booth.
  * `voteId`: A unique identifier for the specific vote.
  * `candidate`: The validated candidate token.
  * `device`: The hardware identifier (`ESP32-DEVKIT-V1`).
* **FR 2.4**: The system must generate a highly unique `voteId` utilizing the `boothId`, the ESP32's `MAC Address`, and a persistent sequential `Vote Counter` (Format: `<BoothID>-<MAC>-<VoteCounter>`).
* **FR 2.5**: The system must handle and interpret HTTP status codes returned by the REST API, translating them into standard UART2 responses:
  * **HTTP 2xx (Success)** -> Transmit `SUCCESS`.
  * **HTTP 4xx (Client Error/Duplicate)** -> Transmit `DUPLICATE`.
  * **HTTP 5xx or Network Failure** -> Transmit `FAIL`.

#### 3.3. Data Persistence and State Tracking
* **FR 3.1**: The system must maintain an internal `Vote Counter` that increments with every valid vote forwarded to the server.
* **FR 3.2**: The `Vote Counter` must be saved to NVS iteratively to guarantee data immutability against unexpected power loss or resets.
* **FR 3.3**: The system must retrieve the `Vote Counter` from NVS automatically upon device initialization.
* **FR 3.4**: The system must support an isolated, one-time execution mode to securely provision and store WiFi credentials (SSID and Password) in NVS, avoiding hardcoded credentials in the final production firmware.

### 4. Non-Functional Requirements (NFR)

#### 4.1. Reliability
* **NFR 1.1**: The system must implement a non-blocking watchdog for the WiFi connection. It must verify the connection status periodically (every 5 seconds) and silently attempt to reconnect if the link is dropped.
* **NFR 1.2**: In the event of a dropped WiFi connection, the system must fast-fail queued vote transmissions, preventing the system from freezing and ensuring the user interface remains responsive.

#### 4.2. Usability & Interfacing
* **NFR 2.1**: The system must log debugging and operational state information (e.g., loaded counters, connection IP, payload dumps, API responses) to the secondary debugging serial port (`Serial 0` / UART0 at 9600 baud).
* **NFR 2.2**: The primary interfacing communication (UART2) responses must be limited to concise, predictable uppercase states (`SUCCESS`, `FAIL`, `BUSY`, `DUPLICATE`) for easy parsing by the Arduino Mega.

#### 4.3. Security
* **NFR 3.1**: To prevent payload manipulation or SQL-Injection-style attacks at the server level, string operations formatting the JSON payload must use memory-safe concatenation methods (e.g., `snprintf`).
