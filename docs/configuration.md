# OpenCBDC Configuration Reference

This document provides a complete reference for all configuration parameters used by the OpenCBDC transaction processing system.
Configuration is loaded from plain-text `.cfg` files using the `cbdc::config::parser` class.

> **Source:** [`src/util/common/config.hpp`](../src/util/common/config.hpp) and [`src/util/common/config.cpp`](../src/util/common/config.cpp)

---

## Table of Contents

- [File Format](#file-format)
- [Environment Variable Overrides](#environment-variable-overrides)
- [Architecture Selection](#architecture-selection)
- [Sentinel Parameters](#sentinel-parameters)
- [Shard Parameters (Atomizer)](#shard-parameters-atomizer)
- [Atomizer Parameters](#atomizer-parameters)
- [Archiver Parameters](#archiver-parameters)
- [Watchtower Parameters](#watchtower-parameters)
- [Shard Parameters (2PC — Locking Shards)](#shard-parameters-2pc--locking-shards)
- [Coordinator Parameters (2PC)](#coordinator-parameters-2pc)
- [Raft Consensus Parameters](#raft-consensus-parameters)
- [Initial Mint Parameters](#initial-mint-parameters)
- [Seed Parameters](#seed-parameters)
- [Load Generator Parameters](#load-generator-parameters)
- [Example Configurations](#example-configurations)

---

## File Format

Configuration files use a simple `key=value` format, one parameter per line.

**Supported value types:**

| Type | Format | Example |
|------|--------|---------|
| String | Double-quoted | `shard0_db="shard0"` |
| Integer | Bare number | `shard_count=4` |
| Double | Number with decimal | `loadgen_invalid_tx_rate=0.05` |
| Endpoint | Quoted `"host:port"` | `shard0_endpoint="127.0.0.1:6666"` |
| Log level | Quoted uppercase string | `shard0_loglevel="WARN"` |

**Log level values** (from least to most verbose): `FATAL`, `ERROR`, `WARN`, `INFO`, `DEBUG`, `TRACE`

**Comments:** Lines that do not match the `key=value` format are ignored.

---

## Environment Variable Overrides

Every configuration parameter can be overridden by setting an environment variable with the **uppercase** version of the config key.

| Config File | Environment Variable |
|-------------|---------------------|
| `window_size=40000` | `WINDOW_SIZE=50000` |
| `shard_count=4` | `SHARD_COUNT=8` |
| `shard0_loglevel="WARN"` | `SHARD0_LOGLEVEL='"DEBUG"'` |

> **Note:** String values in environment variables must be single-quoted around double-quotes: `SOMEKEY='"some_value"'`

---

## Architecture Selection

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `2pc` | Integer | `0` (Atomizer) | Set to `1` to enable Two-Phase Commit mode. When absent or `0`, the Atomizer architecture is used. |

---

## Sentinel Parameters

Sentinels are the front-end gateways for transaction submission. They perform validation and produce attestations.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `sentinel_count` | Integer | `0` | Number of sentinel instances |
| `sentinel<N>_endpoint` | Endpoint | *(required)* | Listen endpoint for sentinel N (e.g., `"127.0.0.1:5555"`) |
| `sentinel<N>_loglevel` | Log level | `"WARN"` | Log verbosity for sentinel N |
| `sentinel<N>_private_key` | String | *(optional)* | 64-character hex-encoded secp256k1 private key for sentinel N |
| `sentinel<N>_public_key` | String | *(required if threshold > 0)* | 64-character hex-encoded public key for sentinel N |
| `attestation_threshold` | Integer | `1` | Minimum number of valid sentinel attestations required on a compact transaction |

**Example:**
```
sentinel_count=2
sentinel0_endpoint="127.0.0.1:5555"
sentinel0_loglevel="INFO"
sentinel0_private_key="0000000000000001000000000000000000000000000000000000000000000000"
sentinel0_public_key="eaa649f21f51bdbae7be4ae34ce6e5217a58fdce7f47f9aa7f3b58fa2120e2b3"
sentinel1_endpoint="127.0.0.1:5556"
sentinel1_loglevel="INFO"
sentinel1_private_key="0000000000000002000000000000000000000000000000000000000000000000"
sentinel1_public_key="abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"
```

---

## Shard Parameters (Atomizer)

In the Atomizer architecture, shards store a prefix-range partition of the UHS in LevelDB.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `shard_count` | Integer | `0` | Number of shard instances |
| `shard<N>_endpoint` | Endpoint | *(required)* | Listen endpoint for shard N |
| `shard<N>_loglevel` | Log level | `"WARN"` | Log verbosity for shard N |
| `shard<N>_db` | String | *(required)* | Relative path to the LevelDB directory for shard N |
| `shard<N>_start` | Integer | *(required)* | Start of the inclusive UHS ID prefix range (0–255) |
| `shard<N>_end` | Integer | *(required)* | End of the inclusive UHS ID prefix range (0–255) |

The prefix range `[start, end]` determines which UHS IDs this shard is responsible for, based on the first byte of the hash.
For a single shard covering the entire range, use `start=0, end=255`.

**Example:**
```
shard_count=2
shard0_endpoint="127.0.0.1:6555"
shard0_db="shard0_db"
shard0_start=0
shard0_end=127
shard0_loglevel="INFO"
shard1_endpoint="127.0.0.1:6556"
shard1_db="shard1_db"
shard1_start=128
shard1_end=255
shard1_loglevel="INFO"
```

---

## Atomizer Parameters

The Atomizer produces a total ordering of transactions by assembling them into blocks.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `atomizer_count` | Integer | `0` | Number of atomizer nodes in the Raft cluster |
| `atomizer<N>_endpoint` | Endpoint | *(required)* | Client-facing endpoint for atomizer N |
| `atomizer<N>_raft_endpoint` | Endpoint | *(required)* | Raft peer-to-peer endpoint for atomizer N |
| `atomizer<N>_loglevel` | Log level | `"WARN"` | Log verbosity for atomizer N |
| `target_block_interval` | Integer | `250` | Target interval between blocks in **milliseconds** |
| `stxo_cache_depth` | Integer | `1` | Number of recent blocks to keep in the spent transaction output cache |
| `batch_size` | Integer | `2000` | Maximum number of transactions per Raft log entry |

---

## Archiver Parameters

Archivers persist blocks for long-term storage and historical queries.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `archiver_count` | Integer | `0` | Number of archiver instances |
| `archiver<N>_endpoint` | Endpoint | *(required)* | Listen endpoint for archiver N |
| `archiver<N>_loglevel` | Log level | `"WARN"` | Log verbosity for archiver N |
| `archiver<N>_db` | String | *(required)* | Relative path to the archiver's database directory |

---

## Watchtower Parameters

Watchtowers index recent blocks so clients can query transaction status.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `watchtower_count` | Integer | `0` | Number of watchtower instances |
| `watchtower<N>_client_endpoint` | Endpoint | *(required)* | Endpoint for client queries |
| `watchtower<N>_internal_endpoint` | Endpoint | *(required)* | Endpoint for receiving blocks from the atomizer |
| `watchtower<N>_loglevel` | Log level | `"WARN"` | Log verbosity for watchtower N |
| `watchtower_block_cache_size` | Integer | `100` | Number of recent blocks to keep in cache (0 = unlimited) |
| `watchtower_error_cache_size` | Integer | `1000000` | Number of transaction errors to keep in cache (0 = unlimited) |

---

## Shard Parameters (2PC — Locking Shards)

In the 2PC architecture, locking shards are Raft-replicated and support per-element locking.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `shard_count` | Integer | `0` | Number of logical shard groups |
| `shard<N>_count` | Integer | *(required)* | Number of Raft nodes in shard group N |
| `shard<N>_<M>_endpoint` | Endpoint | *(required)* | Client-facing endpoint for node M of shard N |
| `shard<N>_<M>_raft_endpoint` | Endpoint | *(required)* | Raft peer endpoint for node M of shard N |
| `shard<N>_<M>_readonly_endpoint` | Endpoint | *(required)* | Read-only query endpoint for node M of shard N |
| `shard<N>_loglevel` | Log level | `"WARN"` | Log verbosity for shard group N |
| `shard<N>_start` | Integer | *(required)* | Start of UHS ID prefix range (0–255) |
| `shard<N>_end` | Integer | *(required)* | End of UHS ID prefix range (0–255) |
| `shard_completed_txs_cache_size` | Integer | `10000000` | Number of completed transactions to cache for read-only queries |

**Example (single shard, single node):**
```
2pc=1
shard_count=1
shard0_count=1
shard0_start=0
shard0_end=255
shard0_loglevel="INFO"
shard0_0_endpoint="127.0.0.1:6666"
shard0_0_raft_endpoint="127.0.0.1:6667"
shard0_0_readonly_endpoint="127.0.0.1:6767"
```

---

## Coordinator Parameters (2PC)

Coordinators orchestrate the two-phase commit protocol across locking shards.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `coordinator_count` | Integer | `0` | Number of logical coordinator groups |
| `coordinator<N>_count` | Integer | *(required)* | Number of Raft nodes in coordinator group N |
| `coordinator<N>_<M>_endpoint` | Endpoint | *(required)* | Client-facing endpoint for node M of coordinator N |
| `coordinator<N>_<M>_raft_endpoint` | Endpoint | *(required)* | Raft peer endpoint for node M of coordinator N |
| `coordinator<N>_loglevel` | Log level | `"WARN"` | Log verbosity for coordinator group N |
| `coordinator_max_threads` | Integer | `75` | Maximum number of parallel batch execution threads per coordinator |

---

## Raft Consensus Parameters

These parameters apply to all Raft-replicated components (Atomizer, Coordinators, Locking Shards).

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `election_timeout_upper` | Integer | `4000` | Upper bound for Raft election timeout in **milliseconds** |
| `election_timeout_lower` | Integer | `2000` | Lower bound for Raft election timeout in **milliseconds** |
| `heartbeat` | Integer | `1000` | Raft heartbeat interval in **milliseconds** |
| `raft_max_batch` | Integer | `100000` | Maximum number of Raft log entries to batch into one RPC message |
| `snapshot_distance` | Integer | `0` | Number of Raft log entries between snapshots (0 = disabled) |

**Tuning guidance:**
- For geo-replicated deployments, increase `election_timeout_upper` and `election_timeout_lower` to account for network latency.
- `heartbeat` should be significantly less than `election_timeout_lower`.
- `raft_max_batch` can be increased for higher throughput at the cost of larger RPC messages.

---

## Initial Mint Parameters

Control the initial minting of coins when bootstrapping a new network.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `initial_mint_count` | Integer | `20000` | Number of outputs in the initial mint transaction |
| `initial_mint_value` | Integer | `100` | Value of each output in the initial mint transaction |

---

## Seed Parameters

Used for pre-populating wallets with deterministic UTXOs for benchmarking.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `seed_privkey` | String | *(optional)* | 64-character hex-encoded private key for generating seeded UTXOs |
| `seed_value` | Integer | `0` | Value of each seeded output |
| `seed_from` | Integer | `0` | Starting index for the seed range |
| `seed_to` | Integer | `0` | Ending index for the seed range |

---

## Load Generator Parameters

Settings for the benchmark load generator tools.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `loadgen_count` | Integer | `0` | Number of load generator instances (seed range is split evenly) |
| `loadgen_sendtx_input_count` | Integer | `2` | Number of inputs per generated transaction |
| `loadgen_sendtx_output_count` | Integer | `2` | Number of outputs per generated transaction |
| `loadgen_invalid_tx_rate` | Double | `0.0` | Fraction of generated transactions that should be intentionally invalid (0.0–1.0) |
| `loadgen_fixed_tx_rate` | Double | `1.0` | Fraction of fixed-size transactions (vs. random-size) |

---

## Example Configurations

### Development — Atomizer (Single-Node)

A minimal single-node Atomizer setup for local development.

```
# Architecture: Atomizer (default)
atomizer_count=1
atomizer0_endpoint="127.0.0.1:5555"
atomizer0_raft_endpoint="127.0.0.1:5556"
atomizer0_loglevel="DEBUG"

shard_count=1
shard0_endpoint="127.0.0.1:6555"
shard0_db="shard0_db"
shard0_start=0
shard0_end=255
shard0_loglevel="DEBUG"

sentinel_count=1
sentinel0_endpoint="127.0.0.1:7555"
sentinel0_loglevel="DEBUG"
sentinel0_private_key="0000000000000001000000000000000000000000000000000000000000000000"
sentinel0_public_key="eaa649f21f51bdbae7be4ae34ce6e5217a58fdce7f47f9aa7f3b58fa2120e2b3"

archiver_count=1
archiver0_endpoint="127.0.0.1:4555"
archiver0_db="archiver0_db"

watchtower_count=1
watchtower0_client_endpoint="127.0.0.1:8555"
watchtower0_internal_endpoint="127.0.0.1:8556"
watchtower_block_cache_size=50

target_block_interval=1000
stxo_cache_depth=5
batch_size=100
```

### Development — 2PC (Single-Node)

A minimal single-node 2PC setup for local development.

```
# Architecture: Two-Phase Commit
2pc=1

sentinel_count=1
sentinel0_endpoint="127.0.0.1:5555"
sentinel0_loglevel="DEBUG"
sentinel0_private_key="0000000000000001000000000000000000000000000000000000000000000000"
sentinel0_public_key="eaa649f21f51bdbae7be4ae34ce6e5217a58fdce7f47f9aa7f3b58fa2120e2b3"

coordinator_count=1
coordinator0_count=1
coordinator0_loglevel="DEBUG"
coordinator0_0_endpoint="127.0.0.1:7777"
coordinator0_0_raft_endpoint="127.0.0.1:7778"
coordinator_max_threads=10

shard_count=1
shard0_count=1
shard0_start=0
shard0_end=255
shard0_loglevel="DEBUG"
shard0_0_endpoint="127.0.0.1:6666"
shard0_0_raft_endpoint="127.0.0.1:6667"
shard0_0_readonly_endpoint="127.0.0.1:6767"
```

### Benchmarking — 2PC (Multi-Shard)

A multi-shard 2PC setup for throughput benchmarking.

```
2pc=1

sentinel_count=1
sentinel0_endpoint="10.0.0.1:5555"
sentinel0_loglevel="WARN"
sentinel0_private_key="0000000000000001000000000000000000000000000000000000000000000000"
sentinel0_public_key="eaa649f21f51bdbae7be4ae34ce6e5217a58fdce7f47f9aa7f3b58fa2120e2b3"

coordinator_count=1
coordinator0_count=1
coordinator0_loglevel="WARN"
coordinator0_0_endpoint="10.0.0.2:7777"
coordinator0_0_raft_endpoint="10.0.0.2:7778"
coordinator_max_threads=100

shard_count=4
shard0_count=1
shard0_start=0
shard0_end=63
shard0_loglevel="WARN"
shard0_0_endpoint="10.0.0.10:6666"
shard0_0_raft_endpoint="10.0.0.10:6667"
shard0_0_readonly_endpoint="10.0.0.10:6767"

shard1_count=1
shard1_start=64
shard1_end=127
shard1_loglevel="WARN"
shard1_0_endpoint="10.0.0.11:6666"
shard1_0_raft_endpoint="10.0.0.11:6667"
shard1_0_readonly_endpoint="10.0.0.11:6767"

shard2_count=1
shard2_start=128
shard2_end=191
shard2_loglevel="WARN"
shard2_0_endpoint="10.0.0.12:6666"
shard2_0_raft_endpoint="10.0.0.12:6667"
shard2_0_readonly_endpoint="10.0.0.12:6767"

shard3_count=1
shard3_start=192
shard3_end=255
shard3_loglevel="WARN"
shard3_0_endpoint="10.0.0.13:6666"
shard3_0_raft_endpoint="10.0.0.13:6667"
shard3_0_readonly_endpoint="10.0.0.13:6767"

# Load generator settings
seed_privkey="0000000000000001000000000000000000000000000000000000000000000000"
seed_value=100
seed_from=0
seed_to=1000000
loadgen_count=4
loadgen_sendtx_input_count=2
loadgen_sendtx_output_count=2
loadgen_fixed_tx_rate=1.0

initial_mint_count=100000
initial_mint_value=1000

# Raft tuning
election_timeout_upper=4000
election_timeout_lower=2000
heartbeat=1000
raft_max_batch=100000
```

---

## See Also

- [Architecture Overview](architecture.md) — understand the components these parameters configure
- [API Reference](api.md) — `cbdc::config::options` struct and `cbdc::config::parser` class
- [Developer Setup Guide](contributing.md) — building and running OpenCBDC
- Example config files: [`atomizer-compose.cfg`](../atomizer-compose.cfg), [`2pc-compose.cfg`](../2pc-compose.cfg)
