// (c) 2022-present Pttn (https://riecoin.xyz/StellaPool/)

#include <nlohmann/json.hpp>
#include <signal.h>

#include "main.hpp"
#include "Pool.hpp"

bool running(false);

std::optional<std::pair<std::string, std::string>> Configuration::_parseLine(const std::string &line) const {
	if (line.size() == 0)
		return std::nullopt;
	if (line[0] == '#')
		return std::nullopt;
	const auto pos(line.find('='));
	if (pos != std::string::npos) {
		std::pair<std::string, std::string> option{line.substr(0, pos), line.substr(pos + 1, line.size() - pos - 1)};
		option.first.erase(std::find_if(option.first.rbegin(), option.first.rend(), [](unsigned char c) {return !std::isspace(c);}).base(), option.first.end()); // Trim spaces before =
		option.second.erase(option.second.begin(), std::find_if(option.second.begin(), option.second.end(), [](unsigned char c) {return !std::isspace(c);})); // Trim spaces after =
		return option;
	}
	else {
		std::cout << "Cannot find the delimiter '=' for: '" << line << "'" << std::endl;
		return std::nullopt;
	}
}

bool Configuration::parse(const int argc, char** argv) {
	std::string confPath("Pool.conf");
	if (argc >= 2)
		confPath = argv[1];
	std::vector<std::string> lines;
	std::ifstream file(confPath, std::ios::in);
	if (file) {
		std::cout << "Opening configuration file " << confPath << "..." << std::endl;
		std::string line;
		while (std::getline(file, line))
			lines.push_back(line);
		file.close();
	}
	else {
		std::cout << confPath << " not found or unreadable, please configure StellaPool now or check your configuration file." << std::endl;
		return false;
	}
	
	for (const auto &line : lines) {
		const std::optional<std::pair<std::string, std::string>> option(_parseLine(line));
		if (!option.has_value())
			continue;
		const std::string key(option.value().first), value(option.value().second);
		if (key == "PoolAddress") _options.poolAddress = value;
		else if (key == "PoolPort") {
			try {_options.poolPort = std::stoi(value);}
			catch (...) {_options.poolPort = 2005U;}
		}
		else if (key == "PoolFee") { // SToD does not throw exceptions
			_options.poolFee = std::stod(value);
			if (_options.poolFee < 0.001) _options.poolFee = 0.;
			if (_options.poolFee > 1.) _options.poolFee = 1.;
		}
		else if (key == "PoolRequiredConfirmations") {
			try {
				_options.poolRequiredConfirmations = std::stoi(value);
				if (_options.poolRequiredConfirmations < 1U) _options.poolRequiredConfirmations = 1U;
				if (_options.poolRequiredConfirmations > 120U) _options.poolRequiredConfirmations = 120U;
			}
			catch (...) {_options.poolRequiredConfirmations = 100U;}
		}
		else if (key == "WithdrawalMinimum") {
			_options.withdrawalMinimum = std::stod(value);
			if (_options.withdrawalMinimum < 0.1) _options.withdrawalMinimum = 0.1;
			if (_options.withdrawalMinimum > 50.) _options.withdrawalMinimum = 50.;
		}
		else if (key == "WithdrawalFee") {
			_options.withdrawalFee = std::stod(value);
			if (_options.withdrawalFee < 0.0001) _options.withdrawalFee = 0.;
			if (_options.withdrawalFee > 0.1) _options.withdrawalFee = 0.1;
		}
		else if (key == "WithdrawalAutomaticThreshold") {
			_options.withdrawalAutomaticThreshold = std::stod(value);
			if (_options.withdrawalAutomaticThreshold < 1.) _options.withdrawalAutomaticThreshold = 1.;
			if (_options.withdrawalAutomaticThreshold > 50.) _options.withdrawalAutomaticThreshold = 50.;
		}
		else if (key == "WithdrawalProcessingInterval") {
			_options.withdrawalProcessingInterval = std::stod(value);
			if (_options.withdrawalProcessingInterval < 1.) _options.withdrawalProcessingInterval = 1.;
			if (_options.withdrawalProcessingInterval > 3600.) _options.withdrawalProcessingInterval = 3600.;
		}
		else if (key == "WalletHost") _options.walletHost = value;
		else if (key == "WalletPort") {
			try {_options.walletPort = std::stoi(value);}
			catch (...) {_options.walletPort = 28332U;}
		}
		else if (key == "WalletName") _options.walletName = value;
		else if (key == "WalletUsername") _options.walletUsername = value;
		else if (key == "WalletPassword") _options.walletPassword = value;
		else if (key == "WalletCookie") _options.walletCookie = value;
		else if (key == "DatabaseHost") _options.databaseHost = value;
		else if (key == "DatabasePort") {
			try {_options.databasePort = std::stoi(value);}
			catch (...) {_options.databasePort = 3306U;}
		}
		else if (key == "DatabaseName") _options.databaseName = value;
		else if (key == "DatabaseUsername") _options.databaseUsername = value;
		else if (key == "DatabasePassword") _options.databasePassword = value;
		else if (key == "DatabaseUpdateInterval") {
			_options.databaseUpdateInterval = std::stod(value);
			if (_options.databaseUpdateInterval < 1.) _options.databaseUpdateInterval = 1.;
			if (_options.databaseUpdateInterval > 60.) _options.databaseUpdateInterval = 60.;
		}
	}
	std::cout << "Pool address: " << _options.poolAddress << std::endl;
	std::vector<uint8_t> scriptPubKey(bech32ToScriptPubKey(_options.poolAddress));
	if (scriptPubKey.size() == 0) {
		std::cout << "Invalid payout address! Please check it. Note that only Bech32 addresses are supported." << std::endl;
		return false;
	}
	std::cout << "  ScriptPubKey: " << v8ToHexStr(scriptPubKey) << std::endl;
	std::cout << "Pool Port: " << _options.poolPort << std::endl;
	std::cout << "Pool Fee: " << 100.*_options.poolFee << "%" << std::endl;
	std::cout << "Pool Required Confirmations: " << _options.poolRequiredConfirmations << std::endl;
	std::cout << "Minimum Withdrawal: " << amountStr(_options.withdrawalMinimum) << " RIC" << std::endl;
	std::cout << "Withdrawal Fee: " << amountStr(_options.withdrawalFee) << " RIC" << std::endl;
	std::cout << "Automatic Withdrawal Threshold: " << amountStr(_options.withdrawalAutomaticThreshold) << " RIC" << std::endl;
	std::cout << "Withdrawal Processing Interval: " << _options.withdrawalProcessingInterval << " s" << std::endl;
	std::cout << "Riecoin Wallet Server: " << _options.walletHost << ":" << _options.walletPort << std::endl;
	std::cout << "Riecoin Wallet Name: " << _options.walletName << std::endl;
	if (_options.walletCookie != "")
		std::cout << "Riecoin Wallet Cookie: " << _options.walletCookie << std::endl;
	else {
		std::cout << "Riecoin Wallet Username: " << _options.walletUsername << std::endl;
		std::cout << "Riecoin Wallet Password: ..." << std::endl;
	}
	std::cout << "Database: " << _options.databaseName << " at " << _options.databaseHost << ":" << _options.databasePort << std::endl;
	std::cout << "Database Username: " << _options.databaseUsername << std::endl;
	std::cout << "Database Password: ..." << std::endl;
	std::cout << "Database Update Interval: " << _options.databaseUpdateInterval << " s" << std::endl;
	return true;
}

void signalHandler(int signum) {
	if (Pool::pool) {
		std::cout << std::endl << "Signal " << signum << " received, stopping Pool." << std::endl;
		Pool::pool->stop();
	}
}

int main(int argc, char** argv) {
	struct sigaction SIGINTHandler;
	SIGINTHandler.sa_handler = signalHandler;
	sigemptyset(&SIGINTHandler.sa_mask);
	SIGINTHandler.sa_flags = 0;
	sigaction(SIGINT, &SIGINTHandler, NULL);
	
	std::cout << versionString << ", Riecoin Pool by Pttn" << std::endl;
	std::cout << "Project page: https://riecoin.dev/en/StellaPool" << std::endl;
	std::cout << "-----------------------------------------------------------" << std::endl;
	std::cout << "G++ " << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__ << " - https://gcc.gnu.org/" << std::endl;
	std::cout << "Curl " << LIBCURL_VERSION << " - https://curl.haxx.se/" << std::endl;
	std::cout << "GMP " << __GNU_MP_VERSION << "." << __GNU_MP_VERSION_MINOR << "." << __GNU_MP_VERSION_PATCHLEVEL << " - https://gmplib.org/" << std::endl;
	std::cout << "PicoSHA2 27fcf69 - https://github.com/okdshin/PicoSHA2"s << std::endl;
	std::cout << "MySQL Connector - https://dev.mysql.com/doc/connector-cpp/8.0/en/" << std::endl;
	std::cout << "NLohmann Json " << NLOHMANN_JSON_VERSION_MAJOR << "." << NLOHMANN_JSON_VERSION_MINOR << "." << NLOHMANN_JSON_VERSION_PATCH << " - https://json.nlohmann.me/" << std::endl;
	std::cout << "-----------------------------------------------------------" << std::endl;
	if (!configuration.parse(argc, argv))
		return 0;
	std::cout << "-----------------------------------------------------------" << std::endl;
	database = Database(configuration.options().databaseHost, configuration.options().databasePort, configuration.options().databaseName, configuration.options().databaseUsername, configuration.options().databasePassword);
	if (!database.isReady()) {
		std::cout << "Something went wrong while connecting to the database :|" << std::endl;
		return -1;
	}
	curl_global_init(CURL_GLOBAL_DEFAULT);
	Pool pool;
	pool.run();
	curl_global_cleanup();
	return 0;
}
