// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/transaction/messages.hpp"
#include "uhs/transaction/transaction.hpp"
#include "uhs/transaction/wallet.hpp"
#include "util/raft/serialization.hpp"
#include "util/serialization/buffer_serializer.hpp"
#include "util/serialization/format.hpp"
#include "util/serialization/size_serializer.hpp"
#include "util/serialization/util.hpp"

#include <gtest/gtest.h>
#include <utility>

class SerializationTest : public ::testing::Test {
  protected:
    SerializationTest() : ser(buf), deser(buf) {}

    void SetUp() override {
        ser.reset();
        deser.reset();
    }

    cbdc::buffer buf;
    cbdc::buffer_serializer ser;
    cbdc::buffer_serializer deser;
};

TEST_F(SerializationTest, TestIntegralPacket) {
    uint64_t val1{27};
    uint64_t val2{28};

    ser << val1 << val2;

    ASSERT_EQ(buf.size(), sizeof(val1) + sizeof(val2));

    uint64_t test_val1{};
    uint64_t test_val2{};

    deser >> test_val1 >> test_val2;

    ASSERT_EQ(val1, test_val1);
    ASSERT_EQ(val2, test_val2);
}

TEST_F(SerializationTest, TestIntegralNuraft) {
    auto arr = nuraft::buffer::alloc(sizeof(uint64_t) * 2);

    uint64_t val1{27};
    uint64_t val2{28};

    ser << val1 << val2;

    uint64_t test_val1{};
    uint64_t test_val2{};

    deser >> test_val1 >> test_val2;

    ASSERT_EQ(val1, test_val1);
    ASSERT_EQ(val2, test_val2);
}

TEST_F(SerializationTest, TestDummy) {
    uint32_t v0 = 0;
    uint64_t v1 = 2;
    ser << v0 << v1;

    auto sz = cbdc::size_serializer();
    sz << v0 << v1;
    ASSERT_EQ(buf.size(), sz.size());

    ASSERT_FALSE(sz.read(nullptr, 0));
    ASSERT_TRUE(sz);
    ASSERT_FALSE(sz.end_of_buffer());

    sz.reset();
    ASSERT_EQ(sz.size(), 0UL);

    sz.advance_cursor(10);
    ASSERT_EQ(sz.size(), 10UL);
}

TEST_F(SerializationTest, TestEndOfBuffer) {
    uint32_t v0 = 0;
    uint64_t v1 = 2;
    ser << v0 << v1;

    ser.reset();
    ASSERT_FALSE(ser.end_of_buffer());
    ser.advance_cursor(12);
    ASSERT_TRUE(ser.end_of_buffer());
}

TEST_F(SerializationTest, TestReadOutOfBounds) {
    uint32_t v0 = 0;
    uint64_t v1 = 2;
    ser << v0 << v1;

    deser.advance_cursor(12);
    ASSERT_FALSE(deser.read(nullptr, 10));
    ASSERT_FALSE(deser);
}

TEST(TxSerializationTest, full_tx_round_trip_single) {
    cbdc::transaction::wallet w;
    auto mint = w.mint_new_coins(1, 100);
    w.confirm_transaction(mint);

    cbdc::transaction::wallet w2;
    auto tx = w.send_to(50, w2.generate_key(), true).value();

    auto serialized = cbdc::make_buffer(tx);
    ASSERT_GT(serialized.size(), 0UL);

    auto deserialized
        = cbdc::from_buffer<cbdc::transaction::full_tx>(serialized);
    ASSERT_TRUE(deserialized.has_value());
    ASSERT_EQ(deserialized.value(), tx);
}

TEST(TxSerializationTest, full_tx_round_trip_multiple) {
    cbdc::transaction::wallet w;
    auto mint = w.mint_new_coins(10, 100);
    w.confirm_transaction(mint);

    cbdc::transaction::wallet w2;
    auto tx = w.send_to(5, 5, w2.generate_key(), true);
    ASSERT_TRUE(tx.has_value());

    auto serialized = cbdc::make_buffer(tx.value());
    ASSERT_GT(serialized.size(), 0UL);

    auto deserialized
        = cbdc::from_buffer<cbdc::transaction::full_tx>(serialized);
    ASSERT_TRUE(deserialized.has_value());
    ASSERT_EQ(deserialized.value(), tx.value());
}

TEST(TxSerializationTest, compact_tx_round_trip) {
    cbdc::transaction::wallet w;
    auto mint = w.mint_new_coins(1, 100);
    w.confirm_transaction(mint);

    cbdc::transaction::wallet w2;
    auto tx = w.send_to(50, w2.generate_key(), true).value();
    auto ctx = cbdc::transaction::compact_tx(tx);

    auto serialized = cbdc::make_buffer(ctx);
    ASSERT_GT(serialized.size(), 0UL);

    auto deserialized
        = cbdc::from_buffer<cbdc::transaction::compact_tx>(serialized);
    ASSERT_TRUE(deserialized.has_value());
    ASSERT_EQ(deserialized.value(), ctx);
    ASSERT_EQ(deserialized->m_inputs.size(), ctx.m_inputs.size());
    ASSERT_EQ(deserialized->m_uhs_outputs.size(), ctx.m_uhs_outputs.size());
}

TEST(TxSerializationTest, empty_buffer_deserialization) {
    cbdc::buffer empty_buf;

    auto result
        = cbdc::from_buffer<cbdc::transaction::full_tx>(empty_buf);
    ASSERT_FALSE(result.has_value());
}

TEST(TxSerializationTest, truncated_buffer_deserialization) {
    cbdc::transaction::wallet w;
    auto mint = w.mint_new_coins(1, 100);
    w.confirm_transaction(mint);

    cbdc::transaction::wallet w2;
    auto tx = w.send_to(50, w2.generate_key(), true).value();

    auto serialized = cbdc::make_buffer(tx);

    // Truncate the buffer to half its size
    cbdc::buffer truncated;
    truncated.extend(serialized.size() / 2);
    std::memcpy(truncated.data(), serialized.data(), serialized.size() / 2);

    auto result
        = cbdc::from_buffer<cbdc::transaction::full_tx>(truncated);
    ASSERT_FALSE(result.has_value());
}

TEST(TxSerializationTest, large_tx_round_trip) {
    cbdc::transaction::wallet w;
    auto mint = w.mint_new_coins(50, 100);
    w.confirm_transaction(mint);

    cbdc::transaction::wallet w2;
    auto tx = w.send_to(30, 30, w2.generate_key(), true);
    ASSERT_TRUE(tx.has_value());

    auto serialized = cbdc::make_buffer(tx.value());
    ASSERT_GT(serialized.size(), 0UL);

    auto deserialized
        = cbdc::from_buffer<cbdc::transaction::full_tx>(serialized);
    ASSERT_TRUE(deserialized.has_value());
    ASSERT_EQ(deserialized.value(), tx.value());
}

TEST(TxSerializationTest, compact_tx_empty_round_trip) {
    cbdc::transaction::full_tx empty_tx;
    auto ctx = cbdc::transaction::compact_tx(empty_tx);

    auto serialized = cbdc::make_buffer(ctx);
    ASSERT_GT(serialized.size(), 0UL);

    auto deserialized
        = cbdc::from_buffer<cbdc::transaction::compact_tx>(serialized);
    ASSERT_TRUE(deserialized.has_value());
    ASSERT_EQ(deserialized.value(), ctx);
    ASSERT_TRUE(deserialized->m_inputs.empty());
    ASSERT_TRUE(deserialized->m_uhs_outputs.empty());
}
