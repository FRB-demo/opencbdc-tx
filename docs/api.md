# OpenCBDC C++ API Reference

This document provides a reference for the core C++ APIs in the OpenCBDC transaction processing system.
It covers the transaction data model, validation, wallet operations, serialization framework, and network protocol interfaces.

For architecture-level context, see [architecture.md](architecture.md).

---

## Table of Contents

- [Transaction Data Model](#transaction-data-model)
- [Transaction Validation](#transaction-validation)
- [Wallet](#wallet)
- [Serialization](#serialization)
- [Network Protocol Interfaces](#network-protocol-interfaces)
- [Configuration API](#configuration-api)

---

## Transaction Data Model

> **Header:** [`src/uhs/transaction/transaction.hpp`](../src/uhs/transaction/transaction.hpp)
> **Namespace:** `cbdc::transaction`

### Core Types

```
hash_t      = std::array<unsigned char, 32>   (SHA-256 hash)
pubkey_t    = std::array<unsigned char, 32>    (secp256k1 public key)
privkey_t   = std::array<unsigned char, 32>    (secp256k1 private key)
signature_t = std::array<unsigned char, 64>    (secp256k1 signature)
witness_t   = std::vector<std::byte>           (witness program data)
```

> Defined in [`src/util/common/hash.hpp`](../src/util/common/hash.hpp) and [`src/util/common/keys.hpp`](../src/util/common/keys.hpp).

### Class Hierarchy

```
                    ┌─────────────┐
                    │  out_point   │  Identifies a specific output of a previous tx
                    │  (tx_id,idx) │
                    └──────┬──────┘
                           │ referenced by
                    ┌──────▼──────┐
                    │   output     │  A new unspent output (value + witness commitment)
                    └──────┬──────┘
                           │ combined into
                    ┌──────▼──────┐
                    │   input      │  An output being consumed (out_point + output data)
                    └──────┬──────┘
                           │ aggregated in
              ┌────────────▼────────────┐
              │        full_tx           │  Complete transaction with proofs
              │  (inputs, outputs,       │
              │   witnesses)             │
              └────────────┬────────────┘
                           │ compacted to
              ┌────────────▼────────────┐
              │       compact_tx         │  Hash-only representation for settlement
              │  (id, input hashes,      │
              │   output hashes,         │
              │   attestations)          │
              └─────────────────────────┘
```

### `out_point`

Uniquely identifies a previous transaction output by its transaction ID and output index.

```cpp
struct out_point {
    hash_t m_tx_id{};       // SHA-256 ID of the source transaction
    uint64_t m_index{0};    // Zero-based index within that transaction's outputs

    out_point() = default;
    out_point(const hash_t& hash, uint64_t index);

    auto operator==(const out_point& rhs) const -> bool;
    auto operator<(const out_point& rhs) const -> bool;
};
```

### `output`

A new unspent transaction output representing a value commitment.

```cpp
struct output {
    hash_t m_witness_program_commitment{};  // Hash of the witness program (e.g., public key)
    uint64_t m_value{0};                    // Monetary value in base units

    output() = default;
    output(hash_t witness_program_commitment, uint64_t value);

    auto operator==(const output& rhs) const -> bool;
    auto operator!=(const output& rhs) const -> bool;
};
```

### `input`

An output being spent in a new transaction. Combines the reference (`out_point`) with the output data.

```cpp
struct input {
    out_point m_prevout;        // Reference to the output being spent
    output m_prevout_data;      // The output data (value, witness commitment)

    input() = default;

    auto operator==(const input& rhs) const -> bool;
    auto operator!=(const input& rhs) const -> bool;

    // Computes the UHS ID (hash commitment) for this input
    [[nodiscard]] auto hash() const -> hash_t;
};
```

### `full_tx`

A complete transaction containing inputs, outputs, and witness (signature) data.

```cpp
struct full_tx {
    std::vector<input> m_inputs{};      // Inputs being spent
    std::vector<output> m_outputs{};    // New outputs being created
    std::vector<witness_t> m_witness{}; // Witness data (one per input, contains signatures)

    full_tx() = default;

    auto operator==(const full_tx& rhs) const -> bool;
};
```

### `compact_tx`

A compact representation of a transaction containing only hash commitments, used for settlement.
Created from a `full_tx` by hashing all inputs and outputs.

```cpp
struct compact_tx {
    hash_t m_id{};                          // Transaction ID (hash of the full_tx)
    std::vector<hash_t> m_inputs;           // UHS IDs of inputs (hashes)
    std::vector<hash_t> m_uhs_outputs;      // UHS IDs of outputs (hashes)
    std::unordered_map<pubkey_t, signature_t, hashing::null> m_attestations;

    compact_tx() = default;
    explicit compact_tx(const full_tx& tx);  // Construct from a full transaction

    // Sign the compact transaction with a private key (sentinel attestation)
    [[nodiscard]] auto sign(secp256k1_context* ctx, const privkey_t& key) const
        -> sentinel_attestation;

    // Verify a sentinel attestation against this transaction
    [[nodiscard]] auto verify(secp256k1_context* ctx,
                              const sentinel_attestation& att) const -> bool;

    // Compute the hash (transaction ID) of this compact transaction
    [[nodiscard]] auto hash() const -> hash_t;

    auto operator==(const compact_tx& tx) const noexcept -> bool;
};
```

### Free Functions

```cpp
// Compute the transaction ID (SHA-256 hash) of a full transaction
[[nodiscard]] auto tx_id(const full_tx& tx) noexcept -> hash_t;

// Create an input from a specific output of a transaction
auto input_from_output(const full_tx& tx, size_t i, const hash_t& txid)
    -> std::optional<input>;
auto input_from_output(const full_tx& tx, size_t i)
    -> std::optional<input>;

// Compute the UHS ID for a given output
auto uhs_id_from_output(const hash_t& entropy, uint64_t i, const output& output)
    -> hash_t;
```

---

## Transaction Validation

> **Header:** [`src/uhs/transaction/validation.hpp`](../src/uhs/transaction/validation.hpp)
> **Namespace:** `cbdc::transaction::validation`

The validation module checks transaction-local invariants (structure, signatures, value balance) without consulting system state.

### Error Types

#### `tx_error_code`

Top-level transaction structural errors:

| Code | Description |
|------|-------------|
| `no_inputs` | Transaction has zero inputs |
| `no_outputs` | Transaction has zero outputs |
| `missing_witness` | Number of witness entries does not match number of inputs |
| `asymmetric_values` | Sum of input values does not equal sum of output values |
| `value_overflow` | Total value exceeds representable range |

#### `input_error_code`

Per-input errors:

| Code | Description |
|------|-------------|
| `duplicate` | Two or more inputs reference the same `out_point` |
| `data_error` | The input's output data fails validation (e.g., zero value) |

#### `output_error_code`

Per-output errors:

| Code | Description |
|------|-------------|
| `zero_value` | Output has a value of zero |

#### `witness_error_code`

Per-witness (signature) errors:

| Code | Description |
|------|-------------|
| `missing_witness_program_type` | Witness data is empty |
| `unknown_witness_program_type` | Witness program type byte is not recognized |
| `malformed` | Witness data has an incorrect length |
| `program_mismatch` | Public key hash does not match the input's witness commitment |
| `invalid_public_key` | Public key cannot be parsed |
| `invalid_signature` | Signature verification failed |

#### Composite Error Types

```cpp
struct input_error {
    input_error_code m_code{};
    std::optional<output_error_code> m_data_err;  // Set if m_code == data_error
    uint64_t m_idx{};                              // Index of the failing input
};

struct witness_error {
    witness_error_code m_code{};
    uint64_t m_idx{};    // Index of the failing witness
};

struct output_error {
    output_error_code m_code{};
    uint64_t m_idx{};    // Index of the failing output
};

// A transaction error is one of: input_error, output_error, witness_error, or tx_error_code
using tx_error = std::variant<input_error, output_error, witness_error, tx_error_code>;
```

### Validation Functions

```cpp
// Full validation: structure + value balance + witness signatures
// Returns std::nullopt on success, or the first error found
auto check_tx(const transaction::full_tx& tx) -> std::optional<tx_error>;

// Structural validation only (no signature checks)
auto check_tx_structure(const transaction::full_tx& tx) -> std::optional<tx_error>;

// Validate a single input's structure
auto check_input_structure(const transaction::input& inp)
    -> std::optional<std::pair<input_error_code, std::optional<output_error_code>>>;

// Validate input/output value balance and check for duplicate inputs
auto check_in_out_set(const transaction::full_tx& tx) -> std::optional<tx_error>;

// Validate a single witness (signature) at the given index
auto check_witness(const transaction::full_tx& tx, size_t idx)
    -> std::optional<witness_error_code>;

// Validate a Pay-to-Public-Key witness
auto check_p2pk_witness(const transaction::full_tx& tx, size_t idx)
    -> std::optional<witness_error_code>;

// Verify sentinel attestations on a compact transaction
// Returns true if at least `threshold` valid attestations from `pubkeys` are present
auto check_attestations(const transaction::compact_tx& tx,
                        const std::unordered_set<pubkey_t, hashing::null>& pubkeys,
                        size_t threshold) -> bool;

// Convert a tx_error to a human-readable string
auto to_string(const tx_error& err) -> std::string;
```

### Witness Programs

Currently, only one witness program type is supported:

| Type | Value | Description |
|------|-------|-------------|
| `p2pk` | `0x0` | Pay to Public Key — the witness contains the public key and a signature over the transaction |

---

## Wallet

> **Header:** [`src/uhs/transaction/wallet.hpp`](../src/uhs/transaction/wallet.hpp)
> **Namespace:** `cbdc::transaction`

The `wallet` class manages private keys, tracks unspent outputs, and constructs transactions.

### Key Methods

#### Key Management

```cpp
// Generate a new key pair and return the public key
auto generate_key() -> pubkey_t;
```

#### Minting

```cpp
// Create a mint transaction with n_outputs outputs, each worth output_val
// Used for bootstrapping the system with initial funds
auto mint_new_coins(size_t n_outputs, uint32_t output_val) -> full_tx;
```

#### Sending

```cpp
// Send `amount` to `payee`; optionally sign the transaction
// Automatically selects inputs from the wallet's UTXO set
// Returns std::nullopt if the wallet has insufficient funds
auto send_to(uint32_t amount, const pubkey_t& payee, bool sign_tx)
    -> std::optional<full_tx>;

// Send with explicit input and output counts (for benchmarking)
auto send_to(size_t input_count, size_t output_count,
             const pubkey_t& payee, bool sign_tx)
    -> std::optional<full_tx>;

// Fan-out: create output_count outputs from available inputs
auto fan(size_t output_count, uint32_t value,
         const pubkey_t& payee, bool sign_tx)
    -> std::optional<transaction::full_tx>;
```

#### Transaction Confirmation

```cpp
// Update the wallet's UTXO set after a transaction is confirmed on-chain
// Removes spent inputs and adds received outputs
void confirm_transaction(const full_tx& tx);

// Confirm specific inputs (credits) received from another wallet
void confirm_inputs(const std::vector<input>& credits);
```

#### Signing

```cpp
// Sign all inputs of a transaction with the wallet's private keys
void sign(full_tx& tx) const;
```

#### Balance & State

```cpp
// Current spendable balance (sum of all tracked UTXO values)
auto balance() const -> uint64_t;

// Number of tracked UTXOs
auto count() const -> size_t;

// Check if a specific input is spendable by this wallet
auto is_spendable(const input& in) const -> bool;
```

#### Persistence

```cpp
// Save wallet state (keys + UTXOs) to a file
void save(const std::string& wallet_file) const;

// Load wallet state from a file
void load(const std::string& wallet_file);
```

#### Seeding (for Benchmarks)

```cpp
// Seed the wallet with deterministic UTXOs derived from a private key
// Used for pre-populating wallets in benchmark setups
auto seed(const privkey_t& privkey, uint32_t value,
          size_t begin_seed, size_t end_seed) -> bool;

// Seed read-only (no private key, cannot spend — for tracking only)
void seed_readonly(const hash_t& witness_commitment, uint32_t value,
                   size_t begin_seed, size_t end_seed);

// Create a transaction from a seeded UTXO at the given index
auto create_seeded_transaction(size_t seed_idx) -> std::optional<full_tx>;
```

#### Exporting

```cpp
// Export the inputs from a send transaction that belong to the payee
// Used for out-of-band transfer of UTXO data to the recipient
static auto export_send_inputs(const full_tx& send_tx, const pubkey_t& payee)
    -> std::vector<input>;
```

---

## Serialization

OpenCBDC uses a custom binary serialization framework based on `operator<<` and `operator>>` overloads.

### Buffer

> **Header:** [`src/util/common/buffer.hpp`](../src/util/common/buffer.hpp)
> **Namespace:** `cbdc`

A contiguous byte buffer used as the underlying storage for serialized data.

```cpp
class buffer {
public:
    buffer() = default;

    [[nodiscard]] auto size() const -> size_t;
    [[nodiscard]] auto data() -> void*;
    [[nodiscard]] auto data() const -> const void*;
    [[nodiscard]] auto data_at(size_t offset) -> void*;
    [[nodiscard]] auto data_at(size_t offset) const -> const void*;

    void append(const void* data, size_t len);
    void clear();
    void extend(size_t len);

    auto operator==(const buffer& other) const -> bool;

    // Hex encoding/decoding
    static auto from_hex(const std::string& hex) -> std::optional<cbdc::buffer>;
    [[nodiscard]] auto to_hex() const -> std::string;
    static auto from_hex_prefixed(const std::string& hex,
                                  const std::string& prefix = "0x")
        -> std::optional<buffer>;
    [[nodiscard]] auto to_hex_prefixed(const std::string& prefix = "0x") const
        -> std::string;
};
```

### Serializer (Abstract Interface)

> **Header:** [`src/util/serialization/serializer.hpp`](../src/util/serialization/serializer.hpp)
> **Namespace:** `cbdc`

```cpp
class serializer {
public:
    virtual ~serializer() = default;

    virtual explicit operator bool() const = 0;    // Check for errors
    virtual void advance_cursor(size_t len) = 0;   // Skip bytes
    virtual void reset() = 0;                       // Reset cursor to start
    [[nodiscard]] virtual auto end_of_buffer() const -> bool = 0;

    virtual auto write(const void* data, size_t len) -> bool = 0;
    virtual auto read(void* data, size_t len) -> bool = 0;
};
```

### Buffer Serializer

> **Header:** [`src/util/serialization/buffer_serializer.hpp`](../src/util/serialization/buffer_serializer.hpp)
> **Namespace:** `cbdc`

Concrete implementation of `serializer` backed by a `buffer`.

```cpp
class buffer_serializer final : public cbdc::serializer {
public:
    explicit buffer_serializer(cbdc::buffer& pkt);

    explicit operator bool() const final;
    void advance_cursor(size_t len) final;
    void reset() final;
    [[nodiscard]] auto end_of_buffer() const -> bool final;
    auto write(const void* data, size_t len) -> bool final;
    auto read(void* data, size_t len) -> bool final;
};
```

### Serialization Operators

> **Header:** [`src/util/serialization/format.hpp`](../src/util/serialization/format.hpp)
> **Namespace:** `cbdc`

The framework provides `operator<<` (serialize) and `operator>>` (deserialize) overloads for standard C++ types.
Custom types are serialized by providing their own overloads.

**Supported types:**

| Type | Notes |
|------|-------|
| `std::byte` | Single byte |
| `buffer` | Length-prefixed byte buffer |
| Integral types (`uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`, `int32_t`, etc.) | Network byte order (big-endian) |
| `std::array<T, N>` | Fixed-size array, elements serialized in order |
| `std::vector<T>` | Length-prefixed, elements serialized in order |
| `std::optional<T>` | Boolean flag + value if present |
| `std::pair<A, B>` | Both elements serialized in order |
| `std::unordered_map<K, V>` | Length-prefixed, key-value pairs |
| `std::set<K>` | Length-prefixed, elements in order |
| `std::unordered_set<K>` | Length-prefixed, elements in order |
| `std::variant<Ts...>` | Type index + value |
| Enum types | Cast to underlying integral type |

**Usage example** (pseudocode):

```cpp
// Serialization
cbdc::buffer buf;
cbdc::buffer_serializer ser(buf);
cbdc::transaction::full_tx tx = /* ... */;
ser << tx;  // Serialize the transaction into the buffer

// Deserialization
cbdc::buffer_serializer deser(buf);
cbdc::transaction::full_tx tx2;
deser >> tx2;  // Deserialize from the buffer
```

### Adding Serialization for Custom Types

To make a custom type serializable, define `operator<<` and `operator>>` overloads in the `cbdc` namespace:

```cpp
// In a header (e.g., my_type_format.hpp)
namespace cbdc {
    auto operator<<(serializer& ser, const my_type& val) -> serializer&;
    auto operator>>(serializer& deser, my_type& val) -> serializer&;
}
```

See [`src/uhs/sentinel/format.hpp`](../src/uhs/sentinel/format.hpp) for an example.

---

## Network Protocol Interfaces

This section describes the RPC interfaces used for communication between components.

### Sentinel Interface

> **Header:** [`src/uhs/sentinel/interface.hpp`](../src/uhs/sentinel/interface.hpp)
> **Namespace:** `cbdc::sentinel`

The Sentinel is the front-end gateway for transaction submission.

#### Transaction Status

```cpp
enum class tx_status {
    pending,         // Submitted to the network, awaiting confirmation
    static_invalid,  // Failed local validation (must fix and resubmit)
    state_invalid,   // Inputs do not exist or already spent
    confirmed        // Successfully settled
};
```

#### RPC Messages

```cpp
// Request: submit a full transaction for execution
struct execute_request : public transaction::full_tx {};

// Response: execution result
struct execute_response {
    cbdc::sentinel::tx_status m_tx_status{};
    std::optional<transaction::validation::tx_error> m_tx_error;
};

// Request: validate and attest a transaction (without execution)
struct validate_request : transaction::full_tx {};

// Response: sentinel attestation (public key + signature)
using validate_response = transaction::sentinel_attestation;

// Combined RPC types
using request = std::variant<execute_request, validate_request>;
using response = std::variant<execute_response, validate_response>;
```

#### Sentinel Interface Methods

```cpp
class interface {
public:
    // Validate, compact, and submit a transaction to the network
    // Returns the execution result or std::nullopt on failure
    virtual auto execute_transaction(transaction::full_tx tx)
        -> std::optional<execute_response> = 0;

    // Validate a transaction and produce a sentinel attestation
    // Returns the attestation or std::nullopt if invalid
    virtual auto validate_transaction(transaction::full_tx tx)
        -> std::optional<validate_response> = 0;
};
```

### Coordinator Interface (2PC)

> **Header:** [`src/uhs/twophase/coordinator/interface.hpp`](../src/uhs/twophase/coordinator/interface.hpp)
> **Namespace:** `cbdc::coordinator`

```cpp
class interface {
public:
    using callback_type = std::function<void(std::optional<bool>)>;

    // Execute a compact transaction through the 2PC protocol
    // result_callback receives: true (committed), false (aborted), nullopt (error)
    virtual auto execute_transaction(transaction::compact_tx tx,
                                     callback_type result_callback) -> bool = 0;
};
```

### Locking Shard Interface (2PC)

> **Header:** [`src/uhs/twophase/locking_shard/interface.hpp`](../src/uhs/twophase/locking_shard/interface.hpp)
> **Namespace:** `cbdc::locking_shard`

```cpp
class interface {
public:
    explicit interface(std::pair<uint8_t, uint8_t> output_range);

    // Phase 1: Lock input hashes for a batch of transactions
    // Returns a vector of bools indicating which txs were successfully locked
    virtual auto lock_outputs(std::vector<tx>&& txs, const hash_t& dtx_id)
        -> std::optional<std::vector<bool>> = 0;

    // Phase 2: Commit or abort each transaction in the batch
    // complete_txs[i] == true means commit tx i; false means abort
    virtual auto apply_outputs(std::vector<bool>&& complete_txs,
                               const hash_t& dtx_id) -> bool = 0;

    // Cleanup: discard batch metadata after apply completes
    virtual auto discard_dtx(const hash_t& dtx_id) -> bool = 0;

    // Check if a hash falls within this shard's responsible range
    [[nodiscard]] virtual auto hash_in_shard_range(const hash_t& h) const -> bool;

    virtual void stop() = 0;
};
```

### Atomizer Shard Interface

> **Header:** [`src/uhs/atomizer/shard/shard.hpp`](../src/uhs/atomizer/shard/shard.hpp)
> **Namespace:** `cbdc::shard`

```cpp
class shard {
public:
    explicit shard(config::shard_range_t prefix_range);

    // Open or restore the LevelDB database for this shard
    auto open_db(const std::string& db_dir) -> std::optional<std::string>;

    // Check input validity and produce an atomizer notification or watchtower error
    auto digest_transaction(transaction::compact_tx tx)
        -> std::variant<atomizer::tx_notify_request, watchtower::tx_error>;

    // Apply a new block from the atomizer (delete inputs, add outputs)
    auto digest_block(const cbdc::atomizer::block& blk) -> bool;

    [[nodiscard]] auto best_block_height() const -> uint64_t;
};
```

### Atomizer Core

> **Header:** [`src/uhs/atomizer/atomizer/atomizer.hpp`](../src/uhs/atomizer/atomizer/atomizer.hpp)
> **Namespace:** `cbdc::atomizer`

```cpp
class atomizer {
public:
    atomizer(uint64_t best_height, size_t stxo_cache_depth);

    // Insert shard attestations for a transaction
    [[nodiscard]] auto insert(uint64_t block_height,
                              transaction::compact_tx tx,
                              std::unordered_set<uint32_t> attestations)
        -> std::optional<watchtower::tx_error>;

    // Assemble a new block from complete transactions
    [[nodiscard]] auto make_block()
        -> std::pair<cbdc::atomizer::block, std::vector<watchtower::tx_error>>;

    [[nodiscard]] auto pending_transactions() const -> size_t;
    [[nodiscard]] auto height() const -> uint64_t;

    // State serialization for Raft snapshots
    [[nodiscard]] auto serialize() -> buffer;
    void deserialize(serializer& buf);
};
```

### Connection Manager

> **Header:** [`src/util/network/connection_manager.hpp`](../src/util/network/connection_manager.hpp)
> **Namespace:** `cbdc::network`

Low-level TCP networking utility used by all components.

```cpp
class connection_manager {
public:
    // Start listening for inbound connections
    [[nodiscard]] auto listen(const ip_address& host, unsigned short port) -> bool;

    // Connect to a set of peer endpoints
    auto cluster_connect(const std::vector<endpoint_t>& endpoints,
                         bool error_fatal = true) -> bool;

    // Start a server that handles packets with the given handler
    [[nodiscard]] auto start_server(const endpoint_t& listen_endpoint,
                                    const packet_handler_t& handler)
        -> std::optional<std::thread>;

    // Send data to a specific peer
    void send(const std::shared_ptr<buffer>& data, peer_id_t peer_id);

    // Broadcast data to all connected peers
    void broadcast(const std::shared_ptr<buffer>& data);

    // Send to any one connected peer (round-robin)
    [[nodiscard]] auto send_to_one(const std::shared_ptr<buffer>& data) -> bool;

    void close();
};
```

---

## Configuration API

> **Header:** [`src/util/common/config.hpp`](../src/util/common/config.hpp)
> **Namespace:** `cbdc::config`

### Loading Configuration

```cpp
// Read options from a config file (no invariant checks)
auto read_options(const std::string& config_file)
    -> std::variant<options, std::string>;

// Read and validate options (returns error string on failure)
auto load_options(const std::string& config_file)
    -> std::variant<options, std::string>;

// Validate an options struct
auto check_options(const options& opts) -> std::optional<std::string>;
```

### Config File Parser

```cpp
class parser {
public:
    explicit parser(const std::string& filename);
    explicit parser(std::istream& stream);

    [[nodiscard]] auto get_string(const std::string& key) const
        -> std::optional<std::string>;
    [[nodiscard]] auto get_ulong(const std::string& key) const
        -> std::optional<size_t>;
    [[nodiscard]] auto get_endpoint(const std::string& key) const
        -> std::optional<network::endpoint_t>;
    [[nodiscard]] auto get_loglevel(const std::string& key) const
        -> std::optional<logging::log_level>;
    [[nodiscard]] auto get_decimal(const std::string& key) const
        -> std::optional<double>;
};
```

For a full reference of all configuration parameters, see [configuration.md](configuration.md).
