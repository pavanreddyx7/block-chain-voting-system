const express = require("express");
const cors = require("cors");
const path = require("path");
const { ethers } = require("ethers");
const fs = require("fs");

const app = express();
const PORT = process.env.PORT || 3000;

app.use(cors());
app.use(express.json());
app.use(express.static(path.join(__dirname, "public")));

const allowedCandidates = new Set(["A", "B", "C", "D"]);

// ── Blockchain setup ─────────────────────────────────────────
let votingContract = null;

function loadContract() {
  const infoPath = path.join(__dirname, "contract-info.json");
  if (!fs.existsSync(infoPath)) {
    console.warn("[blockchain] contract-info.json not found — run deploy script first");
    return;
  }

  const { address, deployerPrivateKey, abi } = JSON.parse(fs.readFileSync(infoPath, "utf8"));
  const provider = new ethers.JsonRpcProvider("http://127.0.0.1:8545");
  const signer = new ethers.Wallet(deployerPrivateKey, provider);
  votingContract = new ethers.Contract(address, abi, signer);
  console.log("[blockchain] Connected to Voting contract at", address);
}

loadContract();

// ── In-memory state (audit log / results cache) ──────────────
const state = {
  votes: {
    A: 0,
    B: 0,
    C: 0,
    D: 0
  },
  processedVoteIds: new Set(),
  auditLog: [],
  publishedResults: null
};

function normalizeVote(body) {
  return {
    boothId: String(body.boothId || "").trim(),
    voteId: String(body.voteId || "").trim(),
    candidate: String(body.candidate || "").trim().toUpperCase(),
    device: String(body.device || "").trim(),
    timestamp: body.timestamp ? String(body.timestamp) : new Date().toISOString()
  };
}

function buildResultsSnapshot() {
  const candidates = [
    { code: "A", name: "Candidate A", votes: state.votes.A },
    { code: "B", name: "Candidate B", votes: state.votes.B },
    { code: "C", name: "Candidate C", votes: state.votes.C },
    { code: "D", name: "Candidate D", votes: state.votes.D }
  ];

  const totalVotes = candidates.reduce((sum, item) => sum + item.votes, 0);
  const leadingCandidate = candidates.reduce((leader, item) => {
    if (!leader || item.votes > leader.votes) {
      return item;
    }
    return leader;
  }, null);

  return {
    totalVotes,
    candidates,
    leadingCandidate
  };
}

app.get("/", (req, res) => {
  res.json({
    project: "Blockchain Voting Backend",
    status: "running",
    endpoints: [
      "POST /api/vote",
      "GET /api/results",
      "GET /api/audit",
      "POST /api/publish",
      "GET /api/published-results"
    ]
  });
});

app.post("/api/vote", async (req, res) => {
  const vote = normalizeVote(req.body);

  if (!vote.boothId || !vote.voteId || !vote.candidate || !vote.device) {
    return res.status(400).json({
      ok: false,
      message: "boothId, voteId, candidate, and device are required"
    });
  }

  if (!allowedCandidates.has(vote.candidate)) {
    return res.status(400).json({
      ok: false,
      message: "Invalid candidate code"
    });
  }

  if (state.processedVoteIds.has(vote.voteId)) {
    return res.status(409).json({
      ok: false,
      message: "Duplicate vote blocked for this request"
    });
  }

  // ── Send to blockchain ───────────────────────────────────────
  let txId = null;
  if (votingContract) {
    try {
      const tx = await votingContract.castVote(vote.boothId, vote.candidate);
      const receipt = await tx.wait();
      txId = receipt.hash;
    } catch (err) {
      // Contract reverts (e.g. duplicate booth) become API errors
      const reason = err?.revert?.args?.[0] ?? err?.reason ?? err?.message ?? "Contract error";
      console.error("[blockchain] castVote failed:", reason);
      return res.status(409).json({ ok: false, message: reason });
    }
  } else {
    // Hardhat not running — fall back to fake txId so ESP32 still works
    txId = "0x" + Math.random().toString(16).slice(2) + Date.now().toString(16);
    console.warn("[blockchain] No contract loaded — using mock txId");
  }

  state.votes[vote.candidate] += 1;
  state.processedVoteIds.add(vote.voteId);

  state.auditLog.push({
    boothId: vote.boothId,
    voteId: vote.voteId,
    candidate: vote.candidate,
    device: vote.device,
    timestamp: vote.timestamp,
    status: "RECORDED",
    txId
  });

  return res.status(201).json({
    ok: true,
    message: "Vote stored successfully",
    candidate: vote.candidate,
    txId
  });
});

app.get("/api/results", (req, res) => {
  res.json({
    ok: true,
    ...buildResultsSnapshot()
  });
});

app.get("/api/audit", (req, res) => {
  res.json({
    ok: true,
    entries: state.auditLog
  });
});

app.post("/api/publish", (req, res) => {
  const snapshot = buildResultsSnapshot();
  const publishedAt = new Date().toISOString();
  const note = String(req.body.note || "").trim();

  state.publishedResults = {
    publishedAt,
    note,
    ...snapshot
  };

  res.json({
    ok: true,
    message: "Results published successfully",
    publishedResults: state.publishedResults
  });
});

app.get("/api/published-results", (req, res) => {
  if (!state.publishedResults) {
    return res.status(404).json({
      ok: false,
      message: "Results have not been published yet"
    });
  }

  res.json({
    ok: true,
    ...state.publishedResults
  });
});

app.post("/api/reset", (req, res) => {
  state.votes.A = 0;
  state.votes.B = 0;
  state.votes.C = 0;
  state.votes.D = 0;
  state.processedVoteIds.clear();
  state.auditLog.length = 0;
  state.publishedResults = null;

  res.json({
    ok: true,
    message: "Demo state reset"
  });
});

app.listen(PORT, () => {
  console.log(`Voting backend running on http://localhost:${PORT}`);
});
