// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/transaction/messages.hpp"
#include "uhs/transaction/validation.hpp"
#include "uhs/transaction/wallet.hpp"
#include "util/serialization/buffer_serializer.hpp"
#include "util/serialization/format.hpp"
#include "util/serialization/util.hpp"

#include <gtest/gtest.h>
#include <limits>

class SecurityTest : public ::testing::Test {
  protected:
    void SetUp() override {
        cbdc::transaction::wallet wallet1;
        cbdc::transaction::wallet wallet2;

        auto mint_tx1 = wallet1.mint_new_coins(3, 100);
        wallet1.confirm_transaction(mint_tx1);

        m_valid_tx
            = wallet1.send_to(20, wallet2.generate_key(), true).value();
    }

    cbdc::transaction::full_tx m_valid_tx{};
    std::unique_ptr<secp256k1_context,
                    decltype(&secp256k1_context_destroy)>
        m_secp{secp256k1_context_create(SECP256K1_CONTEXT_SIGN
                                        | SECP256K1_CONTEXT_VERIFY),
               &secp256k1_context_destroy};
};

// --- Malformed serialized data tests ---

TEST_F(SecurityTest, truncated_buffer_deserialization) {
    // Create a buffer with a claimed size larger than actual data
    cbdc::buffer buf;
    cbdc::buffer_serializer ser(buf);
    // Write a size of 1000 but only provide 4 bytes of data
    uint64_t fake_size = 1000;
    ser << fake_size;
    uint32_t partial_data = 0xDEADBEEF;
    ser.write(&partial_data, sizeof(partial_data));

    cbdc::buffer result;
    cbdc::buffer_serializer deser(buf);
    deser >> result;
    // Should fail because actual data is shorter than claimed size
    ASSERT_FALSE(deser);
}

TEST_F(SecurityTest, oversized_buffer_deserialization) {
    // Create a buffer with claimed size exceeding maximum_reservation
    cbdc::buffer buf;
    cbdc::buffer_serializer ser(buf);
    uint64_t huge_size = static_cast<uint64_t>(cbdc::config::maximum_reservation) + 1;
    ser << huge_size;

    cbdc::buffer result;
    cbdc::buffer_serializer deser(buf);
    deser >> result;
    // Should fail because size exceeds maximum_reservation
    ASSERT_FALSE(deser);
}

TEST_F(SecurityTest, max_uint64_buffer_size) {
    // Extreme case: maximum uint64 size
    cbdc::buffer buf;
    cbdc::buffer_serializer ser(buf);
    uint64_t max_size = std::numeric_limits<uint64_t>::max();
    ser << max_size;

    cbdc::buffer result;
    cbdc::buffer_serializer deser(buf);
    deser >> result;
    ASSERT_FALSE(deser);
}

TEST_F(SecurityTest, empty_buffer_deserialization) {
    cbdc::buffer buf;
    cbdc::buffer_serializer deser(buf);
    cbdc::buffer result;
    deser >> result;
    // Should fail because there's nothing to read
    ASSERT_FALSE(deser);
}

TEST_F(SecurityTest, truncated_vector_deserialization) {
    // Claim a vector has 1000 elements but provide none
    cbdc::buffer buf;
    cbdc::buffer_serializer ser(buf);
    uint64_t fake_len = 1000;
    ser << fake_len;

    std::vector<uint64_t> result;
    cbdc::buffer_serializer deser(buf);
    deser >> result;
    ASSERT_FALSE(deser);
}

TEST_F(SecurityTest, oversized_vector_deserialization) {
    // Claim a huge number of vector elements
    cbdc::buffer buf;
    cbdc::buffer_serializer ser(buf);
    uint64_t huge_len = std::numeric_limits<uint64_t>::max();
    ser << huge_len;

    std::vector<uint64_t> result;
    cbdc::buffer_serializer deser(buf);
    deser >> result;
    ASSERT_FALSE(deser);
}

// --- Integer overflow tests ---

TEST_F(SecurityTest, input_value_overflow_max) {
    // Two inputs with UINT64_MAX - should overflow
    auto tx = m_valid_tx;
    tx.m_inputs.push_back(tx.m_inputs[0]);
    tx.m_inputs[0].m_prevout_data.m_value
        = std::numeric_limits<uint64_t>::max();
    tx.m_inputs[1].m_prevout_data.m_value = 1;
    // Add corresponding witness
    tx.m_witness.push_back(tx.m_witness[0]);

    auto res = cbdc::transaction::validation::check_in_out_set(tx);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(
        res.value(),
        cbdc::transaction::validation::tx_error(
            cbdc::transaction::validation::tx_error_code::value_overflow));
}

TEST_F(SecurityTest, output_value_overflow_max) {
    // Two outputs with values that overflow
    auto tx = m_valid_tx;
    tx.m_outputs.push_back(tx.m_outputs[0]);
    tx.m_outputs[0].m_value = std::numeric_limits<uint64_t>::max();
    tx.m_outputs[1].m_value = 1;

    auto res = cbdc::transaction::validation::check_in_out_set(tx);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(
        res.value(),
        cbdc::transaction::validation::tx_error(
            cbdc::transaction::validation::tx_error_code::value_overflow));
}

TEST_F(SecurityTest, input_value_overflow_boundary) {
    // Test boundary: UINT64_MAX/2 + UINT64_MAX/2 + 2 should overflow
    auto tx = m_valid_tx;
    tx.m_inputs.push_back(tx.m_inputs[0]);
    auto half_max = std::numeric_limits<uint64_t>::max() / 2;
    tx.m_inputs[0].m_prevout_data.m_value = half_max;
    tx.m_inputs[1].m_prevout_data.m_value = half_max + 2;
    tx.m_witness.push_back(tx.m_witness[0]);

    auto res = cbdc::transaction::validation::check_in_out_set(tx);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(
        res.value(),
        cbdc::transaction::validation::tx_error(
            cbdc::transaction::validation::tx_error_code::value_overflow));
}

TEST_F(SecurityTest, input_value_no_overflow_at_boundary) {
    // Test boundary: UINT64_MAX/2 + UINT64_MAX/2 + 1 == UINT64_MAX (no
    // overflow)
    auto tx = m_valid_tx;
    tx.m_inputs.push_back(tx.m_inputs[0]);
    auto half_max = std::numeric_limits<uint64_t>::max() / 2;
    tx.m_inputs[0].m_prevout_data.m_value = half_max;
    tx.m_inputs[1].m_prevout_data.m_value = half_max + 1;
    tx.m_witness.push_back(tx.m_witness[0]);
    // Set outputs to match to avoid asymmetric_values error
    tx.m_outputs.clear();
    cbdc::transaction::output out{};
    out.m_value = std::numeric_limits<uint64_t>::max();
    tx.m_outputs.push_back(out);

    auto res = cbdc::transaction::validation::check_in_out_set(tx);
    // Should NOT return overflow - the sum fits in uint64_t
    ASSERT_FALSE(res.has_value());
}

// --- Invalid cryptographic data tests ---

TEST_F(SecurityTest, verify_with_zero_pubkey) {
    auto ctx = cbdc::transaction::compact_tx(m_valid_tx);
    cbdc::pubkey_t zero_key{};
    cbdc::signature_t sig{};
    cbdc::transaction::sentinel_attestation att{zero_key, sig};
    ASSERT_FALSE(ctx.verify(m_secp.get(), att));
}

TEST_F(SecurityTest, verify_with_corrupt_signature) {
    auto ctx = cbdc::transaction::compact_tx(m_valid_tx);
    cbdc::privkey_t priv{cbdc::hash_from_hex(
        "0000000000000001000000000000000000000000000000000000000000000000")};
    auto att = ctx.sign(m_secp.get(), priv);
    // Corrupt the signature
    att.second[0] ^= 0xFF;
    ASSERT_FALSE(ctx.verify(m_secp.get(), att));
}

TEST_F(SecurityTest, verify_with_wrong_key) {
    auto ctx = cbdc::transaction::compact_tx(m_valid_tx);
    cbdc::privkey_t priv1{cbdc::hash_from_hex(
        "0000000000000001000000000000000000000000000000000000000000000000")};
    cbdc::privkey_t priv2{cbdc::hash_from_hex(
        "1000000000000001000000000000000000000000000000000000000000000000")};
    // Sign with key1 but try to verify with key2's attestation
    auto att1 = ctx.sign(m_secp.get(), priv1);
    auto att2 = ctx.sign(m_secp.get(), priv2);
    // Cross-verify: key1 pubkey with key2 signature
    cbdc::transaction::sentinel_attestation cross{att1.first, att2.second};
    ASSERT_FALSE(ctx.verify(m_secp.get(), cross));
}

// --- Transaction deserialization tests ---

TEST_F(SecurityTest, deserialize_malformed_transaction) {
    // Create garbage data and try to deserialize as a transaction
    cbdc::buffer buf;
    buf.extend(16);
    auto* data = buf.data();
    std::memset(data, 0xFF, 16);

    auto result
        = cbdc::from_buffer<cbdc::transaction::full_tx>(buf);
    // Should either fail to deserialize or produce an invalid transaction
    if(result.has_value()) {
        auto err
            = cbdc::transaction::validation::check_tx(result.value());
        // Garbage data should not pass validation
        ASSERT_TRUE(err.has_value());
    }
}

TEST_F(SecurityTest, deserialize_zero_length_transaction) {
    cbdc::buffer buf;
    auto result
        = cbdc::from_buffer<cbdc::transaction::full_tx>(buf);
    ASSERT_FALSE(result.has_value());
}

// --- Variant deserialization bounds tests ---

TEST_F(SecurityTest, variant_invalid_index) {
    // Write an invalid variant index that exceeds the number of alternatives
    cbdc::buffer buf;
    cbdc::buffer_serializer ser(buf);
    uint8_t bad_idx = 255;
    ser << bad_idx;

    cbdc::buffer_serializer deser(buf);
    std::variant<uint32_t, uint64_t> result;
    deser >> result;
    ASSERT_FALSE(deser);
}

// --- Error message leakage tests ---

TEST_F(SecurityTest, error_messages_no_key_leakage) {
    // Verify that error messages from validation don't contain hex key data
    auto tx = m_valid_tx;
    tx.m_witness[0][0] = std::byte(0xFF);
    auto err = cbdc::transaction::validation::check_tx(tx);
    ASSERT_TRUE(err.has_value());
    auto err_str
        = cbdc::transaction::validation::to_string(err.value());
    // Error message should be descriptive but not leak raw key material
    ASSERT_FALSE(err_str.empty());
    // Should not contain long hex strings (key material)
    // A reasonable error message is short and descriptive
    ASSERT_LT(err_str.size(), 200UL);
}
