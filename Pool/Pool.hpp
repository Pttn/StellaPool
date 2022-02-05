// (c) 2022 Pttn (https://riecoin.dev/en/StellaPool)

#ifndef HEADER_Pool_hpp
#define HEADER_Pool_hpp

#include <curl/curl.h>
#include <fcntl.h>
#include <map>
#include <mutex>
#include <netdb.h>
#include <nlohmann/json.hpp>
#include <set>
#include <sys/epoll.h>
#include <thread>

#include "main.hpp"
#include "tools.hpp"

constexpr uint16_t extraNonce1Length(4U);
constexpr uint16_t extraNonce2Length(4U);
struct Miner {
	int fileDescriptor;
	std::string ip;
	uint16_t port;
	uint32_t id;
	std::string extraNonce1;
	std::optional<uint32_t> userId;
	std::string username; // Can be a Riecoin Address
	std::chrono::time_point<std::chrono::steady_clock> latestShareTp; // Disconnection if no share since long
	enum State {UNSUBSCRIBED, SUBSCRIBED, AUTHORIZED} state;
	
	Miner() : latestShareTp(std::chrono::steady_clock::now()), state(UNSUBSCRIBED) {
		static uint32_t id0(0);
		id = id0++;
		std::ostringstream oss;
		oss << std::setfill('0') << std::setw(2U*extraNonce1Length) << std::hex << id;
		extraNonce1 = oss.str();
	}
	std::string str() const {return "Miner "s + std::to_string(id) + " from "s + ip + ":"s + std::to_string(port) + ", fd "s + std::to_string(fileDescriptor);}
};

struct StratumJob {
	uint64_t id;
	uint32_t height;
	int32_t powVersion;
	std::vector<std::vector<uint32_t>> acceptedPatterns;
	BlockHeader bh;
	std::string transactionsHex, default_witness_commitment;
	std::vector<std::array<uint8_t, 32>> txHashes;
	uint64_t coinbasevalue;
	std::vector<uint8_t> coinbase1, coinbase2;
	
	void coinbase1Gen();
	void coinbase2Gen(const std::vector<uint8_t>&);
	std::vector<uint8_t> coinBaseGen(const std::string&, const std::string&);
	std::array<uint8_t, 32> coinbaseTxId(const std::string&, const std::string&) const;
	void merkleRootGen(const std::string&, const std::string&);
};

struct Round {
	uint32_t id;
	uint32_t heightStart;
	std::optional<uint32_t> heightEnd;
	uint64_t timeStart;
	std::optional<uint64_t> timeEnd;
	std::optional<std::string> blockHash;
	uint16_t state;
	std::optional<std::string> finder;
	std::map<std::string, double> scores; // UserId/Address and points
	
	std::string valuesToString() const;
	std::string scoresToJsonString() const;
	void updateInfo(const uint32_t, const uint64_t, const std::string&, const std::string&);
	void updatePoints(const std::string&, const double); // Ensures that every entry has > 0 points (and also avoids dust)
};

struct Share { // Just used for recent statistics, not actual share data
	uint64_t timestamp;
	std::string finder;
	double points;
};

constexpr int maxEvents(16);
constexpr int maxMessageLength(16384);
constexpr uint32_t maxMinersFromSameIp(16U);
constexpr double jobRefreshInterval(30.);
constexpr double banThreshold(-1800.);
constexpr double maxInactivityTime(600.);
constexpr uint64_t movementsDuration(2592000ULL);
constexpr uint64_t powcDuration(604800ULL);
inline const std::string poolSignature("/SP0.9/");
class Pool {
	const std::vector<uint8_t> _scriptPubKey;
	bool _running;
	std::map<int, std::shared_ptr<Miner>> _miners; // Indexed by File Descriptors
	std::chrono::time_point<std::chrono::steady_clock> _latestJobTp;
	std::vector<StratumJob> _currentJobs;
	uint64_t _currentJobId;
	Round _currentRound;
	std::vector<Share> _recentShares; // For recent statistics
	std::set<std::string> _roundOffsets; // For duplicate shares detection
	std::mutex _recentSharesMutex;
	std::mutex _roundUpdateMutex; // Used when a new Round is started
	std::thread _databaseUpdater, _paymentProcessor;
	std::map<std::string, double> _badIps; // Ban scores, each unit below banThreshold corresponds to a 1 s ban
	CURL *_curlMain;
	
	nlohmann::json _sendRequestToWallet(CURL*, const std::string&, const nlohmann::json&) const; // Sends a RPC call to the Riecoin server and returns the response
	uint64_t _checkPoW(const StratumJob&, const std::vector<uint8_t>&); // Computes the Share Prime Count of a Share
	std::string _generateMiningNotify(const bool); // Generates a Stratum.Notify message
	std::pair<std::string, bool> _processMessage(const std::pair<std::shared_ptr<Miner>, std::string>&); // Processes a Stratum Message from a miner, the Bool indicates whether the miner should be disconnected
	void _startNewRound(const uint32_t); // Inserts a new round in the database and updates _currentRound
	void _endCurrentRound(const uint32_t, const std::string&, const std::string&); // Completes the missing entries of the current round and puts the final scores
	std::map<std::string, double> _decodeScores(const std::string&); // Converts scores from Json to Std::Map
	void _updatePoints(const std::string&, const double); // Updates a score entry and adds an entry in the recent shares
	void _punish(const std::string&, const double); // Lowers an IP's ban score
	bool _isBanned(const std::string&); // Check if the IP has a ban score lower than the threshold
	void _updateDatabase(); // Puts the scores into the database, updates confirmed rounds (and generates payout entries), deletes expired entries
	void _processPayments(); // Handles pending withdrawals
	void _fetchJob(); // Fetches work from the Riecoin server using GetBlockTemplate
public:
	inline static Pool* pool;
	Pool() : _scriptPubKey(bech32ToScriptPubKey(configuration.options().poolAddress)), _running(false) {
		assert(database.isReady());
		assert(!pool);
		pool = this;
	}
	void run();
	void stop() {_running = false;}
};

#endif
