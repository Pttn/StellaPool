<?php // (c) 2022 Pttn (https://riecoin.dev/en/StellaPool)
/*ini_set('display_errors', '1');
error_reporting(-1);*/

require_once('Stella.php');
$stella = new Stella($confPath);

function htmlTop($title) {
?>
<!DOCTYPE html>
<html lang="en">
	<head>
		<meta charset="utf-8"/>
		<style>
			body, input, select, button {
				color: #C0C0C0;
				background: #000000;
			}
			a {
				color: #C0C0C0;
			}
			.errorMessage {
				background-color: rgba(255, 0, 0, 0.25);
				padding: 8px;
			}
			.successMessage {
				background-color: rgba(0, 255, 0, 0.25);
				padding: 8px;
			}
			table {
				border-collapse: collapse;
				overflow-x: auto;
				white-space: nowrap;
			}
			th, td {
				border: 1px solid rgba(255, 255, 255, 0.5);
				padding: 4px;
			}
			th {
				background: rgba(255, 255, 255, 0.25);
			}
			td input, td button {
				width: 100%;
				box-sizing: border-box;
			}
		</style>
		<link rel="shortcut icon" href="Riecoin.svg"/>
		<title>StellaPool - <?php echo $title;?></title>
	</head>
	<body>
		<header><b style="font-size: 3em">StellaPool</b></header>
		<hr>
		<nav>
			<a href="index.php">Main page</a><span style="float:right;">
<?php global $stella;
if ($stella->loggedIn())
	echo '<a href="index.php?p=account">Account</a> | <a href="index.php?p=logout">Logout</a></span>';
else
	echo '<a href="index.php?p=login">Login</a> <a href="index.php?p=register">Register</a></span>';
?>
		</nav>
		<hr>
		<h1><?php echo $title;?></h1>
		<main>
<?php
}

function htmlBottom() {
?>
		</main>
		<hr>
		<footer>(c) 2022 - Pttn | <a href="https://riecoin.dev/en/StellaPool">Project Page</a></footer>
	</body>
</html>
<?php
}

$page = $_GET['p'] ?? '';
if ($page === 'round') {
	try {
		$roundId = intval($_GET['id'] ?? 0);
		$currentRoundId = $stella->getCurrentRoundId();
		$round = $stella->getRound($roundId);
		$scores = json_decode($round['scores'], true)['scores'];
		$timeStart = $round['timeStart'];
		$timeEnd = $round['timeEnd'] ?? time();
		$state = $round['state'];
		if (!isset($round['finder']))
			$blockTemplate = $stella->riecoinRPC()->getblocktemplate(array('rules' => array('segwit')));
		else {
			$finder = $round['finder'];
			if (ctype_digit($finder)) {
				try {
					$user = $stella->getUserById(intval($finder));
					if (isset($user['username']))
						$finder = $user['username'];
				}
				catch (Exception $e) {
					$finder = 'Deleted user';
				}
			}
			else
				$finder = $round['finder'];
			if ($state === 0 || $state === 1) {
				$block = $stella->riecoinRPC()->getblock($round['blockHash']);
				$coinbase = $stella->riecoinRPC()->getrawtransaction($block['tx'][0], 1);
			}
		}
		htmlTop('Round ' . $roundId);
		echo '<p>';
		if ($roundId > 1 && $roundId <= $currentRoundId)
			echo '<a href="?p=round&id=' . ($roundId - 1) . '">Previous Round</a> ';
		if ($roundId >= 1 && $roundId < $currentRoundId)
			echo ' <a href="?p=round&id=' . ($roundId + 1) . '">Next Round</a>';
		echo '</p><form method="get"><input type="hidden" name="p" value="round"/>';
		echo '<p>Jump to Round <input name="id"/> <button>Go</button></p></form>';
		
		echo '<h2>General Information</h2>';
		
		echo '<table>';
		echo '<tr><th>Id</th><td><a href="index.php?p=round&id=' . $round['id'] . '">' . $round['id'] . '</a></td></tr>';
		echo '<tr><th>Block Start</th><td>' . $round['heightStart'] . ', ' . gmdate("Y-m-d H:i:s", $timeStart) . '</td></tr>';
		if (!isset($finder)) {
			$reward = $blockTemplate['coinbasevalue'];
			$difficulty = hexdec($blockTemplate['bits'])/256;
			echo '<tr><th>Current Block</th><td>' . $blockTemplate['height'] . '</td></tr>';
			echo '<tr><th>Since</th><td>' . formatDuration($timeEnd - $timeStart) . '</td></tr>';
			echo '<tr><th>Current Reward</th><td>' . sprintf('%.8f', $reward/1e8) . ' RIC</td></tr>';
			echo '<tr><th>Current Difficulty</th><td>' . $difficulty . '</td></tr>';
		}
		else {
			echo '<tr><th>Block End</th><td>' . $round['heightEnd'] . ', ' . gmdate("Y-m-d H:i:s", $timeEnd) . '</td></tr>';
			echo '<tr><th>Duration</th><td>' . formatDuration($timeEnd - $timeStart) . '</td></tr>';
			if ($state === 0 || $state === 1) {
				echo '<tr><th>Block Hash</th><td>' . $round['blockHash'] . '</td></tr>';
				$reward = $coinbase['vout'][0]['value'];
				echo '<tr><th>Difficulty</th><td>' . $block['difficulty'] . '</td></tr>';
				echo '<tr><th>Reward</th><td>' . sprintf('%.8f', $reward) . ' RIC</td></tr>';
				echo '<tr><th>State</th>';
				if ($state == 0)
					echo '<td style="background: rgba(255, 128, 0, 0.25)">Unconfirmed';
				else if ($state == 1)
					echo '<td style="background: rgba(0, 255, 0, 0.25)">Confirmed';
				echo '</td></tr>';
				echo '<tr><th>Finder</th><td>' . $finder . '</td></tr>';
			}
			else if ($state == 2)
				echo '<tr><th>State</th><td style="background: rgba(255, 0, 0, 0.25)">Orphaned</td></tr>';
			else
				echo '<tr><th>State</th><td style="background: rgba(255, 128, 0, 0.25)">Manual intervention required</td></tr>';
		}
		echo '<tr><th>Miners</th><td>' . count($scores) . '</td></tr>';
		echo '</table>';
		
		echo '<h2>Participating Miners</h2>';
		
		echo '<table>';
		echo '<tr>';
		echo '<th>Miner</th>';
		echo '<th>Score</th>';
		echo '<th>%</th>';
		echo '</tr>';
		$scoreTotal = 0;
		foreach ($scores as &$scoreEntry)
			$scoreTotal += $scoreEntry['score'];
		foreach ($scores as &$scoreEntry) {
			echo '<tr style="text-align: right;">';
			$miner = $scoreEntry['miner'];
			if (ctype_digit($miner)) {
				try {
					$user = $stella->getUserById(intval($miner));
					if (isset($user['username']))
						$miner = $user['username'];
				}
				catch (Exception $e) {
					$miner = 'Deleted user';
				}
			}
			echo '<td>' . $miner . '</td>';
			echo '<td>' . $scoreEntry['score'] . '</td>';
			echo '<td>' . sprintf('%.3f', 100.*$scoreEntry['score']/$scoreTotal) . '</td>';
			echo '</tr>';
		}
		echo '<tr style="text-align: right; border-top: solid 2px;">';
		echo '<td>Total</td>';
		echo '<td>' . $scoreTotal . '</td>';
		echo '<td>' . sprintf('%.3f', 100.) . '</td>';
		echo '</tr>';
		echo '</table>';
	}
	catch (Exception $e) {
		htmlTop('Bad Round');
		echo '<p class="errorMessage">Could not get round data: ' . $e->getMessage() . '</p>';
	}
	htmlBottom();
}
else if ($page === 'register') {
	if ($stella->loggedIn()) {
		header('Location: index.php?p=account');
		exit;
	}
	$registered = false;
	if (isset($_POST['username'], $_POST['address'], $_POST['code'])) {
		$status = $stella->createUser($_POST['username'], $_POST['address'], $_POST['code']);
		if ($status === true)
			$registered = true;
	}
	htmlTop('Register');
	if ($registered === true)
		echo '<p class="successMessage">Registration successful, try to <a href="index.php?p=login">login</a> now!<p>';
	else {
		if (isset($status))
			echo '<p class="errorMessage">Registration failed: ' . $status . '</p>';
?>
		<form method="post">
			<table>
				<colgroup>
					<col span="1">
					<col span="1" style="min-width: 384px;">
					<col span="1">
				</colgroup>
				<tbody>
					<tr>
						<td>Username</td>
						<td><input name="username" value="<?php echo htmlspecialchars($_POST['username'] ?? '');?>"/></td>
						<td>Only letters a-z, A-Z and digits 0-9 are accepted, must be <?php echo minUsernameLength . '-' . maxUsernameLength;?> characters and start with a letter.</td>
					</tr>
					<tr>
						<td>Riecoin Address</td>
						<td><input name="address" value="<?php echo htmlspecialchars($_POST['address'] ?? '');?>"/> </td>
						<td>Only Bech32 ric1q... You must own this address and be able to generate codes with it.</td>
					</tr>
					<tr>
						<td>Code</td>
						<td><input name="code"/></td>
						<td>Use the Riecoin Core's Code Generator (version 22.02+)</td>
					</tr>
					<tr>
						<td colspan="2"><button>Register</button></td>
						<td>Create your account!</td>
					</tr>
				</tbody>
			</table>
		</form>
		
		<p>Until you connect for the first time, we are not using any cookie.</p>
		
		<h2>Security tips</h2>
		
		<ul>
			<li>You are responsible to keep your private keys secret;</li>
			<li>Never enter a code in places other than the site managing your account, always check the domain before entering your code;</li>
			<li>You should register different Riecoin Addresses if you create such accounts in several sites.</li>
		</ul>
<?php
	}
	htmlBottom();
}
else if ($page === 'login') {
	if ($stella->loggedIn()) {
		header('Location: index.php?p=account');
		exit;
	}
	if (isset($_POST['username'], $_POST['code'], $_POST['duration'])) {
		$status = $stella->login($_POST['username'], $_POST['code'], $_POST['duration']);
		if ($status === true) {
			header('Location: index.php?p=account');
			exit;
		}
	}
	htmlTop('Login');
	if (isset($status))
		echo '<p class="errorMessage">Login failed: ' . $status . '</p>';
?>
	<form method="post">
		<form method="get"><input type="hidden" name="p" value="round"/>
		<table>
			<colgroup>
				<col span="1">
				<col span="1" style="min-width: 384px;">
			</colgroup>
			<tbody>
				<tr>
					<td>Username</td>
					<td><input name="username" value="<?php echo htmlspecialchars($_POST['username'] ?? "");?>"/></td>
				</tr>
				<tr>
					<td>Code</td>
					<td><input name="code"/></td>
				</tr>
				<tr>
					<td>Session duration</td>
					<td>
						<select name="duration" style="width: 384px;">
							<option value="3600">1 hour</option>
							<option value="86400">1 day</option>
							<option value="604800">7 days</option>
							<option value="2592000">30 days</option>
							<option value="31556952">1 year</option>
						</select>
					</td>
				</tr>
				<tr>
					<td colspan="2" align="center"><button>Login</button></td>
				</tr>
			</tbody>
		</table>
	</form>
	
	<p>By logging in, you accept that we store a single cookie, containing a unique token that allows the server to remember you.</p>
<?php
	htmlBottom();
}
else if ($page === 'logout') {
	if ($stella->logout() === true) {
		header('Location: index.php');
		exit;
	}
	else {
		htmlTop('Error');
		echo '<p class="errorMessage">Something went wrong while logging out, please try again</p>';
		htmlBottom();
	}
}
else if ($page === 'account') {
	if (!$stella->loggedIn()) {
		header('Location: index.php?p=login');
		exit;
	}
	$user = $stella->getCurrentUser();
	if (($user['id'] ?? null) !== $stella->getCurrentUserId()) {
		htmlTop('Error');
		echo '<p class="errorMessage">Something went wrong while getting the current user, please try again</p>';
		htmlBottom();
		return;
	}
	htmlTop($user['username']);
	if (isset($_POST['accountAction'])) {
		$accountAction = $_POST['accountAction'];
		if ($accountAction === 'addAddress' && isset($_POST['address'], $_POST['code'])) {
			$status = $stella->addAddress($_POST['address'], $_POST['code']);
			if ($status !== true)
				echo '<p class="errorMessage">Could not add address: ' . $status . '</p>';
			else
				echo '<p class="successMessage">Successfully added new address</p>';
		}
		if ($accountAction === 'removeAddress' && isset($_POST['address'])) {
			$status = $stella->removeAddress($_POST['address']);
			if ($status !== true)
				echo '<p class="errorMessage">Could not remove address: ' . $status . '</p>';
			else
				echo '<p class="successMessage">Successfully removed address</p>';
		}
		if ($accountAction === 'createToken' && isset($_POST['expiration'], $_POST['powcLimit'], $_POST['name'], $_POST['code'])) {
			$status = $stella->createToken($_POST['expiration'], $_POST['powcLimit'], $_POST['name'], $_POST['code']);
			if (!ctype_xdigit($status) || strlen($status) != 64)
				echo '<p class="errorMessage">Could not create token: ' . $status . '</p>';
			else
				echo '<p class="successMessage">Successfully created new token ' . $status . '</p>';
		}
		if ($accountAction === 'deleteToken' && isset($_POST['tokenId'])) {
			$status = $stella->deleteToken($_POST['tokenId']);
			if ($status !== true)
				echo '<p class="errorMessage">Could not delete token: ' . $status . '</p>';
			else
				echo '<p class="successMessage">Successfully removed token</p>';
		}
		if ($accountAction === 'moveBalanceFromAddress' && isset($_POST['address'])) {
			$status = $stella->moveBalanceFromAddress($_POST['address']);
			if ($status !== true)
				echo '<p class="errorMessage">Could not move Riecoins: ' . $status . '</p>';
			else
				echo '<p class="successMessage">Successfully moved balance</p>';
		}
		if ($accountAction === 'withdraw' && isset($_POST['address'], $_POST['amount'], $_POST['code'])) {
			$status = $stella->withdraw($_POST['amount'], $_POST['address'], $_POST['code']);
			if ($status !== true)
				echo '<p class="errorMessage">Could not withdraw Riecoins: ' . $status . '</p>';
			else
				echo '<p class="successMessage">Withdrawal successful</p>';
		}
		if ($accountAction === 'deleteAccount') {
			$status = $stella->deleteUser($_POST['code']);
			if ($status !== true)
				echo '<p class="errorMessage">Account deletion failed: ' . $status . '</p>';
			else {
				echo '<p class="successMessage">Deletion successful</p>';
				htmlBottom();
				return;
			}
		}
	}
	
	echo '<p>Registered on ' . gmdate('Y-m-d H:i:s', $user['registrationTime']) . ', user Id ' . $user['id'] . '</p>';
	echo '<p>Actions will be carried out directly without asking for a confirmation, so read properly the explanations and think twice before clicking on a button.</p>';
	
	try {
		$addresses = $stella->getAddresses($user['id']);
		$nAddresses = count($addresses);
		echo '<h2>Registered Riecoin Addresses (' . $nAddresses . '/' . maxAddresses . ')</h2>';
		
		echo '<p>You cannot delete the registered Riecoin Address if it is the only one, but can add a new address then delete the old one. Deleting an address will not clear its associated balance, but you will no longer have access to it in this page (you can regain access if you register the address again).</p>';
		
		echo '<table>';
		echo '<tr><th>Riecoin Address</th><th>Action</th></tr>';
		foreach ($addresses as &$address) {
			echo '<tr><td>' . $address . '</td><td>' . ($nAddresses > 1 ? '<form method="post"><input type="hidden" name="accountAction" value="removeAddress"><input type="hidden" name="address" value="' . $address . '"><button>Delete</button></form>' : ''). '</td></tr>';
		}
		echo '</table>';
		
		if (count($addresses) < maxAddresses) {
?>
			<h3>Register New Riecoin Address</h3>
			<p>You can register up to 8 Riecoin addresses.</p>
			<form method="post"><input type="hidden" name="accountAction" value="addAddress"/>
			<table>
				<tr>
					<td>Riecoin Address</td>
					<td><input name="address" style="width: 384px;"/></td>
				</tr>
				<tr>
					<td>Code for this new address</td>
					<td><input name="code" style="width: 384px;"/></td>
				</tr>
				<tr>
					<td colspan="2" align="center"><button>Add New Address</button></td>
				</tr>
			</table>
		</form>
<?php
		}
	}
	catch (Exception $e) {
		echo '<p class="errorMessage">Could not get the Riecoin addresses, please retry - ' . $e->getMessage() . '</p>';
	}
	
	try {
		$tokens = $stella->getTokens();
		$nTokens = count($tokens);
		echo '<h2>Authorization Tokens (' . $nTokens . '/' . maxTokens . ')</h2>';
		
		echo '<p>You can create tokens in order to authorize services. Important:</p>';
		
		echo '<ul>';
		echo '<li>The token will be shown only once, note it somewhere safe;</li>';
		echo '<li>Only give a token to services that you trust. They will be able to freely consume your PoWc within your limits and access to general account information (username, registered Riecoin addresses, PoWc remaining, blocks found);</li>';
		echo '<li>Consumed PoWc cannot be restored. We recommend to put a PoWc limit.</li>';
		echo '</ul>';
		
		echo '<table>';
		echo '<tr><th>Name</th><th>Expiration</th><th>PoWc remaining</th><th>Actions</th></tr>';
		foreach ($tokens as &$token) {
			echo '<tr><td>' . htmlspecialchars($token['name']) . '</td><td>' . gmdate("Y-m-d H:i:s", $token['expiration']) . '</td><td>' . ($token['powcRemaining'] >= 1e19 ? 'No Limit' : sprintf('%.6f', $token['powcRemaining']) . ' PoWc') . '</td><td><form method="post"><input type="hidden" name="accountAction" value="deleteToken"><input type="hidden" name="tokenId" value="' . $token['id'] . '"><button>Delete</button></form></td></tr>';
		}
		echo '</table>';
		
		if ($nTokens < maxTokens) { ?>
			<h3>Create New Authorization Token</h3>
			<p>You can have up to <?php echo maxTokens ?> tokens at any time.</p>
			<form method="post"><input type="hidden" name="accountAction" value="createToken"/>
			<table>
				<tr>
					<td>PoWc Limit</td>
					<td>
						<select name="powcLimit" style="width: 384px;">
							<option value="0">0</option>
							<option value="0.1">0.1</option>
							<option value="0.2">0.2</option>
							<option value="0.5">0.5</option>
							<option value="1">1</option>
							<option value="2">2</option>
							<option value="5">5</option>
							<option value="10">10</option>
							<option value="1e38">None</option>
						</select>
					</td>
				</tr>
				<tr>
					<td>Expires in</td>
					<td>
						<select name="expiration" style="width: 384px;">
							<option value="86400">1 day</option>
							<option value="604800">7 days</option>
							<option value="2592000">30 days</option>
							<option value="31556952">1 year</option>
							<option value="157784760">5 years</option>
						</select>
					</td>
				</tr>
				<tr>
					<td>Name</td>
					<td><input name="name" style="width: 384px;"/></td>
				</tr>
				<tr>
					<td>Code</td>
					<td><input name="code" style="width: 384px;"/></td>
				</tr>
				<tr>
					<td colspan="2" align="center"><button>Create Token</button></td>
				</tr>
			</table>
		</form>
<?php
		}
	}
	catch (Exception $e) {
		echo '<p class="errorMessage">Could not get the tokens, please retry - ' . $e->getMessage() . '</p>';
	}
	
	try {
		$powCredits = $stella->getPowCredits($user['id']);
		$powcMovements = $stella->getPowCreditsMovements();
		echo '<h2>PoW Credits</h2>';
		echo '<p>You have ' . sprintf('%.6f', $powCredits['total']) . ' PoWc.</p>';
		echo '<h3>Latest PoWc Movements</h3>';
		echo '<table>';
		echo '<tr><th>Id</th><th>Time</th><th>Movement</th><th>Comment</th></tr>';
		$i = 0;
		while ($movement = $powcMovements->fetch()) {
			if ($i < 10)
				echo '<tr><td>' . $movement['id'] . '</td><td>' . gmdate("Y-m-d H:i:s", $movement['time']) . '</td><td>' . sprintf('%+.6f', $movement['value']) . ' PoWc</td><td>' . htmlspecialchars($movement['comment']) . '</td></tr>';
			$i++;
		}
		echo '</table>';
	}
	catch (Exception $e) {
		echo '<p class="errorMessage">Could not get the PoWc, please retry - ' . $e->getMessage() . '</p>';
	}
	
	try {
		$balance = $stella->getBalance();
		$ricTransactions = $stella->getRiecoinTransactions();
		$balanceAddresses = array();
		foreach ($addresses as &$address) {
			$balanceAddress = $stella->getBalanceAddress($address);
			if ($balanceAddress > 0)
				$balanceAddresses[$address] = $balanceAddress;
		}
		echo '<h2>Balances</h2>';
?>
		<p>There are different balances, one associated to your account, and those associated to your addresses. In your miner's configuration, use your username to credit your account balance, or a registered address to credit the balance associated to it.</p>
		
		<h3>Withdraw</h3>
		
		<p>You can only withdraw funds from your account, Move funds from addresses to your account first if desired. Notes:</p>
		<ul>
			<li>Minimum <?php echo $stella->option('WithdrawalMinimum');?> RIC;</li>
			<li>Fee of <?php echo $stella->option('WithdrawalFee');?> RIC, so if you withdraw 100 RIC, the destination will get <?php echo 100. - $stella->option('WithdrawalFee');?> RIC;</li>
			<li>If you withdraw everything, the account transaction history will be cleared.</li>
		</ul>
		
		<form method="post"><input type="hidden" name="accountAction" value="withdraw"/>
			<table>
				<tr>
					<td>Account Balance</td>
					<td><?php echo sprintf('%.8f', $balance) . ' RIC' ?></td>
				</tr>
				<tr>
					<td>Destination Address</td>
					<td><input name="address" style="width: 384px;"/></td>
				</tr>
				<tr>
					<td>Amount</td>
					<td><input name="amount" style="width: 256px;"/> RIC</td>
				</tr>
				<tr>
					<td>Code</td>
					<td><input name="code" style="width: 384px;"/></td>
				</tr>
				<tr>
					<td colspan="2" align="center"><button>Withdraw</button></td>
				</tr>
			</table>
		</form>
		<p>Address Balances: if a balance reaches <?php echo $stella->option('WithdrawalAutomaticThreshold');?> RIC, there will be an automatic withdrawal to the address. You cannot change this behavior.</p>
<?php
		echo '<table>';
		foreach ($balanceAddresses as $address => &$balance) {
			echo '<tr><td>' . $address . '</td><td style="text-align: right;">' . sprintf('%.8f', $balance) . ' RIC</td><td><form method="post"><input type="hidden" name="accountAction" value="moveBalanceFromAddress"><input type="hidden" name="address" value="' . $address . '"><button>Move to account</button></form></td></tr>';
		}
		echo '</table>';
		
		echo '<h3>Latest Account Transactions</h3>';
		
		echo '<table>';
		echo '<tr><th>Id</th><th>Time</th><th>Movement</th><th>Comment</th></tr>';
		$i = 0;
		while ($transaction = $ricTransactions->fetch()) {
			echo '<tr><td>' . $transaction['id'] . '</td><td>' . gmdate("Y-m-d H:i:s", $transaction['time']) . '</td><td>' . sprintf('%+.8f', $transaction['value']) . ' RIC</td><td>' . htmlspecialchars($transaction['comment']) . '</td></tr>';
			$balance += $transaction['value'];
			$i++;
			if ($i >= 10)
				break;
		}
		echo '</table>';
	}
	catch (Exception $e) {
		echo '<p class="errorMessage">Could not get the balances, please retry - ' . $e->getMessage() . '</p>';
	}
	
	echo '<h3>Latest Withdrawals</h3>';
	
	echo '<table>';
	echo '<tr><th>Id</th><th>Time</th><th>Amount</th><th>To</th><th>State/Transaction Id</th><th>Comment</th></tr>';
	$i = 0;
	$withdrawals = $stella->getWithdrawals();
	while ($withdrawal = $withdrawals->fetch()) {
		echo '<tr><td>' . $withdrawal['id'] . '</td><td>' . gmdate("Y-m-d H:i:s", $withdrawal['time']) . '</td><td>' . sprintf('%.8f', $withdrawal['amount']) . ' RIC</td><td>' . $withdrawal['address'] . '</td><td>';
		$status = $withdrawal['state'];
		if ($status == 0)
			echo 'Pending';
		else if ($status == 1)
			echo $withdrawal['txid'];
		else
			echo 'Must be processed manually, please wait';
		echo '</td><td>' . htmlspecialchars($withdrawal['comment']) . '</td></tr>';
		$i++;
		if ($i >= 10)
			break;
	}
	echo '</table>';
?>
	<h3>Delete Account</h3>
	
	<p>This cannot be undone! If you delete your account, the balance and PoW Credits associated to it will be lost.</p>
	
	<form method="post"><input type="hidden" name="accountAction" value="deleteAccount"/>
		<table>
			<tr>
				<td>Code</td>
				<td><input name="code" style="width: 384px;"/></td>
			</tr>
			<tr>
				<td colspan="2" align="center"><button>Delete Account</button></td>
			</tr>
		</table>
	</form>
<?php
	htmlBottom();
}
else {
	htmlTop('Main Page');
	echo '<p>Welcome to our pool! Lorem Ipsum Bla Bla Bla</p>';
	echo '<p>Useful information about pooled mining (definition of a share, earning calculations,...) can be found <a href="https://riecoin.dev/en/Stella">here</a>.</p>';
	echo '<p>Be sure to read the README.md. Also play with the interface to better undestand how things work.</p>';
	echo '<p>It is now your job to make the pool work and look the way you want.</p>';
	
	echo '<h2>How to mine</h2>';
	
	echo "<p>Miners can choose to mine with an account or anonymously with a Riecoin address. In the latter case, the address will still be shown in the rounds' data (as a finder or participating miner). Earnings will be associated to the address. The balance will regularly be checked, and if it exceeds " . $stella->option('WithdrawalAutomaticThreshold') . " RIC, there will be an automatic payout. The advantage of mining with an account is that you get PoW Credits that can be consumed to use some services or as captcha alternative.</p>";
	
	echo '<p>Sample rieMiner configuration:</p>';
	echo "<pre>Mode = Pool\nHost = " . $domain . "\nPort = " . $stella->option('PoolPort') . "\nUsername = username or Riecoin address\nPassword = Hex Token, needed only if using an account</pre>";
	echo '<p><a href="https://riecoin.dev/en/rieMiner">rieMiner\'s page</a> (<a href="https://riecoin.dev/en/rieMiner/Pooled_Mining">pooled mining guide</a>, <a href="https://riecoin.dev/en/rieMiner/Benchmarks">benchmarks</a>)</p>';
	
	echo '<h2>Fees</h2>';
	
	echo '<ul>';
	echo '<li>' . 100.*$stella->option('PoolFee') . '% Pool Fee</li>';
	echo '<li>' . $stella->option('WithdrawalFee') . ' RIC per withdrawal</li>';
	echo '</ul>';
	
	echo '<h2>API</h2>';
	
	echo '<p>A basic API is provided in <a href="api.php">api.php</a> to provide general information about the Pool and use the PoWc. Use supported methods via POST and get a JSON reply:</p>';
	
	echo '<ul>';
	echo '<li>getPoolInfo(), also the default method if nothing was sent via POST: useful for services tracking Riecoin Pools, get general pool info like the Mining Power, how many blocks that have been found,...</li>';
	echo '<li>getPoWInfo(token): get PoW information of a user that gave a valid token: username, PoWc remaining, registered addresses, hash of blocks found during the last 7 days;</li>';
	echo '<li>consumePoWc(token, amount, comment): consume PoWc of a user that gave a valid token.</li>';
	echo '</ul>';
	
	echo '<p>Results are given in a "result" field (null if something went wrong) and errors in an "error" field (null if there were no issue).</p>';
	
	echo '<div style="display: flex;"><div>';
	echo '<h2>Recent Statistics</h2>';
	try {
		$recentStats = $stella->getRecentStatistics();
		echo '<table>';
		echo '<tr><th>Active Miners</th><td>' . $recentStats['activeMiners'] . '</td></tr>';
		echo '<tr><th>Valid Shares</th><td>' . $recentStats['validShares'] . '</td></tr>';
		echo '<tr><th>Invalid Shares</th><td>' . $recentStats['invalidShares'] . '</td></tr>';
		if ($recentStats['validShares'] + $recentStats['invalidShares'] > 0)
			echo '<tr><th>% Valid</th><td>' . sprintf('%.3f', 100.*($recentStats['validShares']/($recentStats['validShares'] + $recentStats['invalidShares']))) . '</td></tr>';
		echo '<tr><th>Shares/s</th><td>' . sprintf('%.6f', $recentStats['validShares']/3600) . '</td></tr>';
		echo '</table>';
	}
	catch (Exception $e) {
		echo '<p class="errorMessage">Could not get recent statistics, please retry later: ' . $e->getMessage() . '</p>';
	}
	echo '</div><div style="padding-left: 32px;">';
	echo '<h2>Riecoin Node Status</h2>';
	try {
		$blockchainInfo = $stella->getBlockchainInfo();
		echo '<table>';
		echo '<tr><th>Chain</th><td>' . $blockchainInfo['chain'] . '</td></tr>';
		echo '<tr><th>Last Block</th><td>' . $blockchainInfo['blocks'] . '</td></tr>';
		echo '<tr><th>Difficulty</th><td>' . $blockchainInfo['difficulty'] . '</td></tr>';
		echo '<tr><th>Mining Power</th><td>' . sprintf('%.3f', $blockchainInfo['miningpower']) . '</td></tr>';
		echo '<tr><th>Connections</th><td>' . $blockchainInfo['connections'] . '</td></tr>';
		echo '</table>';
	}
	catch (Exception $e) {
		echo '<p class="errorMessage">The pool currently has a connection issue with the Riecoin network, please retry later: ' . $e->getMessage() . '</p>';
	}
	echo '</div></div>';
	
	echo '<h2>Current Round</h2>';
	try {
		$roundId = $stella->getCurrentRoundId();
		if ($roundId == 0)
			echo '<p>No round found.</p>';
		else {
			$round = $stella->getRound($roundId);
			$timeStart = $round['timeStart'];
			$timeEnd = empty($round["timeEnd"]) ? time() : $round["timeEnd"];
			$reward = 0;
			echo '<table>';
			echo '<tr><th>Id</th><td><a href="index.php?p=round&id=' . $round['id'] . '">' . $round['id'] . '</a></td></tr>';
			echo '<tr><th>Since Block</th><td>' . $round['heightStart'] . ', ' . gmdate("Y-m-d H:i:s", $round["timeStart"]) . '</td></tr>';
			$blockTemplate = $stella->riecoinRPC()->getblocktemplate(array('rules' => array('segwit')));
			$reward = $blockTemplate['coinbasevalue'] ?? 0;
			$difficulty = hexdec($blockTemplate['bits'] ?? 0)/256;
			echo '<tr><th>Current Block</th><td>' . $blockTemplate['height'] . '</td></tr>';
			echo '<tr><th>Since</th><td>' . formatDuration($timeEnd - $timeStart) . '</td></tr>';
			echo '<tr><th>Current reward</th><td>' . sprintf('%.8f', $reward/1e8) . ' RIC</td></tr>';
			echo '<tr><th>Current difficulty</th><td>' . $difficulty . '</td></tr>';
			echo '</table>';
		}
	}
	catch (Exception $e) {
		echo '<p class="errorMessage">Could not get round data, please retry later: ' . $e->getMessage() . '</p>';
	}
	htmlBottom();
}
?>
