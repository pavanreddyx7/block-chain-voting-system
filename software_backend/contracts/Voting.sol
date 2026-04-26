// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

contract Voting {
    struct Candidate {
        string name;
        uint256 voteCount;
    }

    address public owner;
    mapping(string => Candidate) private candidates;
    string[] private candidateCodes;
    mapping(string => bool) public boothHasVoted;

    constructor() {
        owner = msg.sender;

        candidates["A"] = Candidate("Candidate A", 0);
        candidates["B"] = Candidate("Candidate B", 0);
        candidates["C"] = Candidate("Candidate C", 0);
        candidates["D"] = Candidate("Candidate D", 0);

        candidateCodes.push("A");
        candidateCodes.push("B");
        candidateCodes.push("C");
        candidateCodes.push("D");
    }

    modifier onlyOwner() {
        require(msg.sender == owner, "Only owner");
        _;
    }

    function castVote(string memory boothId, string memory candidateCode) external onlyOwner {
        require(!boothHasVoted[boothId], "Duplicate vote");
        require(bytes(candidates[candidateCode].name).length > 0, "Invalid candidate");

        boothHasVoted[boothId] = true;
        candidates[candidateCode].voteCount += 1;
    }

    function getCandidate(string memory candidateCode) external view returns (string memory name, uint256 voteCount) {
        Candidate memory candidate = candidates[candidateCode];
        return (candidate.name, candidate.voteCount);
    }

    function getCandidateCodes() external view returns (string[] memory) {
        return candidateCodes;
    }
}
