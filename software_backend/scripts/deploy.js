const { ethers } = require("hardhat");
const fs = require("fs");
const path = require("path");

async function main() {
  const [deployer] = await ethers.getSigners();
  console.log("Deploying with account:", deployer.address);

  const Voting = await ethers.getContractFactory("Voting");
  const voting = await Voting.deploy();
  await voting.waitForDeployment();

  const address = await voting.getAddress();
  console.log("Voting contract deployed at:", address);

  // Save address + ABI for server.js to pick up
  const artifact = JSON.parse(
    fs.readFileSync(
      path.join(__dirname, "../artifacts/contracts/Voting.sol/Voting.json"),
      "utf8"
    )
  );

  const deployInfo = {
    address,
    deployerPrivateKey: "0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80", // Hardhat account #0
    abi: artifact.abi
  };

  fs.writeFileSync(
    path.join(__dirname, "../contract-info.json"),
    JSON.stringify(deployInfo, null, 2)
  );

  console.log("contract-info.json written — server.js is ready to use it.");
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
