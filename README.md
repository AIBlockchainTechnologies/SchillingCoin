Schilling Coin (SCH) Integration/Staging Repository
===================================================


Quick installation of the Schilling Coin daemon under Linux Ubuntu 20.04.06 LTS.

Installation of libraries (using root user):

    add-apt-repository ppa:bitcoin/bitcoin -y
    apt-get update
    apt-get install -y build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils
    apt-get install -y libboost-system-dev libboost-filesystem-dev libboost-chrono-dev libboost-program-options-dev libboost-test-dev libboost-thread-dev
    apt-get install -y libdb4.8-dev libdb4.8++-dev

Cloning the repository and compiling (use any user with the sudo group):

    cd
    git clone https://github.com/AIBlockchainTechnologies/SchillingCoin
    cd SCH
    ./autogen.sh
    ./configure
    sudo make install
    cd src
    sudo strip schillingcoind
    sudo strip schillingcoin-cli
    sudo strip schillingcoin-tx
    cd ..

Running the daemon:

    schillingcoind 

Stopping the daemon:

    schillingcoin-cli stop

Demon status:

    schillingcoin-cli getinfo
    schillingcoin-cli mnsync status


Current Coin Specifications

<table>
<tr><td>Algo</td><td>Quark</td></tr>
<tr><td>Block Time</td><td>60 Seconds</td></tr>
<tr><td>Max Coin Supply</td><td>200,000,000 SCH</td></tr>
<tr><td>Premine</td><td>NONE</td></tr>
<tr><td>Maturity</td><td>50</td></tr>
<tr><td>Port</td><td>9070</td></tr>
<tr><td>RPC Port</td><td>9071</td></tr>
<tr><td>Reward Per Block</td><td>32.3 SCH</td></tr>
<tr><td>Reward Split</td><td>80% (MN) / 20% (POS)</td></tr>
</table>

Historical/Current Masternode Collateral

<table>
<thead>
<tr><th>Block Height</th><th>Collateral</th></tr>
</thead>
<tbody>
<tr><td>0–260000</td><td>40,000 SCH</td></tr>
<tr><td>260001–520000</td><td>60,000 SCH</td></tr>
<tr><td>520001–780000</td><td>80,000 SCH</td></tr>
<tr><td>780001–1,040,000</td><td>90,000 SCH</td></tr>
<tr><td>1,040,001–(transition)</td><td>100,000 SCH</td></tr>
<tr><td>(post‑transition)</td><td>1 SCH (permanent; collateral retained only for MasterSeedNode identity)</td></tr>
</tbody>
</table>

<p><strong>SCH Network Transition Notice</strong></p>
<ul>
<li><strong>Final consensus model</strong> — Schilling Coin (SCH) will transition to a full 100% Proof of Stake (PoS) reward model.</li>
<li><strong>Masternode Rewards Removed</strong> — After the transition, <strong>0% of block rewards</strong> will be paid to masternodes; masternode reward payments will be permanently removed forever.</li>
<li><strong>Permanent Collateral Change</strong> — Following the transition, masternode collateral will be set to <strong>1 SCH forever</strong>. This minimal collateral preserves a unique on‑chain identity but no longer confers any reward entitlement.</li>
<li><strong>Repurposing: MasterSeedNodes</strong> — Masternodes will be repurposed as <strong>MasterSeedNodes</strong>. Their role will be limited to acting as reliable network seed nodes (peer discovery and bootstrap assistance). They will not receive block rewards or perform reward‑driven consensus functions.</li>
<li><strong>Activation block</strong> — The exact block height for the PoS transition, collateral change, and masternode reward removal will be announced in an official communication prior to activation.</li>
<li><strong>Collateral &amp; rewards (historical)</strong> — The table above preserves historical collateral tiers for reference. After activation, those tiers are deprecated and replaced by the permanent 1 SCH collateral and the MasterSeedNode role.</li>
<li><strong>Operational Notes</strong> — Existing masternode operators should expect an official migration plan and tooling guidance before the activation block; no operator action is required until the project publishes the transition instructions.</li>
</ul>

⚠️ Critical SCH DataDir Warning (Cross‑OS Safety Notice)

Never copy an SCH **DataDir** between different operating system platforms (e.g., Linux ↔ Windows). DataDir folders created on **Linux (ext4)** rely on metadata, sparse file structures, atomic write semantics, and LevelDB/BerkeleyDB behaviors that **Windows filesystems (NTFS/FAT/exFAT)** cannot preserve.

Copying a Linux‑generated DataDir onto Windows will destroy these attributes and cause:

- LevelDB corruption
- BerkeleyDB lockfile failure
- blk*.dat sparse file collapse
- peers.dat / mncache.dat / mnpayments.dat corruption
- chainstate checksum mismatches

This corruption is **filesystem‑related**, not wallet‑related, and does **NOT** indicate any flaw in the SCH client or consensus logic.

### Safe Transfer Methods (If You Absolutely Must)

- Compress the DataDir on Linux first (**tar.gz recommended**)
- OR use an **ext4‑formatted device**

Even then, SCH developers **strongly advise AGAINST** transferring DataDir files across operating systems. If you attempt it anyway, make **multiple verified backups** beforehand.

**NEVER drag‑and‑drop a Linux DataDir onto a Windows filesystem or vice versa.**

This behavior is well documented and consistent with industry standards across all blockchains. SCH developers are **not responsible** for any SCH funds lost or misplaced due to user error. All SCH software is used **at your own risk**.

More information at [SchillingCoin.org](http://www.schillingcoin.org/)
---
Distributed under the MIT software license, see the accompanying file COPYING or http://www.opensource.org/licenses/mit-license.php.