// (c) 2022-present Pttn (https://riecoin.xyz/StellaPool/)

#ifndef HEADER_main_hpp
#define HEADER_main_hpp

#include <fstream>
#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include "tools.hpp"

using namespace std::string_literals;
using namespace std::chrono_literals;

#define versionString	"StellaPool 0.9"

struct Options {
	std::string poolAddress{"ric1pstellap55ue6keg3ta2qwlxr0h58g66fd7y4ea78hzkj3r4lstrsk4clvn"s};
	uint16_t poolPort{2005U};
	double poolFee{0.01};
	uint16_t poolRequiredConfirmations{100U};
	double withdrawalMinimum{0.5}, withdrawalFee{0.01}, withdrawalAutomaticThreshold{5.}, withdrawalProcessingInterval{60.};
	std::string walletHost{"127.0.0.1"s};
	uint16_t walletPort{28332U};
	std::string walletName{""s}, walletUsername{""s}, walletPassword{""s}, walletCookie{""s}, databaseHost{"127.0.0.1"s};
	uint16_t databasePort{3306U};
	std::string databaseName{"Stella"s}, databaseUsername{""s}, databasePassword{""s};
	double databaseUpdateInterval{10.};
};

class Configuration {
	Options _options;
	std::optional<std::pair<std::string, std::string>> _parseLine(const std::string&) const;
public:
	bool parse(const int, char**);
	Options options() const {return _options;}
};

class Database {
	sql::ConnectOptionsMap _connectOptions;
	sql::Driver *_driver;
	bool _ready;
public:
	Database() : _ready(false) {}
	Database(const std::string &host, const uint16_t port, const std::string &name, const std::string &username, const std::string &password) : _ready(false) {
		try {
			_connectOptions["hostName"] = host;
			_connectOptions["userName"] = username;
			_connectOptions["password"] = password;
			_connectOptions["schema"] = name;
			_connectOptions["port"] = port;
			_connectOptions["CLIENT_MULTI_STATEMENTS"] = true;
			_driver = get_driver_instance();
		}
		catch (sql::SQLException &e) {
			SQLERRORMSG("Could not connect to the database");
			return;
		}
		
		try {
			execute("CREATE TABLE IF NOT EXISTS Users("
			"id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,"
			"registrationTime BIGINT UNSIGNED NOT NULL,"
			"lastLogin BIGINT UNSIGNED,"
			"lastSeen BIGINT UNSIGNED,"
			"username VARCHAR(20) NOT NULL UNIQUE,"
			"balance DOUBLE NOT NULL DEFAULT 0.);");
		}
		catch (sql::SQLException &e) {
			SQLERRORMSG("Could not create users table");
			return;
		}
		
		try {
			execute("CREATE TABLE IF NOT EXISTS RiecoinAddresses("
			"address VARCHAR(90) PRIMARY KEY,"
			"userId BIGINT UNSIGNED NOT NULL,"
			"FOREIGN KEY (userId) REFERENCES Users(id) ON DELETE CASCADE);");
		}
		catch (sql::SQLException &e) {
			SQLERRORMSG("Could not create Riecoin addresses table");
			return;
		}
		
		try {
			execute("CREATE TABLE IF NOT EXISTS Sessions("
			"tokenHash BINARY(32) PRIMARY KEY,"
			"userId BIGINT UNSIGNED NOT NULL,"
			"FOREIGN KEY (userId) REFERENCES Users(id) ON DELETE CASCADE,"
			"expiration BIGINT UNSIGNED NOT NULL);");
		}
		catch (sql::SQLException &e) {
			SQLERRORMSG("Could not create sessions table");
			return;
		}
		
		try {
			execute("CREATE TABLE IF NOT EXISTS Tokens("
			"id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,"
			"hash BINARY(32) NOT NULL UNIQUE,"
			"userId BIGINT UNSIGNED NOT NULL,"
			"FOREIGN KEY (userId) REFERENCES Users(id) ON DELETE CASCADE,"
			"expiration BIGINT UNSIGNED NOT NULL,"
			"powcRemaining DOUBLE NOT NULL,"
			"name VARCHAR(32) NOT NULL);");
		}
		catch (sql::SQLException &e) {
			SQLERRORMSG("Could not create tokens table");
			return;
		}
		
		try {
			execute("CREATE TABLE IF NOT EXISTS Rounds("
			"id INT UNSIGNED PRIMARY KEY AUTO_INCREMENT,"
			"heightStart INT UNSIGNED NOT NULL,"
			"heightEnd INT UNSIGNED,"
			"timeStart BIGINT UNSIGNED NOT NULL,"
			"timeEnd BIGINT UNSIGNED,"
			"blockHash CHAR(64),"
			"state SMALLINT UNSIGNED NOT NULL,"
			"finder VARCHAR(90)," // UserId as string or Riecoin Address
			"scores MEDIUMTEXT NOT NULL);");
		}
		catch (sql::SQLException &e) {
			SQLERRORMSG("Could not create rounds table");
			return;
		}
		
		try {
			execute("CREATE TABLE IF NOT EXISTS SharesRecent("
			"time BIGINT UNSIGNED NOT NULL,"
			"finder VARCHAR(90) NOT NULL,"
			"points DOUBLE NOT NULL);");
		}
		catch (sql::SQLException &e) {
			SQLERRORMSG("Could not create recent shares table");
			return;
		}
		
		try {
			execute("CREATE TABLE IF NOT EXISTS RiecoinMovements("
			"id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,"
			"time BIGINT UNSIGNED NOT NULL,"
			"value DOUBLE NOT NULL,"
			"userId BIGINT UNSIGNED,"
			"FOREIGN KEY (userId) REFERENCES Users(id) ON DELETE CASCADE,"
			"address VARCHAR(90),"
			"comment VARCHAR(255) NOT NULL);");
		}
		catch (sql::SQLException &e) {
			SQLERRORMSG("Could not create internal transactions table");
			return;
		}
		
		try {
			execute("CREATE TABLE IF NOT EXISTS PoWc("
			"id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,"
			"expiration BIGINT UNSIGNED NOT NULL,"
			"amount DOUBLE NOT NULL,"
			"userId BIGINT UNSIGNED NOT NULL,"
			"FOREIGN KEY (userId) REFERENCES Users(id) ON DELETE CASCADE);");
		}
		catch (sql::SQLException &e) {
			SQLERRORMSG("Could not create PoWc table");
			return;
		}
		
		try {
			execute("CREATE TABLE IF NOT EXISTS PoWcMovements("
			"id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,"
			"time BIGINT UNSIGNED NOT NULL,"
			"value DOUBLE NOT NULL,"
			"userId BIGINT UNSIGNED NOT NULL,"
			"FOREIGN KEY (userId) REFERENCES Users(id) ON DELETE CASCADE,"
			"comment VARCHAR(255) NOT NULL);");
		}
		catch (sql::SQLException &e) {
			SQLERRORMSG("Could not create PoWc Movements table");
			return;
		}
		
		try {
			execute("CREATE TABLE IF NOT EXISTS Withdrawals("
			"id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,"
			"time BIGINT UNSIGNED NOT NULL,"
			"amount DOUBLE NOT NULL,"
			"userId BIGINT UNSIGNED,"
			"FOREIGN KEY (userId) REFERENCES Users(id) ON DELETE CASCADE,"
			"address VARCHAR(90) NOT NULL,"
			"state SMALLINT UNSIGNED NOT NULL DEFAULT 0,"
			"txid CHAR(64),"
			"comment VARCHAR(255) NOT NULL);");
		}
		catch (sql::SQLException &e) {
			SQLERRORMSG("Could not create withdrawals table");
			return;
		}
		
		try {
			execute("CREATE TABLE IF NOT EXISTS Events("
			"id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,"
			"time BIGINT UNSIGNED NOT NULL,"
			"ip VARCHAR(45) NOT NULL,"
			"userId BIGINT UNSIGNED,"
			"type VARCHAR(64) NOT NULL,"
			"details TEXT NOT NULL);");
		}
		catch (sql::SQLException &e) {
			SQLERRORMSG("Could not create event log table");
			return;
		}
		
		try {
			execute("SELECT id, registrationTime, lastLogin, lastSeen, username, balance FROM Users;");
			execute("SELECT address, userId FROM RiecoinAddresses;");
			execute("SELECT tokenHash, userId, expiration FROM Sessions;");
			execute("SELECT id, hash, userId, expiration, powcRemaining, name FROM Tokens;");
			execute("SELECT id, heightStart, heightEnd, timeStart, timeEnd, blockHash, state, finder, scores FROM Rounds;");
			execute("SELECT time, finder, points FROM SharesRecent;");
			execute("SELECT id, time, value, userId, address, comment FROM RiecoinMovements;");
			execute("SELECT id, expiration, amount, userId FROM PoWc;");
			execute("SELECT id, time, value, userId, comment FROM PoWcMovements;");
			execute("SELECT id, time, amount, userId, address, state, txid, comment FROM Withdrawals;");
			execute("SELECT id, time, ip, userId, type, details FROM Events;");
		}
		catch (sql::SQLException &e) {
			SQLERRORMSG("Sanity check failed, invalid database");
			return;
		}
		
		_ready = true;
	};
	
	bool isReady() const {return _ready;}
	
	void execute(const std::string &query) const {
		try {
			sql::ConnectOptionsMap connectOptions(_connectOptions); // For some reason, cannot use _connectOptions directly :|
			std::unique_ptr<sql::Connection> connection(_driver->connect(connectOptions));
			std::unique_ptr<sql::Statement> statement(connection->createStatement());
			statement->execute(query);
		}
		catch (sql::SQLException &e) {
			throw e;
		}
	}
	std::shared_ptr<sql::ResultSet> executeQuery(const std::string &query) const {
		try {
			sql::ConnectOptionsMap connectOptions(_connectOptions);
			std::unique_ptr<sql::Connection> connection(_driver->connect(connectOptions));
			std::unique_ptr<sql::Statement> statement(connection->createStatement());
			std::shared_ptr<sql::ResultSet> result(statement->executeQuery(query));
			return result;
		}
		catch (sql::SQLException &e) {
			throw e;
		}
	}
};

inline Configuration configuration;
inline Database database;

#endif
