// (c) 2022-present Pttn (https://riecoin.xyz/StellaPool/)

#include "Pool.hpp"

void StratumJob::coinbase1Gen() {
	// Version (01000000)
	coinbase1 = {0x01, 0x00, 0x00, 0x00};
	// Input Count (01)
	coinbase1.push_back(1);
	// Input TXID (0000000000000000000000000000000000000000000000000000000000000000)
	for (uint32_t i(0) ; i < 32 ; i++) coinbase1.push_back(0x00);
	// Input VOUT (FFFFFFFF)
	for (uint32_t i(0) ; i < 4 ; i++) coinbase1.push_back(0xFF);
	// ScriptSig Size (Pool Signature Length + Block Height Push Size (1-4 added later) + ExtraNonces Lengths)
	coinbase1.push_back(poolSignature.size() + extraNonce1Length + extraNonce2Length);
	// Block Height (Bip 34)
	if (height < 17) {
		coinbase1.back()++;
		coinbase1.push_back(80 + height);
	}
	else if (height < 128) {
		coinbase1.back() += 2;
		coinbase1.push_back(1);
		coinbase1.push_back(height);
	}
	else if (height < 32768) {
		coinbase1.back() += 3;
		coinbase1.push_back(2);
		coinbase1.push_back(height % 256);
		coinbase1.push_back((height/256) % 256);
	}
	else {
		coinbase1.back() += 4;
		coinbase1.push_back(3);
		coinbase1.push_back(height % 256);
		coinbase1.push_back((height/256) % 256);
		coinbase1.push_back((height/65536) % 256);
	}
}
void StratumJob::coinbase2Gen(const std::vector<uint8_t>& scriptPubKey) {
	const std::vector<uint8_t> dwc(hexStrToV8(default_witness_commitment)); // for SegWit
	// Remaining part of ScriptSig (the Pool Signature; the Block height is in Coinbase1 and the Extra Nonces are not included in Coinbase2)
	coinbase2 = std::vector<uint8_t>(poolSignature.begin(), poolSignature.end());
	// Input Sequence (FFFFFFFF)
	for (uint32_t i(0) ; i < 4 ; i++) coinbase2.push_back(0xFF);
	// Output Count
	coinbase2.push_back(1);
	uint64_t reward(coinbasevalue);
	if (dwc.size() > 0) coinbase2.back()++; // Dummy Output for SegWit
	// Output Value
	for (uint32_t i(0) ; i < 8 ; i++) {
		coinbase2.push_back(reward % 256);
		reward /= 256;
	}
	// Output/ScriptPubKey Length
	coinbase2.push_back(scriptPubKey.size());
	// ScriptPubKey (for the Pool payout address)
	coinbase2.insert(coinbase2.end(), scriptPubKey.begin(), scriptPubKey.end());
	// Dummy output for SegWit
	if (dwc.size() > 0) {
		// No reward
		for (uint32_t i(0) ; i < 8 ; i++) coinbase2.push_back(0);
		// Output Length
		coinbase2.push_back(dwc.size());
		// default_witness_commitment from GetBlockTemplate
		coinbase2.insert(coinbase2.end(), dwc.begin(), dwc.end());
	}
	// Lock Time (00000000)
	for (uint32_t i(0) ; i < 4 ; i++) coinbase2.push_back(0);
}
std::vector<uint8_t> StratumJob::coinBaseGen(const std::string &extraNonce1Str, const std::string &extraNonce2Str) {
	std::vector<uint8_t> coinbase;
	const std::vector<uint8_t> dwc(hexStrToV8(default_witness_commitment)); // for SegWit
	coinbase = coinbase1;
	// Marker (00) and Flag (01) for SegWit
	if (dwc.size() > 0)
		coinbase.insert(coinbase.begin() + 4, {0x00, 0x01});
	// Extra Nonces
	std::vector<uint8_t> extraNonce1(hexStrToV8(extraNonce1Str)), extraNonce2(hexStrToV8(extraNonce2Str));
	coinbase.insert(coinbase.end(), extraNonce1.begin(), extraNonce1.end());
	coinbase.insert(coinbase.end(), extraNonce2.begin(), extraNonce2.end());
	// Coinbase 2
	coinbase.insert(coinbase.end(), coinbase2.begin(), coinbase2.end());
	// Witness of the Coinbase Input for SegWit
	if (dwc.size() > 0) {
		// Number of Witnesses/stack items
		coinbase.insert(coinbase.end() - 4, 1);
		// Witness Length
		coinbase.insert(coinbase.end() - 4, 32);
		// Witness of the Coinbase Input
		for (uint32_t i(0) ; i < 32 ; i++) coinbase.insert(coinbase.end() - 4, 0x00);
	}
	return coinbase;
}
std::array<uint8_t, 32> StratumJob::coinbaseTxId(const std::string &extraNonce1Str, const std::string &extraNonce2Str) const {
	std::vector<uint8_t> coinbase, extraNonce1(hexStrToV8(extraNonce1Str)), extraNonce2(hexStrToV8(extraNonce2Str));
	coinbase.insert(coinbase.end(), coinbase1.begin(), coinbase1.end());
	coinbase.insert(coinbase.end(), extraNonce1.begin(), extraNonce1.end());
	coinbase.insert(coinbase.end(), extraNonce2.begin(), extraNonce2.end());
	coinbase.insert(coinbase.end(), coinbase2.begin(), coinbase2.end());
	return sha256d(coinbase.data(), coinbase.size());
}
void StratumJob::merkleRootGen(const std::string &extraNonce1Str, const std::string &extraNonce2Str) {
	const std::array<uint8_t, 32> cbHash(coinbaseTxId(extraNonce1Str, extraNonce2Str));
	txHashes.insert(txHashes.begin(), cbHash);
	bh.merkleRoot = calculateMerkleRoot(txHashes);
}

std::string Round::valuesToString() const {
	return std::to_string(id)
	     + ", "s + std::to_string(heightStart)
	     + ", "s + (heightEnd.has_value() ? std::to_string(heightEnd.value()) : "NULL"s)
	     + ", "s + std::to_string(timeStart)
	     + ", "s + (timeEnd.has_value() ? std::to_string(timeEnd.value()) : "NULL"s)
	     + ", "s + (blockHash.has_value() ? ("'"s + blockHash.value() + "'"s) : "NULL"s)
	     + ", "s + std::to_string(state)
	     + ", "s + (finder.has_value() ? ("'"s + finder.value() + "'"s) : "NULL"s)
	     + ", '"s + scoresToJsonString() + "'"s;
}
std::string Round::scoresToJsonString() const {
	std::string str("{\"scores\": [");
	for (auto it(scores.begin()) ; it != scores.end() ; it++) {
		str += "{\"miner\": \""s + it->first + "\", \"score\": "s + std::to_string(it->second) + "}"s;
		if (std::next(it) != scores.end())
			str += ", "s;
	}
	str += "]}\n"s;
	return str;
}
void Round::updateInfo(const uint32_t heightEnd0, const uint64_t timeEnd0, const std::string &blockHash0, const std::string &finder0) {
	heightEnd = heightEnd0;
	timeEnd = timeEnd0;
	blockHash = blockHash0;
	finder = finder0;
}
void Round::updatePoints(const std::string &userId, const double points) {
	const auto scoreIt(scores.find(userId));
	if (scoreIt != scores.end()) {
		if (scoreIt->second + points > 0.000001)
			scoreIt->second += points;
		else
			scores.erase(scoreIt);
	}
	else if (points > 0.000001)
		scores[userId] = points;
}

static size_t curlWriteCallback(void *data, size_t size, size_t nmemb, std::string *s) {
	s->append((char*) data, size*nmemb);
	return size*nmemb;
}
nlohmann::json Pool::_sendRequestToWallet(CURL *curl, const std::string &method, const nlohmann::json &params) const {
	static std::atomic<uint64_t> id(0ULL);
	nlohmann::json jsonObj;
	if (curl) {
		std::string s;
		const nlohmann::json request{{"method", method}, {"params", params}, {"id", id++}};
		const std::string requestStr(request.dump());
		std::string credentials(configuration.options().walletUsername + ":" + configuration.options().walletPassword);
		if (configuration.options().walletCookie != "") {
			std::ifstream file(configuration.options().walletCookie, std::ios::in);
			if (!file) {
				std::cerr << "Could not open Cookie '"s << configuration.options().walletCookie << "'!"s << std::endl;
				std::cerr << "Check that the Server is running, that the Cookie does exist at this path, and that this instance of StellaPool can read it."s << std::endl;
				return {};
			}
			std::getline(file, credentials);
		}
		curl_easy_setopt(curl, CURLOPT_URL, ("http://"s + configuration.options().walletHost + ":"s + std::to_string(configuration.options().walletPort) + "/wallet/"s + configuration.options().walletName).c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, requestStr.size());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestStr.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
		curl_easy_setopt(curl, CURLOPT_USERPWD, credentials.c_str());
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30); // Have some margin for transactions using a lot of Inputs, which take a lot of time to be created
		const CURLcode cc(curl_easy_perform(curl));
		if (cc != CURLE_OK)
			ERRORMSG("Curl_easy_perform() failed: " << curl_easy_strerror(cc));
		else {
			try {jsonObj = nlohmann::json::parse(s);}
			catch (nlohmann::json::parse_error &e) {
				if (s.size() == 0)
					std::cout << "Nothing was received from the server!" << std::endl;
				else {
					std::cout << "Received bad JSON object!" << std::endl;
					std::cout << "Server message was: " << s << std::endl;
				}
			}
		}
	}
	return jsonObj;
}

static double getScoreTotal(const std::map<std::string, double> &scores) {
	double scoreTotal(0.);
	for (const auto &scoreEntry : scores)
		scoreTotal += scoreEntry.second;
	return scoreTotal;
}

static uint32_t checkConstellation(mpz_class n, const std::vector<uint32_t> offsets, const uint32_t iterations) {
	uint32_t sharePrimeCount(0);
	for (const auto &offset : offsets) {
		n += offset;
		if (mpz_probab_prime_p(n.get_mpz_t(), iterations) != 0)
			sharePrimeCount++;
		else if (sharePrimeCount < 2)
			return 0;
	}
	return sharePrimeCount;
}
uint64_t Pool::_checkPoW(const StratumJob& job, const std::vector<uint8_t>& nOffsetV8) { // See the Riecoin Core's CheckProofOfWork function, or read https://riecoin.xyz/Guides/PoWImplementation/
	if (job.powVersion != 1) {
		ERRORMSG("Unknown PoW Version " << job.powVersion << ", please upgrade StellaPool!");
		return 0U;
	}
	const uint8_t* rawOffset(nOffsetV8.data()); // [31-30 Primorial Number|29-14 Primorial Factor|13-2 Primorial Offset|1-0 Reserved/Version]
	if ((reinterpret_cast<const uint16_t*>(&rawOffset[0])[0] & 65535) != 2)
		return 0U;
	const uint32_t difficultyIntegerPart(decodeBits(job.bh.bits, job.powVersion));
	if (difficultyIntegerPart < 264U) return 0U;
	const uint32_t trailingZeros(difficultyIntegerPart - 264U);
	mpz_class offsetLimit(1);
	offsetLimit <<= trailingZeros;
	const uint16_t primorialNumber(reinterpret_cast<const uint16_t*>(&rawOffset[30])[0]);
	mpz_class primorial(1), primorialFactor, primorialOffset;
	for (uint16_t i(0U) ; i < primorialNumber ; i++) {
		primorial *= primeTable[i];
		if (primorial > offsetLimit)
			return 0U;
	}
	mpz_import(primorialFactor.get_mpz_t(), 16, -1, sizeof(uint8_t), 0, 0, &rawOffset[14]);
	mpz_import(primorialOffset.get_mpz_t(), 12, -1, sizeof(uint8_t), 0, 0, &rawOffset[2]);
	const mpz_class target(job.bh.target(job.powVersion)),
	                offset(primorial - (target % primorial) + primorialFactor*primorial + primorialOffset);
	if (offset >= offsetLimit)
		return 0U;
	const mpz_class result(target + offset);
	uint32_t highestSharePrimeCount(0);
	for (const auto &pattern : job.acceptedPatterns) {
		const uint32_t sharePrimeCount(checkConstellation(result, pattern, 32));
		if (sharePrimeCount > highestSharePrimeCount)
			highestSharePrimeCount = sharePrimeCount;
	}
	return highestSharePrimeCount;
}

static uint32_t toBEnd32(uint32_t n) { // Converts a uint32_t to Big Endian (ABCDEF01 -> 01EFCDAB in a Little Endian system, do nothing in a Big Endian system)
	const uint8_t *tmp((uint8_t*) &n);
	return (uint32_t) tmp[3] | ((uint32_t) tmp[2]) << 8 | ((uint32_t) tmp[1]) << 16 | ((uint32_t) tmp[0]) << 24;
}
static std::vector<std::array<uint8_t, 32>> calculateMerkleBranches(const std::vector<std::array<uint8_t, 32>>& transactions) {
	std::vector<std::array<uint8_t, 32>> merkleBranches, tmp(transactions);
	if (tmp.size() > 0) {
		while (tmp.size() > 0) {
			merkleBranches.push_back(tmp[0]);
			if (tmp.size() % 2 == 0)
				tmp.push_back(tmp.back());
			std::vector<std::array<uint8_t, 32>> tmp2;
			for (uint64_t j(1) ; j + 1 < tmp.size() ; j += 2) {
				std::vector<uint8_t> toHash(64, 0);
				std::copy(tmp[j].begin(), tmp[j].end(), toHash.begin());
				std::copy(tmp[j + 1].begin(), tmp[j + 1].end(), toHash.begin() + 32);
				tmp2.push_back(sha256d(toHash.data(), 64));
			}
			tmp = tmp2;
		}
	}
	return merkleBranches;
}
std::string Pool::_generateMiningNotify(const bool cleanJobs) {
	StratumJob currentJob;
	if (_currentJobs.size() == 0) {
		ERRORMSG("No available job");
		return ""s;
	}
	currentJob = _currentJobs.back();
	std::ostringstream oss;
	oss << "{\"id\": null, \"method\": \"mining.notify\", \"params\": [\"";
	oss << std::hex << currentJob.id << "\"";
	oss << ", \"";
	std::array<uint8_t, 32> previousblockhashBe;
	for (uint8_t i(0) ; i < 8 ; i++) reinterpret_cast<uint32_t*>(previousblockhashBe.data())[i] = toBEnd32(reinterpret_cast<uint32_t*>(currentJob.bh.previousblockhash.data())[i]);
	oss << v8ToHexStr(a8ToV8(previousblockhashBe)) << "\"";
	oss << ", \"" << v8ToHexStr(currentJob.coinbase1) << "\"";
	oss << ", \"" << v8ToHexStr(currentJob.coinbase2) << "\"";
	oss << ", [";
	std::vector<std::array<uint8_t, 32>> merkleBranches(calculateMerkleBranches(currentJob.txHashes));
	for (uint64_t i(0) ; i < merkleBranches.size() ; i++) {
		oss << "\"" << v8ToHexStr(a8ToV8(merkleBranches[i])) << "\"";
		if (i + 1 < merkleBranches.size())
			oss << ", ";
	}
	oss << "]";
	oss << ", \"" << std::setfill('0') << std::setw(8) << std::hex << currentJob.bh.version << "\"";
	oss << ", \"" << std::setfill('0') << std::setw(8) << std::hex << currentJob.bh.bits << "\"";
	oss << ", \"" << std::setfill('0') << std::setw(8) << std::hex << static_cast<uint32_t>(currentJob.bh.curtime) << "\"";
	oss << ", " << (cleanJobs ? "true" : "false");
	oss << ", " << currentJob.powVersion;
	oss << ", [";
	for (uint64_t i(0) ; i < currentJob.acceptedPatterns.size() ; i++) {
		oss << "[" << formatContainer(currentJob.acceptedPatterns[i]) << "]";
		if (i + 1 !=  currentJob.acceptedPatterns.size())
			oss << ", ";
	}
	oss << "]]}\n";
	return oss.str();
}

std::string stratumResponseStr(const std::string &id, const std::string &result) {
	return "{\"id\": "s + id + ", \"result\": "s + result + ", \"error\": null}\n"s;
}
std::string stratumErrorStr(const std::string &id, const int code, const std::string &message) {
	return "{\"id\": "s + id + ", \"result\": null, \"error\": ["s + std::to_string(code) + ", \"" + message + "\"]}\n"s;
}

std::pair<std::string, bool> Pool::_processMessage(const std::pair<std::shared_ptr<Miner>, std::string>& message) {
	const std::shared_ptr<Miner> miner(message.first);
	if (!miner) {
		ERRORMSG("Processing message from a null miner");
		return {stratumErrorStr("null", 20, "Invalid miner"s), true};
	}
	if (_isBanned(miner->ip)) {
		LOGMSG("Received message from banned miner " << miner->str());
		_punish(miner->ip, -10.);
		return {stratumErrorStr("null", 20, "Banned miner"s), true};
	}
	std::string method;
	std::string messageId("null");
	nlohmann::json jsonMessage;
	try {
		jsonMessage = nlohmann::json::parse(message.second);
	}
	catch (std::exception &e) {
		LOGMSG("Received invalid JSON message from " << miner->str());
		_punish(miner->ip, -10.);
		return {stratumErrorStr("null", 20, "Invalid JSON message"s), true};
	}
	try {
		uint64_t messageIdUInt(jsonMessage["id"]);
		messageId = std::to_string(messageIdUInt);
	}
	catch (...) {
		messageId = "null";
	}
	try {
		method = jsonMessage["method"];
	}
	catch (std::exception &e) {
		LOGMSG("Received message with missing or invalid method from " << miner->str());
		_punish(miner->ip, -10.);
		return {stratumErrorStr(messageId, 20, "Missing or invalid method"s), true};
	}
	if (method == "mining.subscribe") {
		miner->state = Miner::State::SUBSCRIBED;
		LOGMSG("Subscribed " << miner->str());
		return {stratumResponseStr(messageId, "[[[\"mining.notify\", \""s + miner->extraNonce1 + "\"]],\""s + miner->extraNonce1 + "\", "s + std::to_string(extraNonce2Length) + "]"s), false};
	}
	else if (method == "mining.authorize") {
		if (miner->state != Miner::State::SUBSCRIBED) {
			LOGMSG("Not authorizing unsubscribed " << miner->str());
			_punish(miner->ip, -10.);
			return {stratumErrorStr(messageId, 25, "Not subscribed"s), true};
		}
		else {
			std::string username;
			try {
				username = jsonMessage["params"][0];
			}
			catch (std::exception &e) {
				LOGMSG("Not authorizing (missing or invalid username) " << miner->str());
				_punish(miner->ip, -2.);
				return {stratumErrorStr(messageId, 20, "Missing or invalid username"s), true};
			}
			if (username.size() == 0) {
				LOGMSG("Not authorizing (empty username) " << miner->str());
				_punish(miner->ip, -2.);
				return {stratumErrorStr(messageId, 20, "Empty username"s), true};
			}
			if (username.size() <= 20 && std::all_of(username.begin(), username.end(), [](unsigned char c){return std::isalnum(c);})) {
				try {
					std::shared_ptr<sql::ResultSet> sqlResult(database.executeQuery("SELECT id FROM Users WHERE username = '"s + username + "';"s));
					if (sqlResult->next()) {
						const uint64_t userId(sqlResult->getUInt64("id"));
						std::string password;
						try {
							password = jsonMessage["params"][1];
						}
						catch (std::exception &e) {
							LOGMSG("Not authorizing (missing or invalid password) " << miner->str());
							_punish(miner->ip, -2.);
							return {stratumErrorStr(messageId, 20, "Missing or invalid password"s), true};
						}
						if (password.size() > 128 || password.size() % 2 != 0) {
							LOGMSG("Not authorizing (invalid token size) " << miner->str());
							_punish(miner->ip, -2.);
							return {stratumErrorStr(messageId, 20, "Invalid token size"s), true};
						}
						if (!isHexStr(password)) {
							LOGMSG("Not authorizing (token is not a hex string) " << miner->str());
							_punish(miner->ip, -2.);
							return {stratumErrorStr(messageId, 20, "Token is not a hex string"s), true};
						}
						const std::vector<uint8_t> tokenV8(hexStrToV8(password));
						std::shared_ptr<sql::ResultSet> sqlResult2(database.executeQuery("SELECT userId FROM Tokens WHERE hash = 0x"s + v8ToHexStr(a8ToV8(sha256(tokenV8.data(), tokenV8.size())))));
						if (sqlResult2->next()) {
							const uint64_t tokenUserId(sqlResult2->getUInt64("userId"));
							if (userId != tokenUserId) {
								LOGMSG("Not authorizing (username and token do not match) " << miner->str());
								_punish(miner->ip, -2.);
								return {stratumErrorStr(messageId, 20, "Username and token do not match"s), true};
							}
							else {
								miner->userId = userId;
								miner->username = username;
								miner->state = Miner::State::AUTHORIZED;
								LOGMSG("Authorized " << miner->str() << "; user " << miner->username << ", Id " << userId);
								return {stratumResponseStr(messageId, "true") + _generateMiningNotify(true) + "{\"id\": null, \"method\": \"client.show_message\", \"params\": [\"Hello "s + miner->username + ", Happy Mining!\"]}\n"s, false};
							}
						}
						else {
							LOGMSG("Not authorizing (token not found) " << miner->str());
							_punish(miner->ip, -2.);
							return {stratumErrorStr(messageId, 20, "Token not found"s), true};
						}
					}
					else {
						LOGMSG("Not authorizing (user not found) " << miner->str());
						_punish(miner->ip, -2.);
						return {stratumErrorStr(messageId, 20, "User not found"s), true};
					}
				}
				catch (std::exception &e) {
					ERRORMSG("Something went wrong while looking for user in database for " << miner->str() << " - " << e.what());
					return {stratumErrorStr(messageId, 20, "Internal error :|"s), true};
				}
			}
			nlohmann::json getaddressinfoResponse;
			try {
				getaddressinfoResponse = _sendRequestToWallet(_curlMain, "getaddressinfo", {username});
			}
			catch (std::exception &e) {
				ERRORMSG("Something went wrong while checking the Riecoin address of " << miner->str() << " - " << e.what());
				return {stratumErrorStr(messageId, 20, "Internal error :|"s), true};
			}
			std::string addressFromGetaddressinfo;
			bool isWitnessAddress(false);
			try {
				addressFromGetaddressinfo = getaddressinfoResponse["result"]["address"];
				isWitnessAddress = getaddressinfoResponse["result"]["iswitness"];
			}
			catch (...) {
				LOGMSG("Not authorizing (neither a registered username nor a valid Riecoin address) " << miner->str());
				_punish(miner->ip, -2.);
				return {stratumErrorStr(messageId, 20, "Neither a registered username nor a valid Riecoin address"s), true};
			}
			if (username != addressFromGetaddressinfo || !isWitnessAddress) {
				LOGMSG("Not authorizing (not a valid Bech32 Address) " << miner->str());
				_punish(miner->ip, -2.);
				return {stratumErrorStr(messageId, 20, "Not a valid Bech32 address"s), true};
			}
			miner->username = username;
			miner->state = Miner::State::AUTHORIZED;
			LOGMSG("Authorized " << miner->str() << "; anonymous miner");
			return {stratumResponseStr(messageId, "true"s) + _generateMiningNotify(true), false};
		}
	}
	else if (method == "mining.submit") {
		if (miner->state != Miner::State::AUTHORIZED) {
			LOGMSG("Ignoring share from unauthorized " << miner->str());
			_punish(miner->ip, -5.);
			return {stratumErrorStr(messageId, 24, "Unauthorized worker"s), true};
		}
		const std::string userId(miner->userId.has_value() ? std::to_string(miner->userId.value()) : miner->username);
		std::string username, id, extranonce2, nTime, nOffset;
		try {
			username = jsonMessage["params"][0];
			id = jsonMessage["params"][1];
			extranonce2 = jsonMessage["params"][2];
			nTime = jsonMessage["params"][3];
			nOffset = jsonMessage["params"][4];
		}
		catch (std::exception &e) {
			LOGMSG("Received invalid submission (invalid parameters) from " << miner->str());
			_updatePoints(userId, -4.);
			_punish(miner->ip, -5.);
			return {stratumErrorStr(messageId, 20, "Invalid params"s), true};
		}
		if (username != miner->username) {
			LOGMSG("Received invalid submission (wrong username) from " << miner->str());
			_updatePoints(userId, -4.);
			_punish(miner->ip, -5.);
			return {stratumErrorStr(messageId, 20, "Wrong username"s), true};
		}
		if (!isHexStrOfSize(extranonce2, 2U*extraNonce2Length)) {
			LOGMSG("Received invalid submission (invalid Extra Nonce 2) from " << miner->str());
			_updatePoints(userId, -4.);
			_punish(miner->ip, -5.);
			return {stratumErrorStr(messageId, 20, "Invalid Extra Nonce 2 (must be "s + std::to_string(2U*extraNonce2Length) + " hex digits"s), true};
		}
		if (!isHexStr(nTime)) {
			LOGMSG("Received invalid submission (invalid nTime) from " << miner->str());
			_updatePoints(userId, -4.);
			_punish(miner->ip, -5.);
			return {stratumErrorStr(messageId, 20, "Invalid nTime (must be a hex str)"s), true};
		}
		if (!isHexStrOfSize(nOffset, 64)) {
			LOGMSG("Received invalid submission (invalid nOffset) from " << miner->str());
			_updatePoints(userId, -4.);
			_punish(miner->ip, -5.);
			return {stratumErrorStr(messageId, 20, "Invalid nTime (Invalid nOffset (must be 64 hex digits)"s), true};
		}
		if (_roundOffsets.find(nOffset) != _roundOffsets.end()) {
			LOGMSG("Received invalid submission (duplicate share) from " << miner->str());
			_updatePoints(userId, -8.);
			_punish(miner->ip, -120.);
			return {stratumErrorStr(messageId, 22, "Duplicate share"s), true};
		}
		uint64_t jobId;
		try {
			jobId = std::stoll(id, nullptr, 16);
		}
		catch (const std::exception &e) { // This should never happen as a Hex Check was previously done
			ERRORMSG("SToLl failed for some reason while decoding the Job Id - " << e.what());
			ERRORMSG("Submission was " << message.second);
			_punish(miner->ip, -5.);
			return {stratumErrorStr(messageId, 20, "Internal error :|"s), true};
		}
		bool jobFound(false);
		StratumJob shareJob;
		for (const auto &job : _currentJobs) {
			if (job.id == jobId) {
				jobFound = true;
				shareJob = job;
				break;
			}
		}
		if (!jobFound) {
			LOGMSG("Received invalid submission (job not found) from " << miner->str());
			_updatePoints(userId, -2.);
			_punish(miner->ip, -2.);
			return {stratumErrorStr(messageId, 21, "Job not found"s), false};
		}
		uint64_t shareTimestamp;
		try {
			shareTimestamp = std::stoll(nTime, nullptr, 16);
		}
		catch (const std::exception &e) { // This should never happen as a Hex Check was previously done
			ERRORMSG("SToLl failed for some reason while decoding the Timestamp - " << e.what());
			ERRORMSG("Submission was " << message.second);
			_punish(miner->ip, -5.);
			return {stratumErrorStr(messageId, 20, "Internal error :|"s), true};
		}
		if (shareTimestamp < shareJob.bh.curtime) {
			LOGMSG("Received invalid submission (timestamp too early) from " << miner->str());
			_updatePoints(userId, -4.);
			_punish(miner->ip, -5.);
			return {stratumErrorStr(messageId, 20, "Timestamp too early (please check your system clock)"s), true};
		}
		else if (shareTimestamp > nowU64() + 5) {
			LOGMSG("Received invalid submission (timestamp too late) from " << miner->str());
			_updatePoints(userId, -4.);
			_punish(miner->ip, -5.);
			return {stratumErrorStr(messageId, 20, "Timestamp too late (please check your system clock))"s), true};
		}
		const std::vector<uint8_t> nOffsetV8(reverse(hexStrToV8(nOffset)));
		if (*reinterpret_cast<const uint16_t*>(&nOffsetV8.data()[0]) != 2) {
			LOGMSG("Received invalid submission (invalid PoW Version) from " << miner->str());
			_updatePoints(userId, -4.);
			_punish(miner->ip, -5.);
			return {stratumErrorStr(messageId, 20, "Invalid PoW Version"s), true};
		}
		shareJob.merkleRootGen(miner->extraNonce1, extranonce2);
		const uint64_t sharePrimeCount(_checkPoW(shareJob, nOffsetV8)), sharePrimeCountMin(std::max(4ULL, shareJob.acceptedPatterns[0].size() - 2ULL));
		if (sharePrimeCount < sharePrimeCountMin) {
			LOGMSG("Received invalid submission (too low Share Prime Count) from " << miner->str());
			_updatePoints(userId, -8.);
			_punish(miner->ip, -120.);
			return {stratumErrorStr(messageId, 23, "Received a "s + std::to_string(sharePrimeCount) + "-share, "s + std::to_string(sharePrimeCountMin) + "-shares or better expected"s), true};
		}
		_roundOffsets.insert(nOffset);
		_updatePoints(userId, 1.);
		miner->latestShareTp = std::chrono::steady_clock::now();
		if (sharePrimeCount > 5ULL) {
			LOGMSG("Accepted " << sharePrimeCount << "-share from " << miner->str() << " (" << miner->username << ")");
			_punish(miner->ip, 60.);
		}
		if (sharePrimeCount >= shareJob.acceptedPatterns[0].size()) {
			_updatePoints(userId, 64.);
			LOGMSG("Submitting block with " << shareJob.txHashes.size() << " transaction(s) (including coinbase)...");
			shareJob.transactionsHex = v8ToHexStr(shareJob.coinBaseGen(miner->extraNonce1, extranonce2)) + shareJob.transactionsHex;
			BlockHeader bh(shareJob.bh);
			bh.nOffset = v8ToA8(nOffsetV8);
			std::ostringstream oss;
			oss << v8ToHexStr(bh.toV8());
			// Using the Variable Length Integer format; having more than 65535 transactions is currently impossible
			if (shareJob.txHashes.size() < 0xFD)
				oss << std::setfill('0') << std::setw(2) << std::hex << shareJob.txHashes.size();
			else
				oss << "fd" << std::setfill('0') << std::setw(2) << std::hex << shareJob.txHashes.size() % 256 << std::setw(2) << shareJob.txHashes.size()/256;
			oss << shareJob.transactionsHex;
			nlohmann::json submitblockResponse, submitblockResponseResult, submitblockResponseError;
			try {
				submitblockResponse = _sendRequestToWallet(_curlMain, "submitblock", {oss.str()});
				submitblockResponseResult = submitblockResponse["result"];
				submitblockResponseError = submitblockResponse["error"];
			}
			catch (std::exception &e) {
				ERRORMSG("Could not submit block: " << e.what() << std::endl);
				return {stratumErrorStr(messageId, 20, "Internal error :|"s), true};
			}
			if (submitblockResponseResult == nullptr && submitblockResponseError == nullptr) {
				std::string blockHash;
				try {
					blockHash = _sendRequestToWallet(_curlMain, "getblockhash", {shareJob.height})["result"];
					const std::lock_guard lock(_roundUpdateMutex);
					_endCurrentRound(shareJob.height, blockHash, userId);
					_startNewRound(shareJob.height + 1);
				}
				catch (std::exception &e) {
					ERRORMSG("Could not start a new round: " << e.what() << std::endl);
					return {stratumErrorStr(messageId, 20, "Internal error :|"s), true};
				}
				LOGMSG("Submission accepted :D ! Blockhash: " << blockHash);
			}
			else {
				LOGMSG("Submission rejected :| ! Received: " << submitblockResponse.dump());
				return {stratumErrorStr(messageId, 20, "Your block was rejected by the Riecoin Network :|, really sorry"s), false};
			}
		}
		return {stratumResponseStr(messageId, "true"), false};
	}
	else {
		LOGMSG("Received invalid request (unsupported method) from " << miner->str());
		_punish(miner->ip, -10.);
		return {stratumErrorStr(messageId, 20, "Unsupported method "s + method), true};
	}
}

void Pool::_startNewRound(const uint32_t heightStart) {
	try {
		Round r{.id = _currentRound.id + 1, .heightStart = heightStart, .heightEnd = {}, .timeStart = nowU64(), .timeEnd = {}, .blockHash = {}, .state = 0U, .finder = {}, .scores = {}};
		database.execute("INSERT INTO Rounds VALUES ("s + r.valuesToString() + ");"s);
		_currentRound = r;
		_roundOffsets.clear();
	}
	catch (sql::SQLException &e) {
		throw e;
	}
}
void Pool::_endCurrentRound(const uint32_t heightEnd, const std::string &blockHash, const std::string &finder) {
	try {
		_currentRound.updateInfo(heightEnd, nowU64(), blockHash, finder);
		database.execute("UPDATE Rounds SET heightEnd = "s + std::to_string(_currentRound.heightEnd.value()) + ", timeEnd = "s + std::to_string(_currentRound.timeEnd.value()) + ", blockHash = '"s + _currentRound.blockHash.value() + "', finder = '"s + _currentRound.finder.value() + "', scores = '"s + _currentRound.scoresToJsonString() + "' WHERE id = "s + std::to_string(_currentRound.id));
	}
	catch (sql::SQLException &e) {
		throw e;
	}
}
std::map<std::string, double> Pool::_decodeScores(const std::string &scoresJsonString) {
	try {
		const nlohmann::json jsonData(nlohmann::json::parse(scoresJsonString));
		std::map<std::string, double> shareData;
		for (const auto &minerShareData : jsonData["scores"])
			shareData[minerShareData["miner"]] = minerShareData["score"];
		return shareData;
	}
	catch (std::exception &e) {
		ERRORMSG("Could not extract scores: " << e.what());
		return {};
	}
}

void Pool::_updatePoints(const std::string &userId, const double points) {
	_currentRound.updatePoints(userId, points);
	const std::lock_guard lock(_recentSharesMutex);
	_recentShares.push_back(Share{nowU64(), userId, points});
}
void Pool::_punish(const std::string &ip, const double points) {
	if (points < 0.) {
		auto badIpIt(_badIps.find(ip));
		if (badIpIt != _badIps.end())
			badIpIt->second += points;
		else
			_badIps[ip] = points;
		if (_badIps[ip] <= banThreshold && _badIps[ip] + points > banThreshold) {
			LOGMSG(ip << " reached the ban threshold and has been banned");
			_badIps[ip] -= 1800.;
			try {
				database.execute("INSERT INTO Events (time, ip, type) VALUES ("s + std::to_string(nowU64()) + ", '"s + ip + "', 'banned miner');"s);
			}
			catch (sql::SQLException &e) {
				SQLERRORMSG("Could not log the ban");
			}
		}
	}
}
bool Pool::_isBanned(const std::string &ip) {
	const auto badIpIt(_badIps.find(ip));
	if (badIpIt == _badIps.end()) return false;
	else return badIpIt->second <= banThreshold;
}

void Pool::_updateDatabase() {
	CURL *curlDatabaseUpdater(curl_easy_init());
	std::chrono::time_point<std::chrono::steady_clock> latestDbUpdateTp(std::chrono::steady_clock::now());
	while (_running) {
		LOGMSG("Updating database...");
		try {
			const std::lock_guard roundUpdateLock(_roundUpdateMutex);
			database.execute("UPDATE Rounds SET scores = '"s + _currentRound.scoresToJsonString() + "' WHERE id = "s + std::to_string(_currentRound.id) + ";"s);
			_roundUpdateMutex.unlock();
			const std::lock_guard recentSharesLock(_recentSharesMutex);
			std::string queries("START TRANSACTION; DELETE FROM SharesRecent WHERE time < "s + std::to_string(nowU64() - 3600) + ";"s);
			if (_recentShares.size() > 0) {
				queries += "INSERT INTO SharesRecent (time, finder, points) VALUES "s;
				for (uint64_t i(0) ; i < _recentShares.size() ; i++) {
					queries += "("s + std::to_string(_recentShares[i].timestamp) + ", '"s + _recentShares[i].finder + "', "s + std::to_string(_recentShares[i].points) + ")"s;
					if (i + 1 < _recentShares.size()) queries += ","s;
				}
				queries += ";"s;
			}
			queries += "COMMIT;"s;
			database.execute(queries);
			_recentShares.clear();
		}
		catch (sql::SQLException &e) {
			SQLERRORMSG("Could not update the scores or recent shares, can be ignored unless it occurs frequently");
		}
		
		std::shared_ptr<sql::ResultSet> sqlResult;
		try {
			sqlResult = database.executeQuery("SELECT id, blockHash, scores FROM Rounds WHERE blockHash IS NOT NULL AND state = 0 ORDER BY id ASC;");
			std::string queries("START TRANSACTION;"s);
			while (sqlResult->next()) {
				const std::string blockHash(sqlResult->getString("blockHash"));
				uint32_t roundId(sqlResult->getUInt("id"));
				int32_t confirmations(0);
				double reward(0.);
				try {
					const nlohmann::json block(_sendRequestToWallet(curlDatabaseUpdater, "getblock", {blockHash})["result"]);
					confirmations = block["confirmations"];
					const std::string coinbaseTxId(block["tx"][0]);
					const nlohmann::json coinbase(_sendRequestToWallet(curlDatabaseUpdater, "getrawtransaction", {coinbaseTxId, true})["result"]);
					reward = coinbase["vout"][0]["value"].get<double>();
				}
				catch (const std::exception &e) {
					ERRORMSG("Could not get number of confirmations for Block " << blockHash << ", it will be attempted again, no manual intervention needed unless it keeps failing - " << e.what());
					continue;
				}
				if (confirmations < 0) {
					LOGMSG("Block found in Round " << roundId << " was orphaned :|");
					try {
						database.execute("UPDATE Rounds SET state = 2 WHERE id = "s + std::to_string(roundId));
					}
					catch (sql::SQLException &e) {
						SQLERRORMSG("Could not update the state of the orphaned block, it will be attempted again, no manual intervention needed unless it keeps failing");
					}
					continue;
				}
				else if (confirmations >= configuration.options().poolRequiredConfirmations) {
					const double rewardNet((1. - configuration.options().poolFee)*reward);
					LOGMSG("Block found in Round " << roundId << " now has enough confirmations, distributing " << amountStr(rewardNet) << " RIC to miners...");
					const std::map<std::string, double> scores(_decodeScores(sqlResult->getString("scores")));
					const double totalScore(getScoreTotal(scores));
					for (const auto &scoreEntry : scores) {
						std::optional<uint64_t> userId;
						bool registeredUser;
						try {
							userId = std::stoll(scoreEntry.first);
							registeredUser = true;
						}
						catch (...) {
							registeredUser = false;
						}
						const double rewardNetMiner((scoreEntry.second*rewardNet)/totalScore);
						if (registeredUser) {
							try {
								std::shared_ptr<sql::ResultSet> sqlResult2(database.executeQuery("SELECT username FROM Users WHERE id = '"s + std::to_string(userId.value()) + "';"s));
								if (sqlResult2->next()) {
									const std::string username(sqlResult2->getString("username"));
									const double powCredits(scoreEntry.second/totalScore);
									queries += "INSERT INTO RiecoinMovements (time, value, userId, comment) VALUES ("s + std::to_string(nowU64()) + ", "s + std::to_string(rewardNetMiner) + ", "s + std::to_string(userId.value()) + ", 'Payout for Round "s + std::to_string(roundId) + "');"s;
									queries += "INSERT INTO PoWcMovements (time, value, userId, comment) VALUES ("s + std::to_string(nowU64()) + ", "s + std::to_string(powCredits) + ", "s + std::to_string(userId.value()) + ", 'PoW Credits for Round "s + std::to_string(roundId) + "');"s;
									queries += "UPDATE Users SET balance = balance + "s + std::to_string(rewardNetMiner) + " WHERE id = "s + std::to_string(userId.value()) + ";"s;
									queries += "INSERT INTO PoWc (expiration, amount, userId) VALUES ("s + std::to_string(nowU64() + powcDuration) + ", "s + std::to_string(powCredits) + ", "s + std::to_string(userId.value()) + ");"s;
									LOGMSG("\t" << amountStr(rewardNetMiner) << " RIC and " << powCredits << " PoWc to " << username << " (User " << userId.value() << ")");
								}
								else {
									LOGMSG("User " << userId.value() << " was deleted, discarding RIC and PoWc.");
								}
							}
							catch (sql::SQLException &e) {
								SQLERRORMSG("Could not check if User " << userId.value() << " exists, you will have to update the balances manually");
							}
						}
						else {
							const std::string address(scoreEntry.first);
							queries += "INSERT INTO RiecoinMovements (time, value, address, comment) VALUES ("s + std::to_string(nowU64()) + ", "s + std::to_string(rewardNetMiner) + ", '"s + address + "', 'Payout for Round "s + std::to_string(roundId) + "');"s;
							LOGMSG("\t" << amountStr(rewardNetMiner) << " RIC to " << scoreEntry.first);
						}
					}
					queries += "UPDATE Rounds SET state = 1 WHERE id = "s + std::to_string(roundId) + ";"s;
				}
			}
			queries += "COMMIT;"s;
			database.execute(queries);
		}
		catch (sql::SQLException &e) {
			SQLERRORMSG("Something went wrong while generating the payouts and PoW Credits, it will be attempted again, no manual intervention needed unless it keeps failing");
		}
		
		try {
			database.execute("DELETE FROM Sessions WHERE expiration <= "s + std::to_string(nowU64()) + ";"s);
			database.execute("DELETE FROM Tokens WHERE expiration <= "s + std::to_string(nowU64()) + ";"s);
			database.execute("DELETE FROM PoWc WHERE expiration <= "s + std::to_string(nowU64()) + ";"s);
		}
		catch (sql::SQLException &e) {
			SQLERRORMSG("Could not delete expired sessions, tokens or PoWc, it will be attempted again, no manual intervention needed unless it keeps failing");
		}
		
		try {
			const uint64_t expirationTime(nowU64() - movementsDuration);
			database.execute("DELETE FROM RiecoinMovements WHERE time <= "s + std::to_string(expirationTime) + ";"s);
			database.execute("DELETE FROM PoWcMovements WHERE time <= "s + std::to_string(expirationTime) + ";"s);
			database.execute("DELETE FROM Withdrawals WHERE time <= "s + std::to_string(expirationTime) + ";"s);
		}
		catch (sql::SQLException &e) {
			SQLERRORMSG("Could not delete old RIC/PoWc movements or withdrawals, it will be attempted again, no manual intervention needed unless it keeps failing");
		}
		
		for (auto it(_badIps.begin()) ; it != _badIps.end() ; it++) {
			const bool wasBanned(it->second <= banThreshold);
			it->second += configuration.options().databaseUpdateInterval;
			if (wasBanned && it->second > banThreshold)
				LOGMSG(it->first << " is no longer banned");
		}
		std::erase_if(_badIps, [](const auto& entry) {return entry.second >= 0.;});
		latestDbUpdateTp = std::chrono::steady_clock::now();
		while (timeSince(latestDbUpdateTp) < configuration.options().databaseUpdateInterval && _running) // Simple way to quit with for example Ctrl + C without having to wait for the full databaseUpdateInterval
			std::this_thread::sleep_for(std::chrono::duration<double>(0.25));
	}
	curl_easy_cleanup(curlDatabaseUpdater);
}

void Pool::_processPayments() {
	CURL *curlPaymentProcessor(curl_easy_init());
	std::chrono::time_point<std::chrono::steady_clock> latestPayoutsTp(std::chrono::steady_clock::now());
	while (_running) {
		LOGMSG("Processing Payments...");
		std::shared_ptr<sql::ResultSet> sqlResult;
		std::map<std::string, std::pair<double, uint64_t>> balances; // Also remember the latest Id in case new movements are concurrently added
		try {
			sqlResult = database.executeQuery("SELECT id, value, address FROM RiecoinMovements WHERE address IS NOT NULL ORDER BY id ASC;");
			while (sqlResult->next()) {
				const uint64_t id(sqlResult->getUInt64("id"));
				const double value(sqlResult->getDouble("value"));
				const std::string address(sqlResult->getString("address"));
				const auto balanceIt(balances.find(address));
				if (balanceIt != balances.end()) {
					balanceIt->second.first += value;
					balanceIt->second.second = id;
				}
				else
					balances[address] = {value, id};
			}
		}
		catch (sql::SQLException &e) {
			SQLERRORMSG("Something went wrong while computing the anonymous balances, it will be attempted again, no manual intervention needed unless it keeps failing");
			balances.clear();
		}
		
		for (auto it(balances.begin()) ; it != balances.end() ; it++) {
			const std::string address(it->first);
			const double amount(it->second.first);
			const uint64_t latestId(it->second.second);
			if (amount > configuration.options().withdrawalAutomaticThreshold) {
				LOGMSG(address << " reached the automatic payout threshold with an amount of " << amountStr(amount) << " RIC, creating withdrawal...");
				try {
					database.execute("START TRANSACTION;"s
								   + "INSERT INTO Withdrawals (time, amount, address, state, comment) VALUES ("s + std::to_string(nowU64()) + ", "s + amountStr(amount - configuration.options().withdrawalFee) + ", '"s + address + "', 0, 'Automatic withdrawal');"s
								   + "DELETE FROM RiecoinMovements WHERE address = '"s + address + "' AND id <= " + std::to_string(latestId) + ";"s
								   + "COMMIT;"s);
				}
				catch (sql::SQLException &e) {
					SQLERRORMSG("Something went wrong while generating a withdrawal, it will be attempted again, no manual intervention needed unless it keeps failing");
					std::this_thread::sleep_for(std::chrono::duration<double>(0.5));
					continue;
				}
			}
		}
		
		try {
			sqlResult = database.executeQuery("SELECT id, amount, address FROM Withdrawals WHERE state = 0 ORDER BY id ASC LIMIT 256;"); // Limit number of outputs in a single Transaction
		}
		catch (sql::SQLException &e) {
			SQLERRORMSG("Could not get pending withdrawals, it will be attempted again, no manual intervention needed unless it keeps failing");
			std::this_thread::sleep_for(std::chrono::duration<double>(1.));
			continue;
		}
		
		std::string query("START TRANSACTION;");
		std::map<std::string, double> outputs;
		while (sqlResult->next()) {
			try {
				const uint64_t id(sqlResult->getUInt64("id"));
				const double amount(sqlResult->getDouble("amount"));
				const std::string address(sqlResult->getString("address"));
				LOGMSG("Processing withdrawal " << id << " of " << amountStr(amount) << " RIC to " << address << "...");
				query += "UPDATE Withdrawals SET state = 3 WHERE id = "s + std::to_string(id) + ";"s;
				auto payoutIt(outputs.find(address)); // Merge outputs to same Address
				if (payoutIt != outputs.end())
					payoutIt->second += amount;
				else
					outputs[address] = amount;
				outputs[address] = std::floor(1e8*outputs[address])/1e8; // SendMany does not work if there are more than 8 decimal digits :|, round down
			}
			catch (sql::SQLException &e) {
				SQLERRORMSG("Something went wrong while processing a withdrawal, it will be attempted again, no manual intervention needed unless it keeps failing");
				std::this_thread::sleep_for(std::chrono::duration<double>(0.5));
				continue;
			}
		}
		query += "COMMIT;"s;
		try {
			database.execute(query);
		}
		catch (sql::SQLException &e) {
			SQLERRORMSG("Could not update withdrawal states, it will be attempted again, no manual intervention needed unless it keeps failing");
			std::this_thread::sleep_for(std::chrono::duration<double>(1.));
			continue;
		}
		
		if (outputs.size() > 0) {
			LOGMSG("Creating transaction with " << outputs.size() << " output(s) (excluding change)...");
			std::string txId;
			try {
				auto sendmany(_sendRequestToWallet(curlPaymentProcessor, "sendmany", {"", outputs, "", "StellaPool Withdrawal"}));
				txId = sendmany["result"];
				database.execute("UPDATE Withdrawals SET state = 1, txid = '"s + txId + "' WHERE state = 3"s);
				LOGMSG("Successfully sent transaction " << txId);
			}
			catch (const std::exception &e) {
				ERRORMSG("Could not send payouts, you may have to do it manually - " << e.what());
				try {
					database.execute("UPDATE Withdrawals SET state = 4 WHERE state = 3"s);
				}
				catch (sql::SQLException &e) {
					SQLERRORMSG("Could not change the state of failed withdrawals! They will be incorrectly shown as successful or have a wrong TxId! Please take a look!");
				}
			}
		}
		latestPayoutsTp = std::chrono::steady_clock::now();
		while (timeSince(latestPayoutsTp) < configuration.options().withdrawalProcessingInterval && _running)
			std::this_thread::sleep_for(std::chrono::duration<double>(0.25));
	}
	curl_easy_cleanup(curlPaymentProcessor);
}

void Pool::_fetchJob() {
	nlohmann::json getblocktemplate, getblocktemplateResult;
	try {
		getblocktemplate = _sendRequestToWallet(_curlMain, "getblocktemplate", {{{"rules", {"segwit"}}}});
		if (getblocktemplate == nullptr)
			return;
		getblocktemplateResult = getblocktemplate["result"];
	}
	catch (std::exception &e) {
		std::cout << "Could not get GetBlockTemplate Data!" << std::endl;
		return;
	}
	StratumJob job;
	job.bh = BlockHeader();
	job.transactionsHex = std::string();
	job.txHashes = std::vector<std::array<uint8_t, 32>>();
	job.default_witness_commitment = std::string();
	try {
		job.bh.version = getblocktemplateResult["version"];
		job.bh.previousblockhash = v8ToA8(reverse(hexStrToV8(getblocktemplateResult["previousblockhash"])));
		job.bh.curtime = getblocktemplateResult["curtime"];
		job.bh.bits = std::stoll(std::string(getblocktemplateResult["bits"]), nullptr, 16);
		job.coinbasevalue = getblocktemplateResult["coinbasevalue"];
		for (const auto &transaction : getblocktemplateResult["transactions"]) {
			const std::vector<uint8_t> txId(reverse(hexStrToV8(transaction["txid"])));
			job.transactionsHex += transaction["data"];
			job.txHashes.push_back(v8ToA8(txId));
		}
		job.default_witness_commitment = getblocktemplateResult["default_witness_commitment"];
		job.height = getblocktemplateResult["height"];
		job.powVersion = getblocktemplateResult["powversion"];
		if (job.powVersion != 1) {
			std::cout << "Unsupported PoW Version " << job.powVersion << ", StellaPool is likely outdated!" << std::endl;
			return;
		}
		job.acceptedPatterns = getblocktemplateResult["patterns"].get<decltype(job.acceptedPatterns)>();
		if (job.acceptedPatterns.size() == 0) {
			std::cout << "Empty or invalid accepted patterns list!" << std::endl;
			return;
		}
	}
	catch (...) {
		std::cout << "Received GetBlockTemplate Data with invalid parameters!" << std::endl;
		std::cout << "Json Object was: " << getblocktemplateResult.dump() << std::endl;
		return;
	}
	
	if (_currentJobs.size() == 0 ? true : (job.height != _currentJobs.back().height || timeSince(_latestJobTp) > jobRefreshInterval)) {
		std::string messageToSend;
		job.id = _currentJobId++;
		job.coinbase1Gen();
		job.coinbase2Gen(_scriptPubKey);
		if (_currentJobs.size() == 0) {
			LOGMSG("Started Pool at Block " << job.height << ", difficulty " << FIXED(3) << decodeBits(job.bh.bits, job.powVersion));
			_currentJobs = {job};
			messageToSend = _generateMiningNotify(true);
		}
		else if (job.height != _currentJobs.back().height) {
			LOGMSG("Block " << job.height << ", difficulty " << FIXED(3) << decodeBits(job.bh.bits, job.powVersion));
			_currentJobs = {job};
			messageToSend = _generateMiningNotify(true);
		}
		else {
			LOGMSG("Refreshing current job and broadcasting. " << job.txHashes.size() + 1U << " transaction(s), " << amountStr(static_cast<double>(job.coinbasevalue)/1e8) << " RIC reward"); // Count Coinbase
			_currentJobs.push_back(job);
			messageToSend = _generateMiningNotify(false);
		}
		_latestJobTp = std::chrono::steady_clock::now();
		for (const auto &miner : _miners) {
			if (write(miner.second->fileDescriptor, messageToSend.c_str(), messageToSend.size()) < 0)
				LOGMSG("Could not notify " << miner.second->str());
		}
	}
}

void Pool::run() {
	addrinfo hints;
	addrinfo *result, *rp;
	memset(&hints, 0, sizeof(addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	LOGMSG("Binding to port " << configuration.options().poolPort << "...");
	if (getaddrinfo(NULL, std::to_string(configuration.options().poolPort).c_str(), &hints, &result) != 0) {
		ERRORMSG("Getaddrinfo failed, errno " << errno << " - " << std::strerror(errno));
		return;
	}
	int poolFd, epollFd;
	epoll_event event, events[maxEvents];
	for (rp = result ; rp != NULL ; rp = rp->ai_next) {
		poolFd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (poolFd == -1)
			continue;
		int optval(1);
		if (setsockopt(poolFd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(decltype(optval))) < 0) // Allows to restart the Pool without delay, otherwise there will be an "Address Already in Use" error for some time
			ERRORMSG("Setsockopt could not set SO_REUSEADDR (safe to ignore)");
		if (bind(poolFd, rp->ai_addr, rp->ai_addrlen) == 0)
			break;
		close(poolFd);
	}
	if (rp == NULL) {
		ERRORMSG("Unable to bind, errno " << errno << " - " << std::strerror(errno));
		return;
	}
	freeaddrinfo(result);
	LOGMSG("Success, the Pool File Descriptor is " << poolFd);
	if (fcntl(poolFd, F_SETFL, fcntl(poolFd, F_GETFL, 0) | O_NONBLOCK) == -1) {
		ERRORMSG("Unable to make the socket non blocking, errno " << errno << " - " << std::strerror(errno));
		return;
	}
	if (listen(poolFd, SOMAXCONN) == -1) {
		ERRORMSG("Unable to listen to socket, errno " << errno << " - " << std::strerror(errno));
		return;
	}
	LOGMSG("Listening, max connections: " << SOMAXCONN);
	epollFd = epoll_create1(0);
	if (epollFd == -1) {
		ERRORMSG("Unable to create Epoll instance, errno " << errno << " - " << std::strerror(errno));
		return;
	}
	event.data.fd = poolFd;
	event.events = EPOLLIN | EPOLLET;
	if (epoll_ctl(epollFd, EPOLL_CTL_ADD, poolFd, &event) == -1) {
		ERRORMSG("Epoll_ctl failed, errno " << errno << " - " << std::strerror(errno));
		return;
	}
	memset(&events, 0, maxEvents*sizeof(epoll_event));
	
	_curlMain = curl_easy_init();
	_currentJobId = 0ULL;
	_fetchJob();
	if (_currentJobs.size() < 1) {
		ERRORMSG("Could not get a first job, check if Riecoin Core is running and your configuration");
		curl_easy_cleanup(_curlMain);
		return;
	}
	
	LOGMSG("Loading current round...");
	std::shared_ptr<sql::ResultSet> sqlResult;
	try {
		sqlResult = database.executeQuery("SELECT * FROM Rounds ORDER BY id DESC LIMIT 1;");
	}
	catch (sql::SQLException &e) {
		SQLERRORMSG("Could not get latest round");
		curl_easy_cleanup(_curlMain);
		return;
	}
	bool needNewRound(false);
	if (sqlResult->next()) {
		Round r;
		try {
			r.id = sqlResult->getUInt("id");
			r.heightStart = sqlResult->getUInt("heightStart");
			r.heightEnd = sqlResult->getUInt("heightEnd");
			if (r.heightEnd.value() == 0U) r.heightEnd = std::nullopt;
			r.timeStart = sqlResult->getUInt64("timeStart");
			r.timeEnd = sqlResult->getUInt64("timeEnd");
			if (r.timeEnd.value() == 0ULL) r.timeEnd = std::nullopt;
			r.blockHash = sqlResult->getString("blockHash");
			if (r.blockHash.value() == ""s) r.blockHash = std::nullopt;
			r.state = sqlResult->getUInt("state");
			r.finder = sqlResult->getString("finder");
			if (r.finder.value() == "") r.finder = std::nullopt;
			r.scores = _decodeScores(sqlResult->getString("scores"));
		}
		catch (sql::SQLException &e) {
			SQLERRORMSG("Invalid round");
			curl_easy_cleanup(_curlMain);
			return;
		}
		_currentRound = r;
		if (_currentRound.heightEnd.has_value()) {
			LOGMSG("Latest round already ended, starting a new one...");
			needNewRound = true;
		}
	}
	else {
		LOGMSG("No round found, creating...");
		_currentRound.id = 0;
		needNewRound = true;
	}
	if (needNewRound) {
		try {
			_startNewRound(_currentJobs.back().height);
		}
		catch (const std::exception &e) {
			ERRORMSG("Could not create first round - " << e.what());
			curl_easy_cleanup(_curlMain);
			return;
		}
	}
	
	LOGMSG("Round " << _currentRound.id << ": since Block " << _currentRound.heightStart);
	LOGMSG(getScoreTotal(_currentRound.scores) << " point(s) from " << _currentRound.scores.size() << " miner(s)");
	
	_running = true;
	LOGMSG("Starting database updater...");
	_databaseUpdater = std::thread(&Pool::_updateDatabase, this);
	LOGMSG("Starting payment processor...");
	_paymentProcessor = std::thread(&Pool::_processPayments, this);
	while (_running) {
		// Update work
		_fetchJob();
		// Process messages from miners if any
		const int nEvents(epoll_wait(epollFd, events, maxEvents, 100));
		for (int i(0) ; i < nEvents ; i++) {
			if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))) {
				LOGMSG("Connection on fd " << events[i].data.fd << " closed");
				close(events[i].data.fd);
				auto minerIt(_miners.find(events[i].data.fd));
				if (minerIt != _miners.end())
					_miners.erase(minerIt);
				continue;
			}
			else if (events[i].data.fd == poolFd) { // New connection(s)
				while (_running) {
					sockaddr address;
					socklen_t addressLength(sizeof(address));
					int minerFd(accept(poolFd, &address, &addressLength));
					if (minerFd == -1) {
						if (errno != EAGAIN && errno != EWOULDBLOCK)
							ERRORMSG("Unable to process incoming connection(s), errno " << errno << " - " << std::strerror(errno));
						break;
					}
					const std::shared_ptr<Miner> newMiner(std::make_shared<Miner>());
					newMiner->fileDescriptor = minerFd;
					char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
					if (getnameinfo(&address, addressLength, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
						ERRORMSG("Something went wrong with Getnameinfo, fd " << newMiner->fileDescriptor << ", errno " << errno << " - " << std::strerror(errno));
						close(newMiner->fileDescriptor);
						continue;
					}
					newMiner->ip = hbuf;
					if (_isBanned(newMiner->ip)) {
						LOGMSG("Rejected connection from banned IP " << newMiner->ip << " on fd " << newMiner->fileDescriptor);
						_punish(newMiner->ip, -30.);
						close(newMiner->fileDescriptor);
						continue;
					}
					try {newMiner->port = std::stoi(sbuf);}
					catch (...) {ERRORMSG("Unable to get port of miner from " << newMiner->ip); newMiner->port = 0U;}
					uint32_t ipOccurrences(0U);
					for (const auto &_miner : _miners) {
						if (_miner.second->ip == newMiner->ip) {
							ipOccurrences++;
							if (ipOccurrences >= maxMinersFromSameIp)
								break;
						}
					}
					if (ipOccurrences >= maxMinersFromSameIp) {
						LOGMSG("Too many connections from " << newMiner->ip << "! Limit: " << maxMinersFromSameIp);
						_punish(newMiner->ip, -30.);
						const std::string messageToSend("{\"id\": null, \"method\": \"client.show_message\", \"params\": [\"Too many connections, consider doing solo mining or spreading your mining power accross other pools\"]}\n"s);
						write(newMiner->fileDescriptor, messageToSend.c_str(), messageToSend.size());
						LOGMSG("Rejected connection from " << newMiner->str() << ". Miners: " << _miners.size());
						close(newMiner->fileDescriptor);
						continue;
					}
					if (fcntl(newMiner->fileDescriptor, F_SETFL, fcntl(newMiner->fileDescriptor, F_GETFL, 0) | O_NONBLOCK) == -1) {
						ERRORMSG("Unable to make incoming socket non blocking, errno " << errno << " - " << std::strerror(errno));
						close(newMiner->fileDescriptor);
						continue;
					}
					event.data.fd = newMiner->fileDescriptor;
					event.events = EPOLLIN | EPOLLET;
					if (epoll_ctl(epollFd, EPOLL_CTL_ADD, newMiner->fileDescriptor, &event) == -1) {
						ERRORMSG("Epoll_ctl failed, errno " << errno << " - " << std::strerror(errno));
						close(newMiner->fileDescriptor);
						continue;
					}
					_miners[newMiner->fileDescriptor] = newMiner;
					LOGMSG("Accepted connection from new " << newMiner->str() << ". Miners: " << _miners.size());
				}
			}
			else { // Data to be processed
				std::shared_ptr<Miner> miner;
				try {
					miner = _miners.at(events[i].data.fd);
				}
				catch (...) {
					ERRORMSG("Could not find the miner with fd " << events[i].data.fd);
					close(events[i].data.fd);
					continue;
				}
				if (_isBanned(miner->ip)) {
					LOGMSG("Ignoring message from banned IP " << miner->ip << " on fd " << events[i].data.fd);
					close(events[i].data.fd);
					_miners.erase(events[i].data.fd);
					continue;
				}
				std::string receivedMessage;
				while (_running) { // Combine partial messages in this loop
					constexpr std::size_t bufferSize(2U*maxMessageLength);
					ssize_t count;
					char buffer[bufferSize];
					memset(&buffer, 0, bufferSize);
					count = read(events[i].data.fd, buffer, bufferSize - 1U);
					receivedMessage += buffer;
					if (receivedMessage.size() > maxMessageLength) { // Ignore unreasonably long message (possible attack)
						LOGMSG("Ignoring long message of " << receivedMessage.size() << " bytes and kicking " << miner->str());
						_punish(miner->ip, -30.);
						close(events[i].data.fd);
						_miners.erase(events[i].data.fd);
						break;
					}
					if (count == -1) { // Either message fully reconstructed, or something went wrong
						if (errno != EAGAIN && errno != EWOULDBLOCK) {
							close(events[i].data.fd);
							_miners.erase(events[i].data.fd);
							LOGMSG("Connection with " << miner->str() << " closed, errno " << errno << " - " << std::strerror(errno) << ". Miners: " << _miners.size());
							break;
						}
						// It is possible that multiple lines have to be treated at once. We need to process all of them.
						std::stringstream resultSS(receivedMessage);
						std::string line;
						while (std::getline(resultSS, line)) {
							const std::pair<std::string, bool> reply(_processMessage(std::make_pair(miner, line))); // Message to send and whether the miner should be disconnected
							if (write(miner->fileDescriptor, reply.first.c_str(), reply.first.size()) == -1)
								ERRORMSG("Could not send message to " << miner->str() << ": " << reply.first);
							if (reply.second || _isBanned(miner->ip)) {
								LOGMSG("Disconnecting " << miner->str());
								close(events[i].data.fd);
								_miners.erase(events[i].data.fd);
								break;
							}
						}
						break;
					}
					else if (count == 0) { // The miner left
						close(events[i].data.fd);
						_miners.erase(events[i].data.fd);
						LOGMSG(miner->str() << " left. Miners: " << _miners.size());
						break;
					}
				}
			}
		}
		for (auto it(_miners.begin()) ; it != _miners.end() ; it++) {
			const std::string disconnectMessage("{\"id\": null, \"method\": \"client.show_message\", \"params\": [\"Miner disconnected due to inactivity\"]}\n"s);
			if (timeSince(it->second->latestShareTp) > maxInactivityTime) {
				LOGMSG("Disconnecting inactive " << it->second->str());
				_punish(it->second->ip, 1.5*maxInactivityTime);
				write(it->second->fileDescriptor, disconnectMessage.c_str(), disconnectMessage.size());
				close(it->second->fileDescriptor);
				_miners.erase(it);
			}
		}
	}
	_databaseUpdater.join();
	_paymentProcessor.join();
	close(epollFd);
	close(poolFd);
	curl_easy_cleanup(_curlMain);
}
