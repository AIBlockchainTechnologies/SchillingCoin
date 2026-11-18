Schilling Coin (SCH) Integration/Staging Repository
===================================================


Quick installation of the Schilling Coin daemon under Linux Ubuntu 20.04.06 LTS.

Installation of libraries (using root user):

    add-apt-repository ppa:schillingcoin/schillingcoin -y
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

<!-- Current Coin Specifications (updated) -->
<table>
  <tr><td><strong>Algo</strong></td><td>Quark</td></tr>
  <tr><td><strong>Block Time</strong></td><td>60 Seconds</td></tr>
  <tr><td><strong>Original Planned Maximum Supply</strong></td><td>200,000,000 SCH under the original SCH monetary model; the current SCH development plan replaces the terminal-supply reward model with continuing block-reward halvings.</td></tr>
  <tr><td><strong>Premine</strong></td><td>NONE</td></tr>
  <tr><td><strong>Maturity</strong></td><td>50</td></tr>
  <tr><td><strong>Port</strong></td><td>9070</td></tr>
  <tr><td><strong>RPC Port</strong></td><td>9071</td></tr>
  <tr><td><strong>Historical / Pre-Halving Base Block Reward</strong></td><td>32.29999999 SCH</td></tr>
  <tr><td><strong>First Planned Halving Height</strong></td><td>5,783,307</td></tr>
  <tr><td><strong>Base Block Reward From First Halving</strong></td><td>16.14999999 SCH</td></tr>
  <tr><td><strong>Long-Term Reward Policy</strong></td><td>Continuing block-reward halvings under the current SCH development plan rather than permanent termination of block rewards at the original 200,000,000 SCH issuance target.</td></tr>
  <tr><td><strong>Reward Split</strong></td><td>80% Masternodes / 20% PoS</td></tr>
</table>

<!-- Historical Masternode Collateral -->
<table>
  <thead>
    <tr><th>Block Height</th><th>Collateral</th></tr>
  </thead>
  <tbody>
    <tr><td>0–260000</td><td>40,000 SCH</td></tr>
    <tr><td>260001–520000</td><td>60,000 SCH</td></tr>
    <tr><td>520001–780000</td><td>80,000 SCH</td></tr>
    <tr><td>780001–1,040,000</td><td>90,000 SCH</td></tr>
    <tr><td>1,040,001–modern collateral activation</td><td>100,000 SCH</td></tr>
  </tbody>
</table>

<p><strong>SCH Permanent Reward and Masternode Model</strong></p>
<ul>
  <li><strong>Permanent Reward Split</strong> — Schilling Coin (SCH) is intended to retain its <strong>80% masternode / 20% Proof of Stake (PoS)</strong> block-reward model permanently under the current SCH development direction. There is no planned transition to a 100% PoS reward model.</li>

  <li><strong>Historical SCH Supply Policy</strong> — The original Schilling Coin developers designed SCH around a planned maximum issuance of <strong>200,000,000 SCH</strong>, with normal block-reward issuance intended to end under that original monetary model.</li>

  <li><strong>Current SCH Monetary Policy</strong> — The current SCH development direction replaces the original terminal-reward model with a continuing <strong>block-reward halving system</strong>. SCH is therefore no longer intended to permanently end normal block rewards after reaching the original 200,000,000 SCH issuance target.</li>

  <li><strong>First Planned Halving</strong> — The current SCH source implements the first planned halving at <strong>block 5,783,307</strong>. The normal base block reward changes from <strong>32.29999999 SCH</strong> to <strong>16.14999999 SCH</strong> under this schedule.</li>

  <li><strong>Long-Term Halving Policy</strong> — Under the current SCH development direction, SCH block rewards are intended to continue under a long-term halving schedule rather than permanently ending under the original 200,000,000 SCH supply design.</li>

  <li><strong>Reward Distribution After Halvings</strong> — Halvings change the amount of the base block reward, not SCH's permanent reward-distribution model. Applicable rewards continue to be divided <strong>80% to masternodes and 20% to the Proof of Stake block producer</strong>.</li>

  <li><strong>Future Transaction Fee Rewards</strong> — SCH is also intended to transition away from the historical treatment in which ordinary PoS transaction fees are not added to the staking reward. Following a future consensus activation, ordinary transaction fees are planned to become part of SCH's block-reward distribution.</li>

  <li><strong>Transaction Fee Distribution</strong> — After activation, ordinary transaction fees are intended to follow SCH's established reward allocation: <strong>80% to the scheduled masternode and 20% to the block-producing PoS staker</strong>.</li>

  <li><strong>Fee-Rewards Activation</strong> — The final consensus activation height for incorporating ordinary transaction fees into SCH rewards remains undecided and is separate from the block-subsidy halving schedule.</li>

  <li><strong>PoS Reward</strong> — The block-producing Proof of Stake participant receives <strong>20%</strong> of the applicable block reward.</li>

  <li><strong>Masternode Reward</strong> — SCH masternodes collectively retain <strong>80%</strong> of the applicable block reward. The current SCH development direction does not plan to eliminate masternode rewards or reduce the masternode reward share to 0%.</li>

  <li><strong>100 Masternode Collateral Levels</strong> — Beginning at the activation height defined by SCH consensus code, the modern SCH masternode system uses <strong>100 separate collateral levels</strong>. Level 1 requires <strong>100,000 SCH</strong>, Level 2 requires <strong>200,000 SCH</strong>, Level 3 requires <strong>300,000 SCH</strong>, continuing in increments of 100,000 SCH through Level 100 at <strong>10,000,000 SCH</strong>.</li>

  <li><strong>Payment Units</strong> — Each <strong>100,000 SCH</strong> of collateral represents one masternode payment unit. Level 1 therefore represents one payment unit, Level 2 represents two payment units, and so forth through Level 100, which represents 100 payment units.</li>

  <li><strong>Consecutive Masternode Rewards</strong> — When selected for payment, a Level 1 masternode receives <strong>1 consecutive masternode block reward</strong>. A Level 2 masternode receives <strong>2 consecutive masternode block rewards</strong>. This pattern continues through Level 100, which receives <strong>100 consecutive masternode block rewards</strong>.</li>

  <li><strong>Payment Queue</strong> — After a masternode completes its full consecutive payout sequence, it returns to the back of the masternode payment queue and waits to be selected again.</li>

  <li><strong>Proportional Collateral Economics</strong> — The 100-level design is intended to maintain proportional reward treatment per 100,000 SCH payment unit. Increasing collateral increases the number of consecutive rewards associated with a selection rather than creating a separate percentage reward schedule for each collateral level.</li>

  <li><strong>Selection</strong> — Eligible masternodes participate in the SCH masternode payment-selection system regardless of collateral level. Collateral level determines the number of consecutive masternode reward payments received after selection.</li>

  <li><strong>Long-Term Model</strong> — Under the current SCH development direction, the <strong>20% PoS / 80% masternode</strong> reward split and the <strong>100-level, 100,000-to-10,000,000 SCH collateral structure</strong> are intended as permanent features of Schilling Coin.</li>

  <li><strong>Historical Compatibility</strong> — Historical collateral requirements and historical reward behavior remain consensus-relevant for the portions of the existing SCH blockchain during which those rules applied. Modern rules do not rewrite or reinterpret historical SCH blocks.</li>
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