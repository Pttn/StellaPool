<?php // (c) 2022 Pttn (https://riecoin.dev/en/StellaPool)

$confPath = '../Pool/Pool.conf';
$domain = '127.0.0.1';
$cookieSecure = false;

class RiecoinRPC {
	private $credentials;
	private $url;
	private $id = 0;
	
	public function __construct($daemonConf) {
		$this->credentials = $daemonConf['rpcuser'] . ':' . $daemonConf['rpcpassword'];
		$this->url = 'http://' . $daemonConf['rpcip'] . ':' . $daemonConf['rpcport'] . '/wallet/' . $daemonConf['walletname'];
	}
	
	public function __call($method, $params) {
		$this->id++; // The ID should be unique for each call
		$request = json_encode(array(
			'method' => $method,
			'params' => $params,
			'id'	 => $this->id
		));
		$curl = curl_init($this->url);
		curl_setopt_array($curl, array(
			CURLOPT_HTTPAUTH       => CURLAUTH_BASIC,
			CURLOPT_USERPWD        => $this->credentials,
			CURLOPT_RETURNTRANSFER => true,
			CURLOPT_FOLLOWLOCATION => true,
			CURLOPT_MAXREDIRS      => 10,
			CURLOPT_HTTPHEADER     => array('Content-type: application/json'),
			CURLOPT_POST           => true,
			CURLOPT_POSTFIELDS     => $request
		));
		$response = curl_exec($curl);
		$status = curl_getinfo($curl, CURLINFO_HTTP_CODE);
		$curlError = curl_error($curl);
		curl_close($curl);
		if (!empty($curlError))
			throw new Exception($curlError);
		$jsonResponse = json_decode($response, true);
		if (isset($jsonResponse['error']['message']))
			throw new Exception($jsonResponse['error']['message']);
		if ($status !== 200)
			throw new Exception($method . ' call failed with HTTP code ' . $status);
		return $jsonResponse['result'];
	}
}

function isAlphaNumSpaces($str) {
	return strlen($str) === 0 || preg_match('/^[a-zA-Z0-9ĉĝĵŝŭĈĜĴŜŬ\040]+$/', $str);
}
const minUsernameLength = 3;
const maxUsernameLength = 20;
const maxAddresses = 8;
const maxTokens = 16;
const maxTokenNameLength = 32;

class Stella {
	private $options;
	private $database;
	private $riecoinRPC;
	
	private $currentIp;
	private $currentUserId;
	
	public function __construct($confPath) {
		try {
			$configurationFile = file_get_contents(__DIR__ . '/' . $confPath);
			if ($configurationFile == false)
				throw new Exception('Could not read the configuration file');
			$this->options = array();
			$lines = explode("\n", $configurationFile);
			foreach ($lines as &$line) {
				if (strlen($line) == 0)
					continue;
				if ($line[0] == '#')
					continue;
				$keyValue = explode('=', $line, 2);
				if (count($keyValue) == 2)
					$this->options[trim($keyValue[0])] = trim($keyValue[1]);
			}
			$this->database = new PDO('mysql:host=' . ($this->options['DatabaseHost'] ?? '127.0.0.1') . ':' . ($this->options['DatabasePort'] ?? 3306) . ';dbname=' . $this->options['DatabaseName'] . ';', $this->options['DatabaseUsername'], $this->options['DatabasePassword'], [
				PDO::ATTR_EMULATE_PREPARES   => false,
				PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
				PDO::MYSQL_ATTR_FOUND_ROWS   => true,
				PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION
			]);
			if (isset($this->options['WalletCookie'])) {
				$cookie = file_get_contents($this->options['WalletCookie']);
				if ($cookie == false)
					throw new Exception('Could not read Cookie');
				$userPass = explode(':', $cookie, 2);
				if (count($userPass) != 2)
					throw new Exception('Invalid Cookie');
				$this->options['WalletUsername'] = $userPass[0];
				$this->options['WalletPassword'] = $userPass[1];
			}
			$this->riecoinRPC = new RiecoinRPC(array(
				'rpcuser' => $this->options['WalletUsername'],
				'rpcpassword' => $this->options['WalletPassword'],
				'rpcip' => $this->options['WalletHost'] ?? "127.0.0.1",
				'rpcport' => $this->options['WalletPort'] ?? 28332,
				'walletname' => $this->options['WalletName']));
			$this->currentIp = $_SERVER['REMOTE_ADDR'];
			$this->currentUserId = null;
			if (!empty($_COOKIE['session'])) {
				if (strlen($_COOKIE['session']) != 64 || !ctype_xdigit($_COOKIE['session']))
					$this->unsetSessionCookie();
				else {
					$tokenHash = hash('sha256', hex2bin($_COOKIE['session']), true);
					try {
						$statement = $this->database->prepare('SELECT userId FROM Sessions WHERE tokenHash = :tokenHash');
						$statement->bindValue(':tokenHash', $tokenHash, PDO::PARAM_LOB);
						$statement->execute();
						$userId = $statement->fetch()['userId'] ?? false;
						if (!is_int($userId))
							$this->unsetSessionCookie();
						else {
							$this->currentUserId = $userId;
							$statement = $this->database->prepare('UPDATE Users SET lastSeen = :now WHERE id = :userId');
							$statement->bindValue(':now', time(), PDO::PARAM_INT);
							$statement->bindValue(':userId', $this->currentUserId, PDO::PARAM_INT);
							$statement->execute();
						}
					}
					catch (Exception $e) {}
				}
			}
		}
		catch (Exception $e) {
			die('Sorry, we are encountering technical difficulties, please refresh or come back later: ' . $e->getMessage());
		}
	}
	
	private function logEvent($type, $details) {
		try {
			$statement = $this->database->prepare('INSERT INTO Events (time, ip, userId, type, details) VALUES (:now, :ip, :userId, :type, :details)');
			$statement->bindValue(':now', time(), PDO::PARAM_INT);
			$statement->bindValue(':ip', $this->currentIp, PDO::PARAM_STR);
			$statement->bindValue(':userId', $this->currentUserId, PDO::PARAM_INT);
			$statement->bindValue(':type', $type, PDO::PARAM_STR);
			$statement->bindValue(':details', $details, PDO::PARAM_STR);
			$statement->execute();
		}
		catch (Exception $e) {}
	}
	public function option($key) {return $this->options[$key] ?? null;}
	public function riecoinRPC() {return $this->riecoinRPC;}
	public function unsetSessionCookie() {global $domain, $cookieSecure;
		setcookie('session', '', time() - 1, '/', $domain, $cookieSecure, true);
	}
	public function loggedIn() {return is_int($this->currentUserId);}
	public function getCurrentUserId() {return $this->currentUserId;}
	public function getCurrentUser() {
		if ($this->loggedIn()) {
			try {
				return $this->getUserById($this->currentUserId);
			}
			catch (Exception $e) {
				return null;
			}
		}
		return null;
	}
	
	public function getUserById($userId) {
		$statement = $this->database->prepare('SELECT * FROM Users WHERE id = :userId');
		$statement->bindValue(':userId', $userId, PDO::PARAM_INT);
		$statement->execute();
		$user = $statement->fetch();
		if (empty($user['username']))
			throw new Exception('User not found');
		return $user;
	}
	public function getUserByUsername($username) {
		$statement = $this->database->prepare('SELECT * FROM Users WHERE username = :username');
		$statement->bindValue(':username', $username, PDO::PARAM_STR);
		$statement->execute();
		$user = $statement->fetch();
		if (!isset($user['username']))
			throw new Exception('User not found');
		return $user;
	}
	public function getUserByToken($tokenHex) {
		if (strlen($tokenHex) != 64 || !ctype_xdigit($tokenHex))
			throw new Exception('Invalid token');
		$statement = $this->database->prepare('SELECT userId, powcRemaining FROM Tokens WHERE hash = :hash');
		$statement->bindValue(':hash', hash('sha256', hex2bin($tokenHex), true), PDO::PARAM_LOB);
		$statement->execute();
		$tokenData = $statement->fetch();
		if (empty($tokenData['userId']))
			throw new Exception('Token not found');
		$user = $this->getUserById($tokenData['userId']);
		$user['powc'] = $this->getPowCredits($user['id']);
		$user['powcLimit'] = $tokenData['powcRemaining'];
		return $user;
	}
	public function getUserIdByUsername($username) {
		$statement = $this->database->prepare('SELECT id FROM Users WHERE username = :username');
		$statement->bindValue(':username', $username, PDO::PARAM_STR);
		$statement->execute();
		$user = $statement->fetch();
		if (!isset($user['id']))
			throw new Exception('User not found');
		return $user['id'];
	}
	
	public function checkUsername($username) {
		try {
			if (strlen($username) > maxUsernameLength)
				throw new Exception('Too long username');
			if (strlen($username) < minUsernameLength)
				throw new Exception('Too short username');
			if (!ctype_alnum($username))
				throw new Exception('The username has symbols that are not allowed');
			if (!ctype_alpha($username[0]))
				throw new Exception('The username must start with a letter');
			$statement = $this->database->prepare('SELECT COUNT(*) FROM Users WHERE username = :username');
			$statement->bindValue(':username', $username, PDO::PARAM_STR);
			$statement->execute();
			if ($statement->fetch()['COUNT(*)'] !== 0)
				throw new Exception('Username already taken');
			return true;
		}
		catch (Exception $e) {
			return $e->getMessage();
		}
	}
	public function checkRiecoinAddress($address, $checkExistence = false) {
		try {
			if (strlen($address) > 128)
				throw new Exception('Invalid Riecoin address');
			if (!ctype_alnum($address))
				throw new Exception('Invalid Riecoin address');
			$getaddressinfoResponse = $this->riecoinRPC->getaddressinfo($address);
			$addressResponse = $getaddressinfoResponse['address'] ?? null;
			$isWitnessAddress = $getaddressinfoResponse['iswitness'] ?? null;
			if ($address !== $addressResponse)
				throw new Exception('Invalid Riecoin address');
			if ($isWitnessAddress !== true)
				throw new Exception('Not a Bech32 address');
			if ($checkExistence === false)
				return true;
			$statement = $this->database->prepare('SELECT COUNT(*) FROM RiecoinAddresses WHERE address = :address');
			$statement->bindValue(':address', $address, PDO::PARAM_STR);
			$statement->execute();
			if ($statement->fetch()['COUNT(*)'] !== 0)
				throw new Exception('The address is already used');
			return true;
		}
		catch (Exception $e) {
			return $e->getMessage();
		}
	}
	public function checkCode($address, $code) {
		try {
			$status = $this->checkRiecoinAddress($address);
			if ($status !== true)
				throw new Exception($status);
			if (strlen($code) > 256)
				throw new Exception('Invalid code');
			$verifycodeResponse = $this->riecoinRPC->verifycode($address, $code);
			$valid = $verifycodeResponse ?? false;
			if ($valid !== true)
				throw new Exception('Invalid code');
			return true;
		}
		catch (Exception $e) {
			return $e->getMessage();
		}
	}
	public function checkCodeByUserId($userId, $code) {
		try {
			$addresses = $this->getAddresses($userId);
			if (count($addresses) == 0)
				throw new Exception('The user does not exist');
			foreach ($addresses as &$address) {
				if ($this->checkCode($address, $code) === true)
					return true;
			}
			return 'Invalid code';
		}
		catch (Exception $e) {
			return $e->getMessage();
		}
	}
	
	public function createUser($username, $address, $code) {
		try {
			$status = $this->checkUsername($username);
			if ($status === true)
				$status = $this->checkRiecoinAddress($address, true);
			if ($status === true)
				$status = $this->checkCode($address, $code);
			if ($status !== true)
				throw new Exception($status);
			$this->database->query('START TRANSACTION;');
			$statement = $this->database->prepare('INSERT INTO Users(registrationTime, username, balance, powc) VALUES(:now, :username, 0, 0);');
			$statement->bindValue(':now', time(), PDO::PARAM_INT);
			$statement->bindValue(':username', $username, PDO::PARAM_STR);
			$statement->execute();
			$userId = $this->getUserIdByUsername($username);
			$statement = $this->database->prepare('INSERT INTO RiecoinAddresses(address, userId) VALUES(:address, :userId)');
			$statement->bindValue(':address', $address, PDO::PARAM_STR);
			$statement->bindValue(':userId', $userId, PDO::PARAM_INT);
			$statement->execute();
			$this->database->query('COMMIT;');
			$this->logEvent('createUser', 'Created user ' . $username . ', Id ' . $userId);
			return true;
		}
		catch (Exception $e) {
			$this->logEvent('createUserFail', $e->getMessage());
			return $e->getMessage();
		}
	}
	public function deleteUser($code) {
		try {
			$status = $this->checkCodeByUserId($this->currentUserId, $code);
			if ($status !== true)
				throw new Exception($status);
			$statement = $this->database->prepare('DELETE FROM Users WHERE id = :userId');
			$statement->bindValue(':userId', $this->currentUserId, PDO::PARAM_INT);
			$statement->execute();
			$this->logEvent('deleteUser', 'User deleted');
			return true;
		}
		catch (Exception $e) {
			$this->logEvent('deleteUserFail', $e->getMessage());
			return $e->getMessage();
		}
	}
	public function login($username, $code, $duration) {
		try {
			if (intval($duration) == 0)
				throw new Exception('Invalid duration');
			if (intval($duration) < 60)
				throw new Exception('Duration must be >= 60 s');
			if (intval($duration) > 157784760)
				throw new Exception('Duration must be <= 5 years');
			$userId = $this->getUserIdByUsername($username);
			$status = $this->checkCodeByUserId($userId, $code);
			if ($status !== true)
				throw new Exception($status);
			$token = random_bytes(32);
			$tokenHash = hash('sha256', $token, true);
			$expiration = time() + $duration;
			$statement = $this->database->prepare('INSERT INTO Sessions(tokenHash, userId, expiration) VALUES(:tokenHash, :userId, :expiration)');
			$statement->bindValue(':tokenHash', $tokenHash, PDO::PARAM_LOB);
			$statement->bindValue(':userId', $userId, PDO::PARAM_INT);
			$statement->bindValue(':expiration', $expiration, PDO::PARAM_INT);
			$statement->execute();
			$statement = $this->database->prepare('UPDATE Users SET lastLogin = :now WHERE id = :userId');
			$statement->bindValue(':now', time(), PDO::PARAM_INT);
			$statement->bindValue(':userId', $userId, PDO::PARAM_INT);
			$statement->execute();
			setcookie('session', bin2hex($token), $expiration, '/', '', false, true);
			$this->logEvent('login', $username . ' successfully logged in');
			return true;
		}
		catch (Exception $e) {
			$this->logEvent('loginFail', $e->getMessage());
			return $e->getMessage();
		}
	}
	public function logout() {
		if ($this->loggedIn() === true) {
			try {
				$tokenHash = hash('sha256', hex2bin($_COOKIE['session']), true);
				$statement = $this->database->prepare('DELETE FROM Sessions WHERE tokenHash = :tokenHash;');
				$statement->bindValue(':tokenHash', $tokenHash, PDO::PARAM_LOB);
				$statement->execute();
			}
			catch (Exception $e) {
				return $e->getMessage();
			}
		}
		$this->unsetSessionCookie();
		return true;
	}
	
	public function getAddresses($userId) {
		$statement = $this->database->prepare('SELECT address FROM RiecoinAddresses WHERE userId = :userId');
		$statement->bindValue(':userId', $userId, PDO::PARAM_INT);
		$statement->execute();
		$addresses = array();
		while ($addressData = $statement->fetch())
			$addresses[] = $addressData['address'];
		return $addresses;
	}
	
	public function addAddress($address, $code) {
		try {
			$status = $this->checkRiecoinAddress($address, true);
			if ($status === true)
				$status = $this->checkCode($address, $code);
			if ($status !== true)
				throw new Exception($status);
			$statement = $this->database->prepare('SELECT COUNT(*) FROM RiecoinAddresses WHERE userId = :userId');
			$statement->bindValue(':userId', $this->currentUserId, PDO::PARAM_INT);
			$statement->execute();
			if ($statement->fetch()['COUNT(*)'] >= maxAddresses)
				throw new Exception('Cannot have more than ' . maxAddresses . ' addresses');
			$statement = $this->database->prepare('INSERT INTO RiecoinAddresses(address, userId) VALUES(:address, :userId)');
			$statement->bindValue(':address', $address, PDO::PARAM_STR);
			$statement->bindValue(':userId', $this->currentUserId, PDO::PARAM_INT);
			$statement->execute();
			$this->logEvent('addAddress', $address . ' added');
			return true;
		}
		catch (Exception $e) {
			$this->logEvent('addAddressFail', $e->getMessage());
			return $e->getMessage();
		}
	}
	public function removeAddress($address) {
		try {
			$statement = $this->database->prepare('SELECT COUNT(*) FROM RiecoinAddresses WHERE userId = :userId');
			$statement->bindValue(':userId', $this->currentUserId, PDO::PARAM_INT);
			$statement->execute();
			if ($statement->fetch()['COUNT(*)'] < 2)
				throw new Exception('At least be one Riecoin address must be kept');
			$statement = $this->database->prepare('SELECT userId FROM RiecoinAddresses WHERE address = :address');
			$statement->bindValue(':address', $address, PDO::PARAM_STR);
			$statement->execute();
			$addressUserId = $statement->fetch()['userId'] ?? null;
			if ($addressUserId !== $this->currentUserId)
				throw new Exception('The Riecoin address is invalid or not associated to this user');
			$statement = $this->database->prepare('DELETE FROM RiecoinAddresses WHERE address = :address;');
			$statement->bindValue(':address', $address, PDO::PARAM_STR);
			$statement->execute();
			$this->logEvent('removeAddress', $address . ' successfully removed');
			return true;
		}
		catch (Exception $e) {
			$this->logEvent('removeAddressFail', $e->getMessage());
			return $e->getMessage();
		}
	}
	
	public function getTokens() {
		$statement = $this->database->prepare('SELECT * FROM Tokens WHERE userId = :userId ORDER BY id DESC');
		$statement->bindValue(':userId', $this->currentUserId, PDO::PARAM_INT);
		$statement->execute();
		$tokens = array();
		while ($token = $statement->fetch())
			$tokens[] = $token;
		return $tokens;
	}
	public function createToken($duration, $powcLimit, $name, $code) {
		try {
			if (intval($duration) == 0)
				throw new Exception('Invalid duration');
			if (intval($duration) < 60)
				throw new Exception('Duration must be >= 60 s');
			if (intval($duration) > 157784760)
				throw new Exception('Duration must be <= 5 years');
			if (!is_numeric($powcLimit))
				throw new Exception('Invalid PoWc Limit');
			if ($powcLimit < 0.001)
				throw new Exception('PoWc Limit must be >= 0.001');
			if (strlen($name) > maxTokenNameLength)
				throw new Exception('The name is too long');
			if (!isAlphaNumSpaces($name))
				throw new Exception('The name can only be alphanumeric with spaces');
			$status = $this->checkCodeByUserId($this->currentUserId, $code);
			if ($status !== true)
				throw new Exception($status);
			$statement = $this->database->prepare('SELECT COUNT(*) FROM Tokens WHERE userId = :userId');
			$statement->bindValue(':userId', $this->currentUserId, PDO::PARAM_INT);
			$statement->execute();
			if ($statement->fetch()['COUNT(*)'] >= maxTokens)
				throw new Exception('Cannot have more than ' . maxTokens . ' tokens');
			$token = random_bytes(32);
			$tokenHash = hash('sha256', $token, true);
			$statement = $this->database->prepare('INSERT INTO Tokens(hash, userId, expiration, powcRemaining, name) VALUES(:hash, :userId, :expiration, :powcRemaining, :name)');
			$statement->bindValue(':hash', $tokenHash, PDO::PARAM_LOB);
			$statement->bindValue(':userId', $this->currentUserId, PDO::PARAM_INT);
			$statement->bindValue(':expiration', time() + $duration, PDO::PARAM_INT);
			$statement->bindValue(':powcRemaining', $powcLimit, PDO::PARAM_STR);
			$statement->bindValue(':name', $name, PDO::PARAM_STR);
			$statement->execute();
			$this->logEvent('createToken', 'Successfully created token, hash ' . bin2hex($tokenHash));
			return bin2hex($token);
		}
		catch (Exception $e) {
			$this->logEvent('createTokenFail', $e->getMessage());
			return $e->getMessage();
		}
	}
	public function deleteToken($tokenId) {
		try {
			$statement = $this->database->prepare('SELECT userId, hash FROM Tokens WHERE id = :tokenId');
			$statement->bindValue(':tokenId', $tokenId, PDO::PARAM_INT);
			$statement->execute();
			$token = $statement->fetch();
			if (($token['userId'] ?? false) !== $this->currentUserId)
				throw new Exception('The token does not exist or does not belong to the user');
			$statement = $this->database->prepare('DELETE FROM Tokens WHERE id = :tokenId');
			$statement->bindValue(':tokenId', $tokenId, PDO::PARAM_INT);
			$statement->execute();
			$this->logEvent('deleteToken', 'Successfully deleted token, hash ' . bin2hex($token['hash']));
			return true;
		}
		catch (Exception $e) {
			$this->logEvent('deleteTokenFail', $e->getMessage());
			return $e->getMessage();
		}
	}
	
	public function getBalance() {
		$statement = $this->database->prepare('SELECT balance FROM Users WHERE id = :userId');
		$statement->bindValue(':userId', $this->currentUserId, PDO::PARAM_INT);
		$statement->execute();
		$user = $statement->fetch();
		if (!is_numeric($user['balance']))
			throw new Exception('User not found or invalid');
		return floor(1e8*$user['balance'])/1e8;
	}
	public function getBalanceAddress($address) {
		$statement = $this->database->prepare('SELECT SUM(value) FROM RiecoinMovements WHERE address = :address;');
		$statement->bindValue(':address', $address, PDO::PARAM_STR);
		$statement->execute();
		$data = $statement->fetch();
		if (!isset($data['SUM(value)']))
			return 0.;
		if (!is_numeric($data['SUM(value)']))
			return 0.;
		return $data['SUM(value)'];
	}
	public function getRiecoinTransactions() {
		$statement = $this->database->prepare('SELECT * FROM RiecoinMovements WHERE userId = :userId ORDER BY id DESC');
		$statement->bindValue(':userId', $this->currentUserId, PDO::PARAM_INT);
		$statement->execute();
		return $statement;
	}
	public function moveBalanceFromAddress($address) {
		try {
			$statement = $this->database->prepare('SELECT userId FROM RiecoinAddresses WHERE address = :address');
			$statement->bindValue(':address', $address, PDO::PARAM_STR);
			$statement->execute();
			$addressData = $statement->fetch();
			if (($addressData['userId'] ?? false) !== $this->currentUserId)
				throw new Exception('No user is registered with this Riecoin address or it does not belong to this user');
			$amount = $this->getBalanceAddress($address);
			if ($amount <= 0.)
				throw new Exception('Address balance is empty');
			$this->database->query('START TRANSACTION;');
			$statement = $this->database->prepare('DELETE FROM RiecoinMovements WHERE address = :address');
			$statement->bindValue(':address', $address, PDO::PARAM_STR);
			$statement->execute();
			$statement = $this->database->prepare('INSERT INTO RiecoinMovements (time, value, userId, comment) VALUES (' . time() . ', :amount, :userId, "Balance moved to Account")');
			$statement->bindValue(':amount', $amount, PDO::PARAM_STR);
			$statement->bindValue(':userId', $this->currentUserId, PDO::PARAM_INT);
			$statement->execute();
			$statement = $this->database->prepare('UPDATE Users SET balance = balance + :amount WHERE id = :userId');
			$statement->bindValue(':amount', $amount, PDO::PARAM_STR);
			$statement->bindValue(':userId', $this->currentUserId, PDO::PARAM_INT);
			$statement->execute();
			$this->database->query('COMMIT');
			$this->logEvent('moveBalance', $amount . ' RIC from ' . $address . ' moved to User ' . $this->currentUserId);
			return true;
		}
		catch (Exception $e) {
			$this->logEvent('moveBalanceFail', $e->getMessage());
			return $e->getMessage();
		}
	}
	
	public function getWithdrawals() {
		$statement = $this->database->prepare('SELECT * FROM Withdrawals WHERE userId = :userId ORDER BY id DESC');
		$statement->bindValue(':userId', $this->currentUserId, PDO::PARAM_INT);
		$statement->execute();
		return $statement;
	}
	public function withdraw($amount, $address, $code) {
		try {
			if (!is_numeric($amount))
				throw new Exception('Invalid amount');
			$status = $this->checkRiecoinAddress($address);
			if ($status === true)
				$status = $this->checkCodeByUserId($this->currentUserId, $code);
			if ($status !== true)
				throw new Exception($status);
			$balance = $this->getBalance();
			$withdrawalMinimum = $this->options['WithdrawalMinimum'];
			$withdrawalFee = $this->options['WithdrawalFee'];
			if ($amount < $withdrawalMinimum)
				throw new Exception('Amount must be at least ' . $this->options['WithdrawalMinimum'] . ' RIC');
			if ($amount > $balance)
				throw new Exception('Amount exceeds the balance');
			$this->database->query('START TRANSACTION');
			if ($amount + 0.00000001 < $balance) {
				$statement = $this->database->prepare('INSERT INTO RiecoinMovements (time, value, userId, comment) VALUES (:now, :value, :userId, "Manual Withdrawal")');
				$statement->bindValue(':now', time(), PDO::PARAM_INT);
				$statement->bindValue(':value', -$amount, PDO::PARAM_STR);
				$statement->bindValue(':userId', $this->currentUserId, PDO::PARAM_INT);
				$statement->execute();
				$statement = $this->database->prepare('UPDATE Users SET balance = balance - :amount WHERE id = :userId');
				$statement->bindValue(':amount', $amount, PDO::PARAM_STR);
				$statement->bindValue(':userId', $this->currentUserId, PDO::PARAM_INT);
				$statement->execute();
			}
			else {
				$statement = $this->database->prepare('DELETE FROM RiecoinMovements WHERE userId = :userId');
				$statement->bindValue(':userId', $this->currentUserId, PDO::PARAM_INT);
				$statement->execute();
				$statement = $this->database->prepare('INSERT INTO RiecoinMovements (time, value, userId, comment) VALUES (:now, 0., :userId, :comment)');
				$statement->bindValue(':now', time(), PDO::PARAM_INT);
				$statement->bindValue(':userId', $this->currentUserId, PDO::PARAM_INT);
				$statement->bindValue(':comment', 'All the ' . $amount . ' RIC were withdrawn.', PDO::PARAM_STR);
				$statement->execute();
				$statement = $this->database->prepare('UPDATE Users SET balance = 0. WHERE id = :userId');
				$statement->bindValue(':userId', $this->currentUserId, PDO::PARAM_INT);
				$statement->execute();
			}
			$statement = $this->database->prepare('INSERT INTO Withdrawals (time, amount, userId, address, comment) VALUES (:now, :amount, :userId, :address, "Manual Withdrawal")');
			$statement->bindValue(':now', time(), PDO::PARAM_INT);
			$statement->bindValue(':amount', $amount - $withdrawalFee, PDO::PARAM_STR);
			$statement->bindValue(':userId', $this->currentUserId, PDO::PARAM_INT);
			$statement->bindValue(':address', $address, PDO::PARAM_STR);
			$statement->execute();
			$this->database->query('COMMIT');
			$this->logEvent('withdraw', $amount . ' RIC to ' . $address);
			return true;
		}
		catch (Exception $e) {
			$this->logEvent('withdrawFail', $e->getMessage());
			return $e->getMessage();
		}
	}
	
	public function getBlocks($userId) {
		$statement = $this->database->prepare('SELECT blockHash FROM Rounds WHERE finder = :userId AND timeEnd > :time ORDER BY id DESC');
		$statement->bindValue(':userId', strval($userId), PDO::PARAM_STR);
		$statement->bindValue(':time', time() - 604800, PDO::PARAM_STR);
		$statement->execute();
		$blocks = array();
		while ($roundData = $statement->fetch())
			$blocks[] = $roundData['blockHash'];
		return $blocks;
	}
	
	public function getPowCredits($userId) {
		$statement = $this->database->prepare('SELECT id, amount FROM PoWc WHERE userId = :userId ORDER BY expiration ASC');
		$statement->bindValue(':userId', $userId, PDO::PARAM_INT);
		$statement->execute();
		$powc = array('amounts' => array(), 'total' => 0.);
		while ($powcData = $statement->fetch()) {
			$powc['amounts'][] = $powcData;
			$powc['total'] += $powcData['amount'];
		}
		return $powc;
	}
	public function getPowCreditsMovements() {
		$statement = $this->database->prepare('SELECT * FROM PoWcMovements WHERE userId = :userId ORDER BY id DESC');
		$statement->bindValue(':userId', $this->currentUserId, PDO::PARAM_INT);
		$statement->execute();
		return $statement;
	}
	public function getPoWInfo($tokenHex) {
		$user = $this->getUserByToken($tokenHex);
		return array(
			'username' => $user['username'],
			'registrationTime' => $user['registrationTime'],
			'powc' => min($user['powc']['total'], $user['powcLimit']),
			'addresses' => $this->getAddresses($user['id']),
			'blocks' => $this->getBlocks($user['id'])
		);
	}
	public function consumePoWc($tokenHex, $amount, $comment) {
		if ($amount < 0.001)
			throw new Exception('Cannot deduct less than 0.001 PoWc');
		if ($amount > 1.)
			throw new Exception('Cannot deduct more than 1 PoWc');
		$user = $this->getUserByToken($tokenHex);
		if ($amount > min($user['powc']['total'], $user['powcLimit']))
			throw new Exception('Insufficient PoWc');
		$this->database->query('START TRANSACTION');
		$tokenHash = hash('sha256', hex2bin($tokenHex), true);
		$remainingAmount = $amount;
		foreach ($user['powc']['amounts'] as &$powc) {
			if ($remainingAmount < $powc['amount']) {
				$statement = $this->database->prepare('UPDATE PoWc SET amount = amount - :consumedAmount WHERE id = :id');
				$statement->bindValue(':consumedAmount', $remainingAmount, PDO::PARAM_STR);
				$statement->bindValue(':id', $powc['id'], PDO::PARAM_INT);
				$statement->execute();
				break;
			}
			$remainingAmount -= $powc['amount'];
			$statement = $this->database->prepare('DELETE FROM PoWc WHERE id = :id');
			$statement->bindValue(':id', $powc['id'], PDO::PARAM_INT);
			$statement->execute();
		}
		$statement = $this->database->prepare('UPDATE Tokens SET powcRemaining = powcRemaining - :amount WHERE hash = :hash');
		$statement->bindValue(':hash', $tokenHash, PDO::PARAM_LOB);
		$statement->bindValue(':amount', $amount, PDO::PARAM_STR);
		$statement->execute();
		$statement = $this->database->prepare('INSERT INTO PoWcMovements (time, value, userId, comment) VALUES (:now, :amount, :userId, :comment)');
		$statement->bindValue(':now', time(), PDO::PARAM_INT);
		$statement->bindValue(':amount', -$amount, PDO::PARAM_STR);
		$statement->bindValue(':userId', $user['id'], PDO::PARAM_INT);
		$statement->bindValue(':comment', htmlspecialchars($comment), PDO::PARAM_STR);
		$statement->execute();
		$this->database->query('COMMIT;');
		return true;
	}
	
	public function getBlockchainInfo() {
		$blockchainInfo = $this->riecoinRPC->getblockchaininfo();
		$blockchainInfo['miningpower'] = $this->riecoinRPC->getnetworkminingpower();
		$blockchainInfo['connections'] = $this->riecoinRPC->getconnectioncount();
		return $blockchainInfo;
	}
	
	public function getPoolInfo() {
		$blockchainInfo = $this->riecoinRPC->getblockchaininfo();
		$info = array();
		$info['time'] = time();
		$info['currency'] = 'Riecoin';
		$info['ticker'] = 'RIC';
		$info['algorithm'] = 'Stella';
		$info['poolAddress'] = $this->options['PoolAddress'];
		$statement = $this->database->query('SELECT COUNT(*) FROM Rounds WHERE State = 1');
		$info['blocksFound'] = $statement->fetch()['COUNT(*)'];
		$statement = $this->database->query('SELECT heightEnd, blockHash FROM Rounds WHERE State = 1 ORDER BY id DESC LIMIT 1');
		$info['latestKnownBlockHeight'] = $blockchainInfo['blocks'];
		$latestBlock = $statement->fetch();
		if ($latestBlock != false) {
			$info['latestFoundBlockHeight'] = $latestBlock['heightEnd'];
			$info['latestFoundBlockHash'] = $latestBlock['blockHash'];
		}
		$statement = $this->database->query('SELECT COUNT(DISTINCT finder) AS miners FROM SharesRecent');
		$info['activeMiners'] = $statement->fetch()['miners'];
		$statement = $this->database->query('SELECT COUNT(*) FROM SharesRecent WHERE points > 0');
		$info['shareRate'] = $statement->fetch()['COUNT(*)']/3600.;
		$statement = $this->database->query('SELECT COUNT(*) FROM SharesRecent WHERE points > 0');
		$D = $blockchainInfo['difficulty'];
		$r = 0.0181116*$D;
		$P = (10./($r*$r*$r)*(1. - 1./$r)*(1. - 1./$r) + 5./($r*$r*$r*$r)*(1. - 1./$r) + 1./($r*$r*$r*$r*$r))/($r*$r);
		$cps = $info['shareRate']/$P;
		$info['candidatesPerSecond'] = $cps;
		$info['miningPower'] = 150.*$cps/($r*$r*$r*$r*$r*$r*$r)*pow($D/600., 9.3);
		return $info;
	}
	
	public function getRecentStatistics() {
		$recentStats = array();
		$statement = $this->database->query('SELECT COUNT(DISTINCT finder) AS miners FROM SharesRecent');
		$recentStats['activeMiners'] = $statement->fetch()['miners'];
		$statement = $this->database->query('SELECT COUNT(*) FROM SharesRecent WHERE points > 0');
		$recentStats['validShares'] = $statement->fetch()['COUNT(*)'];
		$statement = $this->database->query('SELECT COUNT(*) FROM SharesRecent WHERE points <= 0');
		$recentStats['invalidShares'] = $statement->fetch()['COUNT(*)'];
		return $recentStats;
	}
	
	public function getRound($roundId) {
		$statement = $this->database->prepare('SELECT * FROM Rounds WHERE id = :roundId');
		$statement->bindValue(':roundId', $roundId, PDO::PARAM_INT);
		$statement->execute();
		$round = $statement->fetch();
		if (!isset($round['id']))
			throw new Exception('Round not found');
		return $round;
	}
	
	public function getCurrentRoundId() {
		$statement = $this->database->query('SELECT id FROM Rounds ORDER BY id DESC LIMIT 1');
		return $statement->fetch()['id'] ?? 0;
	}
}

function formatDuration($duration) {
	if ($duration > 86400)
		return intdiv($duration, 86400) . ' d ' . (intdiv($duration, 3600) % 24) . ' h ' . (intdiv($duration, 60) % 60) . ' min ' . ($duration % 60) . ' s';
	else if ($duration > 3600)
		return intdiv($duration, 3600) . ' h ' . (intdiv($duration, 60) % 60) . ' min ' . ($duration % 60) . ' s';
	else if ($duration > 60)
		return intdiv($duration, 60) . ' min ' . ($duration % 60) . ' s';
	else
		return $duration . ' s';
}
?>
