Namecoin
===================

Namecoin is a decentralized key/value registration and transfer system based on Bitcoin technology (a decentralized cryptocurrency).

It can be used for multiple purposes, for example a secure and censorship resistant replacement for DNS.

Ownership of a name is based on ownership of a coin, which is in turn based on public key cryptography.  The namecoin network reaches consensus every few minutes as to which names have been reserved or updated.

There is a FAQ.md to answer some general questions.

See http://namecoin.info or http://dot-bit.org for more information.


Technical
=====================

The Bitcoin protocol is augmented with namecoin operations, to reserve, register and update names.  In addition to DNS like entries, arbitrary name/value pairs are allowed and multiple namespaces will be available.  This will include a personal handle namespace mapping handles to public keys and personal address data.

The protocol differences from bitcoin include:

* Different blockchain, port, IRC bootstrap and message header
* New transaction types: new, first-update, update
* Validation on the new transaction types
* RPC calls for managing names
* Network fees to slow down the initial rush

Please read DESIGN-namecoind.md for details.


BUILDING
======================

Building is supported on Linux, Windows and Mac. For building on windows you can use the scripts in ./contrib/easywinbuilder. Find build instructions on http://dot-bit.org.

## Building 'namecoind' on Ubuntu

The following instructions are for building a headless namecoin daemon (namecoind) on a **32-bit Ubuntu 13** and do **not** apply to 64-bit versions. First install the required packages:

```
sudo apt-get update
sudo apt-get install git build-essential libboost-dev libdb5.1++-dev
sudo apt-get install libboost-chrono-dev libboost-date-time-dev libboost-filesystem-dev libboost-program-options-dev
sudo apt-get install libboost-serialization-dev libboost-system-dev libboost-thread-dev libglibmm-2.4-dev
sudo apt-get install qt4-qmake libqt4-dev qt4-linguist-tools libssl-dev 
```
Next, checkout the github repo and the correct release: 

```
git clone https://github.com/namecoinq/namecoinq.git
cd namecoinq/
git checkout vQ.3.72
```

**Note:** You should only trust **version Q.3.72** or higher with names because of a severe bug that was discovered in Aug 2013.

Build namecoind:
```
qmake "USE_UPNP=-"
cd src
make
```
Configure namecoind:
```
touch ~/.namecoin/namecoin.conf
chmod 600 ~/.namecoin/namecoin.conf
```
Copy/paste the following into namecoin.conf after making the appropriate edits
```
rpcuser=<your_username>
rpcpassword=<your_long_secure_password>
rpcport=<the_port_you_want_to_use>
```
Now, you can run namecoind and verfiy that it is running:
```
cd namecoinq/src/
./namecoind
./namecoind getinfo 
```
