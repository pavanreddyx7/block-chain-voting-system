# Backend Installation Guide

This guide explains how to install and run the Node.js REST API Server and its underlying simulated blockchain for the voting machine project.

## Requirements
To complete this, you must have [Node.js](https://nodejs.org/) installed on your computer.

---

## 🚀 Quick Start (Mock Mode)
*Best for testing the ESP32 and Arduino boards quickly. The server will run, but simulate blockchain transactions.*

1. Open a terminal (Command Prompt or PowerShell).
2. Navigate to your backend directory:
   ```powershell
   cd "c:\Users\shree\Desktop\selling projects\software_backend"
   ```
3. Install the required Node packages:
   ```powershell
   npm install
   ```
4. Start the server:
   ```powershell
   npm start
   ```
5. You should see:
   `Voting backend running on http://localhost:3000`
   `[blockchain] No contract loaded — using mock txId`

---

## ⛓️ Full Start (Blockchain Local Node Mode)
*Use this mode if you want to deploy actual smart contracts to a local Ethereum simulator using Hardhat.*

You will need **three separate terminal windows** open at the `software_backend` directory.

### Terminal 1: Install & Start Local Blockchain
1. Navigate to the backend directory and install dependencies:
   ```powershell
   cd "c:\Users\shree\Desktop\selling projects\software_backend"
   npm install
   ```
2. Start the Hardhat simulated blockchain:
   ```powershell
   npm run chain
   ```
   *(Keep this terminal open and running. You will see several accounts and private keys generated).*

### Terminal 2: Deploy Smart Contracts
1. Open a new terminal in the same directory.
2. Run the deployment script to push your contracts to the simulated blockchain:
   ```powershell
   npm run deploy
   ```
   *This will generate a `contract-info.json` file inside the directory.*

### Terminal 3: Start the Backend Server
1. Open a final terminal in the same directory.
2. Start the Express server:
   ```powershell
   npm start
   ```
3. You should see:
   `Voting backend running on http://localhost:3000`
   `[blockchain] Connected to Voting contract at <Contract_Address>`

---

## 🔗 Connecting Your Hardware
Once the server is running on `YOUR_COMPUTER_IP:3000`:
1. Find your computer's local IP address (e.g., `192.168.1.5`). You can find this by running `ipconfig` in PowerShell and looking for the IPv4 Address.
2. Open your `esp32_devkit_wifi_bridge.ino` and/or `esp32_voting_node.ino` files in the Arduino IDE.
3. Find the `API_URL` variable:
   ```cpp
   static const char* API_URL = "http://192.168.1.5:3000/api/vote";
   ```
   *(Make sure to use your actual IP address instead of `192.168.1.5` and ensure your ESP32 is connected to the same WiFi network).*

## 🌐 Web Interface
Once running, you can view the local demonstration interfaces by opening these URLs in your browser:
* **Admin Dashboard:** [http://localhost:3000/admin.html](http://localhost:3000/admin.html)
* **Live Results Page:** [http://localhost:3000/published.html](http://localhost:3000/published.html)
