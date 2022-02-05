<?php // (c) 2022 Pttn (https://riecoin.dev/en/StellaPool)
require_once('Stella.php');
$stella = new Stella($confPath);

header('Content-Type: application/json; charset=utf-8');
try {
	$result = null;
	$method = $_POST['method'] ?? 'getPoolInfo';
	if ($method === 'getPoolInfo')
		$result = $stella->getPoolInfo();
	else if ($method === 'getPoWInfo') {
		if (empty($_POST['token']))
			throw new Exception('Missing token');
		$result = $stella->getPoWInfo($_POST['token']);
	}
	else if ($method === 'consumePoWc') {
		if (empty($_POST['token']))
			throw new Exception('Missing token');
		if (empty($_POST['amount']))
			throw new Exception('Missing amount');
		$comment = $_POST['comment'] ?? '';
		$result = $stella->consumePoWc($_POST['token'], $_POST['amount'], $comment);
	}
	else
		throw new Exception('Invalid or unsupported method');
	echo json_encode(array('result' => $result, 'error' => null));
}
catch (Exception $e) {
	echo json_encode(array('result' => null, 'error' => $e->getMessage()));
}
exit;
?>
