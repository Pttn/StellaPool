# StellaPool

StellaPool provides a Riecoin mining pool, including basic account support and a "PoW Credits" system, explained below. It uses the Stratum protocol.

If you wish to operate a mining pool, you should already have some experience with cryptocurrency related software, databases, good system administration knowledge, and some programming abilities. Existing pool operators in our Riecoin discussion channels might help you, but if you get stuck at every step or error message, then you are certainly not ready to operate a pool.

StellaPool has been designed as a starting point for pool operators, and we encourage them to modify it heavily to make it the way they want. It is not meant to be used as a reference code, though operators may still look once a while at the commits, especially if there are changes in order to support new Riecoin Core releases. Suggestions on the Riecoin Forum are also welcomed.

## Components

The code is divided into two components.

### Backend

In the Pool folder, you will find C++ code for a backend responsible for allowing connections from miners, handling submitted shares, distributing earnings and processing payouts.

A ban system against misbehaving miners (too many invalid shares,...) is integrated.

### Web Interface

In the Web folder, you will find a PHP frontend code. It is a web interface displaying useful information like the Riecoin network status or round statistics. Accounts can also be created and managed, and withdrawals can be requested.

No administration panel is provided, you should be able to use something like PhpMyAdmin for manual interventions.

The interface is minimalist and it is your job to make it look like the way you want.

The backend must have been ran at least once (but no restart needed) before accessing the Web Interface.

## Accounts and PoW Credits

Accounts are created using the Riecoin Core's Code Generator or compatible, allowing registration without any personal information like an email address or a password.

PoW Credits (PoWc) are points distributed to account owners, in accordance with the mining work provided, with the reference 1 PoWc = 1 Riecoin Block. They don't have any monetary value, but quantify the computational work recently done by a given user. The PoWc cannot be transmitted and are valid only 7 days, but may be "consumed" by services either provided by the pool or external ones that trust it.

Besides a system rewarding active miners if the pool provides services consuming PoWc, the Credits are an interesting Captcha replacement given that it is difficult to get them, deterring spammers by reducing considerably their efficiency and preventing them from selling large amount of PoWc due to their quick expiration. Start integrating the PoWc system in your services and make the Internet a better place!

## Preparation

### Configure Riecoin Core

First, configure Riecoin Core using the `riecoin.conf` file. Here is a basic template:

```
daemon=1
server=1
txindex=1

rpcuser=(choose an username)
rpcpassword=(choose a password)

[main]
rpcport=28332
port=28333
rpcbind=127.0.0.1

[test]
rpcport=38332
port=38333
rpcbind=127.0.0.1
```

Once you are ready, start Riecoin Core (Mainnet, Testnet, or both, depending on your goal). If needed, create a new wallet and generate an address where block rewards will be sent before being redistributed to miners. Of course, make sure that the synchronization is done.

If you wish to support both Mainnet and Testnet, you must create two separate databases and run two separate Backends and Web Interfaces (or mod heavily the code).

### Create a MySQL/MariaDB database and a user

Set up a MySQL or MariaDB server and create a database and optionally a user for the pool.

Your host provider might already provide a database server, or you can setup one by yourself. In the latter case, there are plenty of tutorials explaining how to do this.

## Compile the backend

You must have a recent enough Linux and an appropriate compiler with C++20 support. Other and old operating systems are not supported.

### On Debian/Ubuntu

You can get the source code with Git and compile this C++ program with g++ and make, install them if needed. Then, get if needed the following dependencies:

* [Curl](https://curl.haxx.se/)
* [GMP](https://gmplib.org/)
* [MySQL C++ Connector](https://dev.mysql.com/doc/connector-cpp/8.0/en/)
* [NLohmann Json](https://json.nlohmann.me/)

On Debian 11, you can easily install these by doing as root:

```bash
apt install g++ make git libcurl4-openssl-dev libgmp-dev libmysqlcppconn-dev nlohmann-json3-dev
```

Then, download the source files, go/`cd` to the directory, and run `make`:

```bash
git clone https://github.com/Pttn/StellaPool.git
cd StellaPool
cd Pool
make
```

For other Linux, executing equivalent commands (using `pacman` instead of `apt`,...) should work.

## Configure the Pool

StellaPool uses a text configuration file, by default a `Pool.conf` file next to the backend executable. It is also possible to use custom paths, examples:

```bash
./StellaPool config/example.txt
./StellaPool "config 2.conf"
./StellaPool /home/user/Pool/Pool.conf
```

In the web PHP code, change the relevant lines in the in the top of the `Stella.php` file accordingly, in particular make sure that the frontend uses the same configuration file as the backend.

Each option is set by a line like

```
Option = Value
```

It is case sensitive. A line starting with `#` will be ignored, as well as invalid ones. Spaces or tabs just before or after `=` are also trimmed. If an option is missing, the default value(s) will be used. If there are duplicate lines for the same option, the last one will be used.

### Pool Settings

* `PoolAddress`: the Block Rewards are generated with this Riecoin address. You can use Bech32 "ric1" addresses (only lowercase). Default: a donation address;
* `PoolPort`: the port the miners connect and send shares to. Default: 2005;
* `PoolFee`: the part of the Block Rewards that is kept by the Pool. For example, 0.01 will mean that 1% of the rewards will not be distributed to miners. Default: 0.01;
* `PoolRequiredConfirmations`: the number of confirmations required for blocks found by the pool, before crediting miners. Note that 100 confirmations are needed by the Riecoin protocol in order to be able to spend block rewards, so you should have reserve funds if you require less confirmations. Default: 100;
* `WithdrawalMinimum`: the minimum amount in RIC for manual withdrawals via the Web interface. Default: 0.5;
* `WithdrawalFee`: the fee in RIC for every withdrawal. Default: 0.01;
* `WithdrawalAutomaticThreshold`: anonymous balances that reach this amount of RIC will trigger an automatic withdrawal. Default: 5;
* `WithdrawalProcessingInterval`: how often in s to check for and process withdrawals if any (automatic withdrawal generation and coin sending). Default: 60.

### Riecoin Daemon/Wallet Settings

* `WalletHost`: the IP of the Riecoin server. Default: 127.0.0.1;
* `WalletPort`: the port of the Riecoin server (same as rpcport in riecoin.conf). Default: 28332 (default RPC port for Riecoin Core);
* `WalletName`: the name of the wallet, set this if you created multiple wallets in Riecoin Core. Default: empty;
* `WalletUsername`: the username used to connect to the Riecoin server (same as rpcuser in riecoin.conf). Default: empty;
* `WalletPassword`: the password used to connect to the Riecoin server (same as rpcpassword in riecoin.conf). Default: empty.

### Database Settings

* `DatabaseHost`: the IP of the database server. Default: 127.0.0.1;
* `DatabasePort`: the port of the database server. Default: 3306 (default MySQL Port);
* `DatabaseName`: the database name. Default: Stella;
* `DatabaseUsername`: the database username. Default: empty;
* `DatabasePassword`: the database password. Default: empty;
* `DatabaseUpdateInterval`: how often in s to update the database (store share scores,...). Default: 10.

## Developers and license

* [Pttn](https://github.com/Pttn), author and maintainer. You can reach me on the [Riecoin Forum](https://forum.riecoin.dev/).

This work is released under the MIT license.

### Versioning

The version naming scheme is 0.9, 0.99, 0.999 and so on for major versions, analogous to 1.0, 2.0, 3.0,.... The first non 9 decimal digit is minor, etc. For example, the version 0.9925a can be thought as 2.2.5a. A perfect bug-free software will be version 1. No precise criteria have been decided about incrementing major or minor versions for now.

## Contributing

You are encouraged to discuss about improvements on the Riecoin Forum and make useful Pull Requests.

By contributing to StellaPool, you accept to place your code under the MIT license.

Donations welcome:

* Riecoin: ric1qpttn5u8u9470za84kt4y0lzz4zllzm4pyzhuge
* Bitcoin: bc1qpttn5u8u9470za84kt4y0lzz4zllzm4pwvel4c

### Quick contributor's checklist

* Code that add support for non Linux systems or older versions of distributions or PHP will not be merged;
* Your code must compile and work on recent Debian based distributions;
* StellaPool must work with any realistic setting;
* Ensure that your changes did not break anything;
* Follow the style of the rest of the code (curly braces position, camelCase variable names, tabs and not spaces, spaces around + and - but not around * and /,...).
  * Avoid using old C style and prefer modern C++ code;
  * Prefer longer and explicit variable names (except for loops indexes where single letter variables should be used in most cases).

## Resources

* [Riecoin website](https://Riecoin.dev/)
  * [StellaPool's page](https://riecoin.dev/en/StellaPool)
* [Bitcoin Wiki - Stratum](https://en.bitcoin.it/wiki/Stratum_mining_protocol)
