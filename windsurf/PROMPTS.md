# Windsurf Prompts for opencbdc-tx

> This file contains ready-to-use prompts for [Windsurf](https://windsurf.com), Cognition's AI-powered IDE. Open this repo in Windsurf, pick a prompt below, and paste it into Cascade. Each prompt includes context on what problem it solves and what Windsurf will do.
>
> Prompts are ordered from simple → complex so you can start easy and build confidence.

---

## Prompt 1 — Navigate the CBDC Architecture (Beginner)
**What this solves:** opencbdc-tx is a research-grade CBDC (Central Bank Digital Currency) transaction processor from MIT's Digital Currency Initiative — built in collaboration with the Boston Fed. The C++ codebase is large and architecturally complex. You need a map before you can contribute.

**What Windsurf will do:** Analyze the entire codebase and give you a clear architectural overview — what each component does, how transactions flow through the system, and where to look for specific functionality.

**Paste this into Cascade:**
```
I need to understand this CBDC transaction processor. Give me a comprehensive architectural walkthrough:

1. **High-level purpose**: What does this system do? What problem does it solve for central banks?
2. **The two architectures**: The README mentions two architectures — "atomizer" and "2-phase commit". Explain both:
   - How does each one process a transaction from submission to confirmation?
   - What are the trade-offs (throughput vs latency vs consistency)?
   - Which source directories implement which architecture?
3. **Core data structures**: What does a Transaction look like in this codebase? Find the main struct/class and explain its fields. What about Block, UHS (Unspent Hash Set)?
4. **Component map**: Walk through the src/ directory. For each major subdirectory, explain:
   - What component it implements
   - What its role is in the transaction pipeline
   - How it communicates with other components
5. **Build system**: How do I build this project? What dependencies does it need? Walk me through the CMakeLists.txt structure.
6. **Testing**: Where are the tests? How do I run them? What do they cover?

Draw me an ASCII architecture diagram showing the components and transaction flow.
```

---

## Prompt 2 — Add Input Validation to Transaction Processing (Intermediate)
**What this solves:** In a financial system, every input from the network must be validated before processing. Malformed or malicious transaction data could cause crashes, undefined behavior, or security vulnerabilities. You need defense-in-depth validation.

**What Windsurf will do:** Identify the transaction deserialization and validation code, audit it for gaps, and add robust bounds checking and validation.

**Paste this into Cascade:**
```
I need to harden the transaction input validation in this CBDC processor. This is financial infrastructure — we can't trust any input from the network.

1. Find where transactions are deserialized from network bytes. Trace the code path from "bytes arrive on the network" to "transaction object is created."

2. For each field in the transaction, check whether the code validates:
   - Transaction size (total byte length) — is there a maximum?
   - Number of inputs/outputs — is there a limit to prevent DoS?
   - Amount values — are they checked for overflow, negative values, or zero?
   - Hash/signature lengths — are they verified before use?
   - Any variable-length fields — are lengths checked before allocation?

3. For each missing validation, add it:
   - Define reasonable limits as named constants (e.g., MAX_TX_INPUTS = 1024)
   - Return clear error codes (not assertions or crashes) for invalid input
   - Log validation failures at WARNING level for monitoring

4. Add unit tests for each validation:
   - Valid transaction passes validation
   - Each invalid case is caught and returns the correct error
   - Boundary values (exactly at the limit, one over the limit)

Show me each validation gap you found and the fix. Explain the security impact of each gap.
```

---

## Prompt 3 — Write Performance Benchmarks (Advanced)
**What this solves:** This is a high-performance transaction processor, but there are no standardized benchmarks. Developers making changes need to know if they've improved or regressed performance. Central banks evaluating this need throughput and latency numbers.

**What Windsurf will do:** Create a benchmarking framework that measures the key performance metrics of the transaction processing pipeline.

**Paste this into Cascade:**
```
Create a performance benchmarking suite for this CBDC transaction processor. I need reproducible measurements for:

1. **Transaction throughput benchmark** (benchmarks/bench_throughput.cpp):
   - Generate N synthetic valid transactions (configurable, default 100,000)
   - Measure end-to-end processing time: submission → validation → ordering → confirmation
   - Report: transactions/second, p50/p95/p99 latency per transaction
   - Test with 1, 4, 8, and 16 threads to show scaling behavior

2. **Serialization benchmark** (benchmarks/bench_serialization.cpp):
   - Measure transaction serialization and deserialization speed
   - Test with varying transaction sizes (1 input/1 output up to 100 inputs/100 outputs)
   - Report: operations/second, bytes/second

3. **Hash computation benchmark** (benchmarks/bench_hash.cpp):
   - Measure UHS (Unspent Hash Set) lookup performance
   - Test with varying set sizes (1K, 10K, 100K, 1M entries)
   - Report: lookups/second, memory usage

4. **Benchmark runner** (benchmarks/CMakeLists.txt + benchmarks/run_benchmarks.sh):
   - Integrate with CMake build system
   - Shell script that runs all benchmarks and produces a summary report
   - Output results in both human-readable and CSV format for tracking over time

Use Google Benchmark if it's already a dependency, otherwise use simple chrono-based timing. Include a README in benchmarks/ explaining how to run and interpret results.
```

---

## Prompt 4 — Security Audit: Cryptographic Operations Review (Expert)
**What this solves:** A CBDC system handles digital currency for a central bank — security bugs could have catastrophic financial consequences. The cryptographic operations (hashing, signing, verification) need to be reviewed for correctness, timing attacks, and proper key handling.

**What Windsurf will do:** Perform a thorough security-focused code review of all cryptographic code paths, identify vulnerabilities, and implement fixes.

**Paste this into Cascade:**
```
I need a security audit of the cryptographic operations in this CBDC transaction processor. This is central bank financial infrastructure — any crypto bug is critical severity.

Review every file that touches cryptographic operations and check:

1. **Constant-time comparisons**: Find all hash/signature comparisons. Are they using constant-time comparison functions (like `CRYPTO_memcmp` or equivalent)? Variable-time comparison enables timing attacks that can leak secret material. Flag every `==` or `memcmp` on crypto data.

2. **Random number generation**: Find all random number generation. Is it using a CSPRNG (cryptographically secure PRNG)? Flag any use of `rand()`, `mt19937`, or other non-cryptographic sources for security-relevant randomness.

3. **Key/secret material handling**: 
   - Are private keys zeroed after use? (Check for `memset_s`, `explicit_bzero`, or `SecureZeroMemory`)
   - Are secrets ever logged or included in error messages?
   - Are secrets allocated on the heap? (Heap memory can be swapped to disk)

4. **Hash function usage**:
   - What hash function is used? Is it collision-resistant for this application?
   - Are there any length-extension attack vulnerabilities?
   - Is the hash function consistently applied (same algorithm everywhere)?

5. **Signature verification**:
   - Is signature verification performed before any transaction processing?
   - Can signature verification be bypassed through any code path?
   - Are malleable signatures handled correctly?

For each finding: describe the vulnerability, rate severity (Critical/High/Medium/Low), show the vulnerable code, and provide the fix. Order findings by severity.
```

---
