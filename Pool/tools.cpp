// (c) 2018-present Pttn (https://riecoin.xyz/StellaPool/)
// Lots of code taken from rieMiner (https://riecoin.xyz/rieMiner).

#include "tools.hpp"

std::random_device randomDevice;
uint8_t rand(uint8_t min, uint8_t max) {
	if (min > max) std::swap(min, max);
	std::uniform_int_distribution<uint8_t> urd(min, max);
	return urd(randomDevice);
}

std::vector<uint8_t> hexStrToV8(std::string str) {
	if (str.size() % 2 != 0) str = "0" + str;
	std::vector<uint8_t> v;
	for (std::string::size_type i(0) ; i < str.size() ; i += 2) {
		uint8_t byte;
		try {byte = std::stoll(str.substr(i, 2), nullptr, 16);}
		catch (...) {byte = 0;}
		v.push_back(byte);
	}
	return v;
}

std::vector<uint32_t> generatePrimeTable(const uint32_t limit) {
	if (limit < 2) return {};
	std::vector<uint64_t> compositeTable((limit + 127ULL)/128ULL, 0ULL); // Booleans indicating whether an odd number is composite: 0000100100101100...
	for (uint64_t f(3ULL) ; f*f <= limit ; f += 2ULL) { // Eliminate f and its multiples m for odd f from 3 to square root of the limit
		if (compositeTable[f >> 7ULL] & (1ULL << ((f >> 1ULL) & 63ULL))) continue; // Skip if f is composite (f and its multiples were already eliminated)
		for (uint64_t m((f*f) >> 1ULL) ; m <= (limit >> 1ULL) ; m += f) // Start eliminating at f^2 (multiples of f below were already eliminated)
			compositeTable[m >> 6ULL] |= 1ULL << (m & 63ULL);
	}
	std::vector<uint32_t> primeTable(1, 2);
	for (uint64_t i(1ULL) ; (i << 1ULL) + 1ULL <= limit ; i++) { // Fill the prime table using the composite table
		if (!(compositeTable[i >> 6ULL] & (1ULL << (i & 63ULL))))
			primeTable.push_back((i << 1ULL) + 1ULL); // Add prime number 2i + 1
	}
	return primeTable;
}

constexpr std::array<uint8_t, 128> bech32Values = {
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	 15, 255,  10,  17,  21,  20,  26,  30,   7,   5, 255, 255, 255, 255, 255, 255,
	255,  29, 255,  24,  13,  25,   9,   8,  23, 255,  18,  22,  31,  27,  19, 255,
	  1,   0,   3,  16,  11,  28,  12,  14,   6,   4,   2, 255, 255, 255, 255, 255,
	255,  29, 255,  24,  13,  25,   9,   8,  23, 255,  18,  22,  31,  27,  19, 255,
	  1,   0,   3,  16,  11,  28,  12,  14,   6,   4,   2, 255, 255, 255, 255, 255
};
static std::vector<uint8_t> expandHrp(const std::string& hrp) {
	std::vector<uint8_t> expandedHrp(2*hrp.size() + 1, 0);
	for (uint8_t i(0) ; i < hrp.size() ; i++) {
		expandedHrp[i] = hrp[i] >> 5;
		expandedHrp[i + hrp.size() + 1] = hrp[i] & 31;
	}
	return expandedHrp;
}
static uint32_t bech32Polymod(const std::vector<uint8_t>& values) {
	const uint32_t gen[5] = {0x3b6a57b2, 0x26508e6d, 0x1ea119fa, 0x3d4233dd, 0x2a1462b3};
	uint32_t chk(1);
	for (uint8_t i(0) ; i < values.size() ; i++) {
		uint8_t b(chk >> 25);
		chk = ((chk & 0x1ffffff) << 5) ^ values[i];
		for (uint8_t j(0) ; j < 5 ; j++) {if (b & (1 << j)) chk ^= gen[j];}
	}
	return chk;
}
static std::vector<uint8_t> v5ToV8(const std::vector<uint8_t>& v5) {
	std::vector<uint8_t> v8;
	int acc(0), bits(0);
	const int maxv((1 << 8) - 1), max_acc((1 << (5 + 8 - 1)) - 1);
	for (const auto &d5 : v5) {
		int value = d5;
		acc = ((acc << 5) | value) & max_acc;
		bits += 5;
		while (bits >= 8) {
			bits -= 8;
			v8.push_back((acc >> bits) & maxv);
		}
	}
	if (bits >= 5 || ((acc << (8 - bits)) & maxv))
		return {};
	return v8;
}
std::vector<uint8_t> bech32ToScriptPubKey(const std::string &address) {
	if (address.size() < 6 || address.size() > 90)
		return {};
	const auto delimiterPos(address.find('1'));
	if (delimiterPos >= address.size() - 6)
		return {};
	std::string addrHrp(address.substr(0, delimiterPos)),
	            addrData(address.substr(delimiterPos + 1, address.size() - delimiterPos - 1));
	std::vector<uint8_t> v5;
	for (const auto &c : addrData) {
		const uint8_t d5(bech32Values[static_cast<uint8_t>(c)]);
		if (d5 == 255) return {};
		v5.push_back(d5);
	}
	std::vector<uint8_t> expHrpData(expandHrp(addrHrp));
	expHrpData.insert(expHrpData.end(), v5.begin(), v5.end());
	if (bech32Polymod(expHrpData) != 0x2bc830a3)
		return {};
	std::vector<uint8_t> spk(v5ToV8(std::vector<uint8_t>(v5.begin() + 1, v5.end() - 6)));
	if ((spk.size() == 0 && addrData.size() != 6) || v5[0] > 16)
		return {};
	spk.insert(spk.begin(), spk.size());
	spk.insert(spk.begin(), v5[0] == 0 ? 0 : 80 + v5[0]);
	return spk;
}

double decodeBits(const uint32_t nBits, const int32_t powVersion) {
	if (powVersion == 1)
		return static_cast<double>(nBits)/256.;
	else
		ERRORMSG("Unexpected PoW Version " << powVersion << "! Please upgrade StellaPool!");
	return 1.;
}
std::vector<uint8_t> BlockHeader::toV8() const {
	std::vector<uint8_t> v8;
	for (uint32_t i(0) ; i < 4 ; i++) v8.push_back(reinterpret_cast<const uint8_t*>(&version)[i]);
	v8.insert(v8.end(), previousblockhash.begin(), previousblockhash.end());
	v8.insert(v8.end(), merkleRoot.begin(), merkleRoot.end());
	for (uint32_t i(0) ; i < 8 ; i++) v8.push_back(reinterpret_cast<const uint8_t*>(&curtime)[i]);
	for (uint32_t i(0) ; i < 4 ; i++) v8.push_back(reinterpret_cast<const uint8_t*>(&bits)[i]);
	v8.insert(v8.end(), nOffset.begin(), nOffset.end());
	return v8;
}
mpz_class BlockHeader::target(const int32_t powVersion) const {
	const uint32_t difficultyIntegerPart(decodeBits(bits, powVersion));
	uint32_t trailingZeros;
	const std::array<uint8_t, 32> hash(sha256d(toV8().data(), 80));
	mpz_class target;
	if (powVersion == 1) {
		if (difficultyIntegerPart < 264U) return 0;
		const uint32_t df(bits & 255U);
		target = 256 + ((10U*df*df*df + 7383U*df*df + 5840720U*df + 3997440U) >> 23U);
		target <<= 256;
		mpz_class hashGmp;
		mpz_import(hashGmp.get_mpz_t(), 32, -1, sizeof(uint8_t), 0, 0, hash.begin());
		target += hashGmp;
		trailingZeros = difficultyIntegerPart - 264U;
		target <<= trailingZeros;
	}
	else
		ERRORMSG("Unknown PoW Version " << powVersion << ", please upgrade StellaPool!");
	return target;
}

std::array<uint8_t, 32> calculateMerkleRoot(const std::vector<std::array<uint8_t, 32>> &txHashes) {
	std::array<uint8_t, 32> merkleRoot{};
	if (txHashes.size() == 0)
		ERRORMSG("No transaction to hash");
	else if (txHashes.size() == 1)
		return txHashes[0];
	else {
		std::vector<std::array<uint8_t, 32>> txHashes2;
		for (uint32_t i(0) ; i < txHashes.size() ; i += 2) {
			std::array<uint8_t, 64> concat;
			std::copy(txHashes[i].begin(), txHashes[i].end(), concat.begin());
			if (i == txHashes.size() - 1) // Concatenation of the last element with itself for an odd number of transactions
				std::copy(txHashes[i].begin(), txHashes[i].end(), concat.begin() + 32);
			else
				std::copy(txHashes[i + 1].begin(), txHashes[i + 1].end(), concat.begin() + 32);
			txHashes2.push_back(sha256d(concat.data(), 64));
		}
		// Process the next step
		merkleRoot = calculateMerkleRoot(txHashes2);
	}
	return merkleRoot;
}

std::string formattedClockTimeNow() {
	const auto now(std::chrono::system_clock::now());
	const auto seconds(std::chrono::time_point_cast<std::chrono::seconds>(now));
	const auto milliseconds(std::chrono::duration_cast<std::chrono::milliseconds>(now - seconds));
	const std::time_t timeT(std::chrono::system_clock::to_time_t(now));
	const std::tm *timeTm(std::gmtime(&timeT));
	std::ostringstream oss;
	oss << "[" << std::put_time(timeTm, "%H:%M:%S") << "." << leading0s(2) << static_cast<uint32_t>(std::floor(milliseconds.count()))/10 << "]";
	return oss.str();
}
std::string formattedDuration(const double &duration) {
	std::ostringstream oss;
	if (duration < 0.001) oss << std::round(1000000.*duration) << " us";
	else if (duration < 1.) oss << std::round(1000.*duration) << " ms";
	else if (duration < 60.) oss << FIXED(2 + (duration < 10.)) << duration << " s";
	else if (duration < 3600.) oss << FIXED(2 + (duration/60. < 10.)) << duration/60. << " min";
	else if (duration < 86400.) oss << FIXED(2 + (duration/3600. < 10.)) << duration/3600. << " h";
	else if (duration < 31556952.) oss << FIXED(3) << duration/86400. << " d";
	else oss << FIXED(3) << duration/31556952. << " y";
	return oss.str();
}

