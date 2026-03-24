// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/transaction/transaction.hpp"

#include <gtest/gtest.h>
#include <limits>

TEST(CTransaction, input_from_output_basic) {
    cbdc::transaction::full_tx tx;
    cbdc::transaction::output send_out;
    cbdc::transaction::output receive_out;

    send_out.m_value = 40;
    send_out.m_witness_program_commitment = {'a', 'b', 'c', 'd'};
    tx.m_outputs.push_back(send_out);

    receive_out.m_value = 60;
    receive_out.m_witness_program_commitment = {'e', 'f', 'g', 'h'};
    tx.m_outputs.push_back(receive_out);

    auto send_result = cbdc::transaction::input_from_output(tx, 0);
    ASSERT_TRUE(send_result);
    ASSERT_EQ(send_result->m_prevout.m_tx_id, cbdc::transaction::tx_id(tx));
    ASSERT_EQ(send_result->m_prevout.m_index, uint32_t{0});
    ASSERT_EQ(send_result->m_prevout_data.m_value, uint32_t{40});

    auto receive_result = cbdc::transaction::input_from_output(tx, 1);
    ASSERT_TRUE(receive_result);
    ASSERT_EQ(receive_result->m_prevout.m_tx_id, cbdc::transaction::tx_id(tx));
    ASSERT_EQ(receive_result->m_prevout.m_index, uint32_t{1});
    ASSERT_EQ(receive_result->m_prevout_data.m_value, uint32_t{60});
}

TEST(CTransaction, input_from_output_out_of_bounds) {
    cbdc::transaction::full_tx tx;

    auto result = cbdc::transaction::input_from_output(tx, 1);
    ASSERT_FALSE(result);
}

TEST(CTransaction, multiple_inputs_and_outputs) {
    cbdc::transaction::full_tx tx;

    for(uint64_t i = 0; i < 5; i++) {
        cbdc::transaction::output out;
        out.m_value = (i + 1) * 10;
        out.m_witness_program_commitment = {static_cast<unsigned char>(i)};
        tx.m_outputs.push_back(out);
    }

    ASSERT_EQ(tx.m_outputs.size(), 5UL);

    for(size_t i = 0; i < tx.m_outputs.size(); i++) {
        auto inp = cbdc::transaction::input_from_output(tx, i);
        ASSERT_TRUE(inp);
        ASSERT_EQ(inp->m_prevout.m_index, i);
        ASSERT_EQ(inp->m_prevout_data.m_value, (i + 1) * 10);
        ASSERT_EQ(inp->m_prevout.m_tx_id, cbdc::transaction::tx_id(tx));
    }

    auto out_of_range = cbdc::transaction::input_from_output(tx, 5);
    ASSERT_FALSE(out_of_range);
}

TEST(CTransaction, tx_id_consistency) {
    cbdc::transaction::full_tx tx;
    cbdc::transaction::output out;
    out.m_value = 100;
    out.m_witness_program_commitment = {'x', 'y', 'z'};
    tx.m_outputs.push_back(out);

    auto id1 = cbdc::transaction::tx_id(tx);
    auto id2 = cbdc::transaction::tx_id(tx);
    ASSERT_EQ(id1, id2);
}

TEST(CTransaction, tx_id_different_for_different_tx) {
    cbdc::transaction::full_tx tx1;
    cbdc::transaction::output out1;
    out1.m_value = 100;
    out1.m_witness_program_commitment = {'a'};
    tx1.m_outputs.push_back(out1);

    cbdc::transaction::full_tx tx2;
    cbdc::transaction::output out2;
    out2.m_value = 200;
    out2.m_witness_program_commitment = {'b'};
    tx2.m_outputs.push_back(out2);

    ASSERT_NE(cbdc::transaction::tx_id(tx1), cbdc::transaction::tx_id(tx2));
}

TEST(CTransaction, compact_tx_from_full_tx) {
    cbdc::transaction::full_tx tx;

    cbdc::transaction::input inp;
    inp.m_prevout.m_tx_id = {'a', 'b'};
    inp.m_prevout.m_index = 0;
    inp.m_prevout_data.m_value = 50;
    inp.m_prevout_data.m_witness_program_commitment = {'c', 'd'};
    tx.m_inputs.push_back(inp);

    cbdc::transaction::output out;
    out.m_value = 50;
    out.m_witness_program_commitment = {'e', 'f'};
    tx.m_outputs.push_back(out);

    auto ctx = cbdc::transaction::compact_tx(tx);

    ASSERT_EQ(ctx.m_id, cbdc::transaction::tx_id(tx));
    ASSERT_EQ(ctx.m_inputs.size(), 1UL);
    ASSERT_EQ(ctx.m_uhs_outputs.size(), 1UL);
    ASSERT_EQ(ctx.m_inputs[0], inp.hash());
}

TEST(CTransaction, compact_tx_multiple_inputs_outputs) {
    cbdc::transaction::full_tx tx;

    for(uint64_t i = 0; i < 3; i++) {
        cbdc::transaction::input inp;
        inp.m_prevout.m_tx_id = {static_cast<unsigned char>(i)};
        inp.m_prevout.m_index = i;
        inp.m_prevout_data.m_value = 10;
        inp.m_prevout_data.m_witness_program_commitment
            = {static_cast<unsigned char>(i + 10)};
        tx.m_inputs.push_back(inp);
    }

    for(uint64_t i = 0; i < 3; i++) {
        cbdc::transaction::output out;
        out.m_value = 10;
        out.m_witness_program_commitment
            = {static_cast<unsigned char>(i + 20)};
        tx.m_outputs.push_back(out);
    }

    auto ctx = cbdc::transaction::compact_tx(tx);

    ASSERT_EQ(ctx.m_id, cbdc::transaction::tx_id(tx));
    ASSERT_EQ(ctx.m_inputs.size(), 3UL);
    ASSERT_EQ(ctx.m_uhs_outputs.size(), 3UL);

    for(size_t i = 0; i < 3; i++) {
        ASSERT_EQ(ctx.m_inputs[i], tx.m_inputs[i].hash());
    }
}

TEST(CTransaction, empty_transaction) {
    cbdc::transaction::full_tx tx;

    ASSERT_TRUE(tx.m_inputs.empty());
    ASSERT_TRUE(tx.m_outputs.empty());
    ASSERT_TRUE(tx.m_witness.empty());

    auto id = cbdc::transaction::tx_id(tx);
    cbdc::hash_t zero_hash{};
    ASSERT_NE(id, zero_hash);

    auto ctx = cbdc::transaction::compact_tx(tx);
    ASSERT_EQ(ctx.m_id, id);
    ASSERT_TRUE(ctx.m_inputs.empty());
    ASSERT_TRUE(ctx.m_uhs_outputs.empty());
}

TEST(CTransaction, zero_value_output) {
    cbdc::transaction::full_tx tx;
    cbdc::transaction::output out;
    out.m_value = 0;
    out.m_witness_program_commitment = {'a'};
    tx.m_outputs.push_back(out);

    auto inp = cbdc::transaction::input_from_output(tx, 0);
    ASSERT_TRUE(inp);
    ASSERT_EQ(inp->m_prevout_data.m_value, uint64_t{0});
}

TEST(CTransaction, max_uint64_value_output) {
    cbdc::transaction::full_tx tx;
    cbdc::transaction::output out;
    out.m_value = std::numeric_limits<uint64_t>::max();
    out.m_witness_program_commitment = {'m', 'a', 'x'};
    tx.m_outputs.push_back(out);

    auto inp = cbdc::transaction::input_from_output(tx, 0);
    ASSERT_TRUE(inp);
    ASSERT_EQ(inp->m_prevout_data.m_value,
              std::numeric_limits<uint64_t>::max());

    auto id = cbdc::transaction::tx_id(tx);
    cbdc::hash_t zero_hash{};
    ASSERT_NE(id, zero_hash);
}

TEST(CTransaction, compact_tx_equality) {
    cbdc::transaction::full_tx tx;
    cbdc::transaction::output out;
    out.m_value = 42;
    out.m_witness_program_commitment = {'t'};
    tx.m_outputs.push_back(out);

    auto ctx1 = cbdc::transaction::compact_tx(tx);
    auto ctx2 = cbdc::transaction::compact_tx(tx);
    ASSERT_EQ(ctx1, ctx2);
}

TEST(CTransaction, input_from_output_with_txid) {
    cbdc::transaction::full_tx tx;
    cbdc::transaction::output out;
    out.m_value = 77;
    out.m_witness_program_commitment = {'p'};
    tx.m_outputs.push_back(out);

    auto txid = cbdc::transaction::tx_id(tx);
    auto inp1 = cbdc::transaction::input_from_output(tx, 0, txid);
    auto inp2 = cbdc::transaction::input_from_output(tx, 0);

    ASSERT_TRUE(inp1);
    ASSERT_TRUE(inp2);
    ASSERT_EQ(*inp1, *inp2);
}

TEST(CTransaction, output_equality) {
    cbdc::transaction::output out1;
    out1.m_value = 100;
    out1.m_witness_program_commitment = {'a'};

    cbdc::transaction::output out2;
    out2.m_value = 100;
    out2.m_witness_program_commitment = {'a'};

    cbdc::transaction::output out3;
    out3.m_value = 200;
    out3.m_witness_program_commitment = {'a'};

    ASSERT_EQ(out1, out2);
    ASSERT_NE(out1, out3);
}

TEST(CTransaction, full_tx_equality) {
    cbdc::transaction::full_tx tx1;
    cbdc::transaction::output out;
    out.m_value = 50;
    out.m_witness_program_commitment = {'z'};
    tx1.m_outputs.push_back(out);

    cbdc::transaction::full_tx tx2;
    tx2.m_outputs.push_back(out);

    ASSERT_EQ(tx1, tx2);

    cbdc::transaction::output out2;
    out2.m_value = 99;
    out2.m_witness_program_commitment = {'y'};
    tx2.m_outputs.push_back(out2);

    ASSERT_FALSE(tx1 == tx2);
}
