// (c) 2018-2022 Pttn (https://riecoin.dev/en/StellaPool)
// Lots of code taken from rieMiner (https://riecoin.dev/en/rieMiner).

#ifndef HEADER_tools_hpp
#define HEADER_tools_hpp

#include <array>
#include <chrono>
#include <gmpxx.h>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "external/picosha2.h"

#define leading0s(x) std::setw(x) << std::setfill('0')
#define FIXED(x) std::fixed << std::setprecision(x)

#define LOGMSG(message) std::cout << formattedClockTimeNow() << " " << message << std::endl
#define ERRORMSG(message) std::cerr << formattedClockTimeNow() << " E: "  << __func__ << ": " << message << std::endl
#define SQLERRORMSG(message) std::cerr << formattedClockTimeNow() << " E: " << __func__ << ": " << message << "; " << e.what() << " (MySQL Error " << e.getErrorCode() << ", SQLState: " << e.getSQLState() << ")" << std::endl

uint8_t rand(uint8_t, uint8_t);

inline bool isHexStr(const std::string &s) {
	return std::all_of(s.begin(), s.end(), [](unsigned char c){return std::isxdigit(c);});
}
inline bool isHexStrOfSize(const std::string &s, const size_t size) {
	if (s.size() != size)
		return false;
	return std::all_of(s.begin(), s.end(), [](unsigned char c){return std::isxdigit(c);});
}

inline std::string v8ToHexStr(const std::vector<uint8_t> &v) {
	std::ostringstream oss;
	for (const auto &u8 : v) oss << std::setfill('0') << std::setw(2) << std::hex << static_cast<uint32_t>(u8);
	return oss.str();
}
std::vector<uint8_t> hexStrToV8(std::string);
inline std::array<uint8_t, 32> v8ToA8(std::vector<uint8_t> v8) {
	std::array<uint8_t, 32> a8{0};
	std::copy_n(v8.begin(), std::min(static_cast<int>(v8.size()), 32), a8.begin());
	return a8;
}
inline std::vector<uint8_t> a8ToV8(const std::array<uint8_t, 32> &a8) {
	return std::vector<uint8_t>(a8.begin(), a8.end());
}

template <class C> inline C reverse(C c) {
	std::reverse(c.begin(), c.end());
	return c;
}
template <class C> std::string formatContainer(const C& container) {
	std::ostringstream oss;
	for (auto it(container.begin()) ; it < container.end() ; it++) {
		oss << *it;
		if (it != container.end() - 1) oss << ", ";
	}
	return oss.str();
}

inline std::array<uint8_t, 32> sha256(const uint8_t *data, uint32_t len) {
	std::array<uint8_t, 32> hash;
	picosha2::hash256(data, data + len, hash.begin(), hash.end());
	return hash;
}
inline std::array<uint8_t, 32> sha256d(const uint8_t *data, uint32_t len) {
	return sha256(sha256(data, len).data(), 32);
}

std::vector<uint32_t> generatePrimeTable(const uint32_t);
const std::vector<uint32_t> primeTable(generatePrimeTable(821641)); // Used to calculate the Primorial when checking

std::vector<uint8_t> bech32ToScriptPubKey(const std::string&);

double decodeBits(const uint32_t, const int32_t);
struct BlockHeader {
	uint32_t version;
	std::array<uint8_t, 32> previousblockhash;
	std::array<uint8_t, 32> merkleRoot;
	uint64_t curtime;
	uint32_t bits;
	std::array<uint8_t, 32> nOffset;
	
	std::vector<uint8_t> toV8() const;
	mpz_class target(const int32_t) const;
};

std::array<uint8_t, 32> calculateMerkleRoot(const std::vector<std::array<uint8_t, 32>>&);

inline double timeSince(const std::chrono::time_point<std::chrono::steady_clock> &t0) {
	const std::chrono::time_point<std::chrono::steady_clock> t(std::chrono::steady_clock::now());
	const std::chrono::duration<double> dt(t - t0);
	return dt.count();
}
inline uint64_t nowU64() {
	return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
std::string formattedClockTimeNow();
std::string formattedDuration(const double &duration);
inline std::string amountStr(const double amount) {
	std::ostringstream oss;
	oss << FIXED(8) << std::floor(1e8*amount)/1e8;
	return oss.str();
}

#endif
