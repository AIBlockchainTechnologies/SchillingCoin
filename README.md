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

<!-- Current Coin Specifications (updated) -->
<table>
  <tr><td><strong>Algo</strong></td><td>Quark</td></tr>
  <tr><td><strong>Block Time</strong></td><td>60 Seconds</td></tr>
  <tr><td><strong>Max Coin Supply</strong></td><td>200,000,000 SCH</td></tr>
  <tr><td><strong>Premine</strong></td><td>NONE</td></tr>
  <tr><td><strong>Maturity</strong></td><td>50</td></tr>
  <tr><td><strong>Port</strong></td><td>9070</td></tr>
  <tr><td><strong>RPC Port</strong></td><td>9071</td></tr>
  <tr><td><strong>Reward Per Block (original 200,000,000 max supply pre‑mint completion)</strong></td><td>32.29999999 SCH</td></tr>
  <tr><td><strong>Reward Per Block (original 200,000,000 max supply post‑mint completion)</strong></td><td>1 SCH (fixed forever; effective on the first block after total circulating initial max supply reaches 200,000,000 SCH)</td></tr>
  <tr><td><strong>Reward Split (pre‑mint completion)</strong></td><td>80% (MN) / 20% (PoS)</td></tr>
  <tr><td><strong>Reward Split (pre‑mint completion)</strong></td><td>100% to PoS; timing of new masternode collateral becoming 1 SCH permanently to be announced separately, shortly after 100% of rewards go to PoS.</td></tr>
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
    <tr><td>1,040,001–(pre‑repurposing)</td><td>100,000 SCH</td></tr>
    <tr><td>(post‑repurposing)</td><td>1 SCH (permanent; collateral retained only for MasterSeedNode identity)</td></tr>
  </tbody>
</table>

<p><strong>SCH Network Transition Notice</strong></p>
<ul>
  <li><strong>Final Consensus Model</strong> — Schilling Coin (SCH) will transition to a full <strong>100% Proof of Stake (PoS)</strong> reward model as part of the network roadmap.</li>
  <li><strong>Post‑Mint Block Reward</strong> — When the total circulating supply reaches <strong>200,000,000 SCH</strong>, the block reward will change to <strong>1 SCH</strong> and remain fixed at 1 SCH forever, effective on the first block after the initial 200,000,000 max supply cap is reached, estimated to be late in the year 2029.</li>
  <li><strong>Activation Block For 100% PoS</strong> — The exact block height at which block rewards become 100% PoS is not yet decided. An official announcement will specify the activation block before it occurs.</li>
  <li><strong>Masternode Reward Removal Timing</strong> — The precise block height and timing for removing or redistributing masternode rewards (i.e., reducing masternode reward share per block to 0%) have not been finalized. Any change to masternode reward distribution will be announced separately and in advance, and will occur only after the network has moved to 100% PoS.</li>
  <li><strong>Collateral Repurposing Sequence</strong> — The permanent reduction of masternode collateral to <strong>1 SCH</strong> and the repurposing of masternodes into <strong>MasterSeedNodes</strong> is planned to occur shortly after the activation that makes PoS 100% of block rewards. The exact block height for the collateral change will be announced in the same communications that define the PoS activation and masternode reward schedule.</li>
  <li><strong>MasterSeedNode Role</strong> — After repurposing, MasterSeedNodes will serve primarily as reliable network seed nodes for peer discovery and bootstrap assistance. Under SCH's current development team, masternodes will never again be entitled to block rewards, at all.</li>
  <li><strong>Historical Reference</strong> — The collateral tiers and reward behavior above are retained for historical reference. They will be deprecated or updated according to the official transition plan announced with the activation block(s).</li>
  <li><strong>Operator Guidance</strong> — Existing masternode operators should await the formal migration plan and tooling guidance. No operator action is required until the project publishes the transition instructions and the activation block(s).</li>
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