// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/transaction/validation.hpp"
#include "uhs/transaction/wallet.hpp"

#include <filesystem>
#include <gtest/gtest.h>

class WalletTest : public ::testing::Test {
  protected:
    void SetUp() override {
        auto mint_tx = m_wallet.mint_new_coins(1, 100);
        m_wallet.confirm_transaction(mint_tx);
    }

    void TearDown() override {
        std::filesystem::remove(m_wallet_file);
    }

    cbdc::transaction::wallet m_wallet{};
    static constexpr auto m_wallet_file = "test_wallet.dat";
};

TEST_F(WalletTest, update_balance_basic) {
    cbdc::transaction::input in0;
    in0.m_prevout.m_tx_id = {'e'};
    in0.m_prevout.m_index = 1;
    in0.m_prevout_data.m_value = 14;
    in0.m_prevout_data.m_witness_program_commitment = {'a'};

    cbdc::transaction::input in1;
    in1.m_prevout.m_tx_id = {'p'};
    in1.m_prevout.m_index = 0;
    in1.m_prevout_data.m_value = 22;
    in1.m_prevout_data.m_witness_program_commitment = {'j'};

    m_wallet.confirm_inputs({in0, in1});
    ASSERT_EQ(m_wallet.balance(), uint32_t{136});
}

TEST_F(WalletTest, update_balance_double_credit) {
    cbdc::transaction::input c0;
    c0.m_prevout.m_tx_id = {'e'};
    c0.m_prevout.m_index = 1;
    c0.m_prevout_data.m_value = 14;
    c0.m_prevout_data.m_witness_program_commitment = {'a'};

    m_wallet.confirm_inputs({c0});
    m_wallet.confirm_inputs({c0});
    ASSERT_EQ(m_wallet.balance(), uint32_t{114});
}

TEST_F(WalletTest, export_send_input_basic) {
    cbdc::pubkey_t target_addr = {'a', 'b', 'c', 'd'};
    auto send_tx = m_wallet.send_to(25, target_addr, false).value();
    auto receiver_inputs
        = cbdc::transaction::wallet::export_send_inputs(send_tx, target_addr);
    ASSERT_EQ(receiver_inputs.size(), 1UL);

    ASSERT_EQ(receiver_inputs[0].m_prevout.m_tx_id,
              cbdc::transaction::tx_id(send_tx));
    ASSERT_EQ(receiver_inputs[0].m_prevout_data.m_value, 25U);
}

TEST_F(WalletTest, fan_out) {
    cbdc::pubkey_t target_addr = {'a', 'b', 'c', 'd'};
    auto send_tx = m_wallet.fan(20, 5, target_addr, false).value();
    ASSERT_EQ(send_tx.m_outputs.size(), 20UL);
    auto witcom = cbdc::transaction::validation::get_p2pk_witness_commitment(
        target_addr);
    for(const auto& out : send_tx.m_outputs) {
        ASSERT_EQ(out.m_value, 5UL);
        ASSERT_EQ(out.m_witness_program_commitment, witcom);
    }
    auto receiver_inputs
        = cbdc::transaction::wallet::export_send_inputs(send_tx, target_addr);
    ASSERT_EQ(receiver_inputs.size(), 20UL);
}

TEST_F(WalletTest, fan_out_change) {
    cbdc::pubkey_t target_addr = {'a', 'b', 'c', 'd'};
    auto send_tx = m_wallet.fan(19, 5, target_addr, false).value();
    ASSERT_EQ(send_tx.m_outputs.size(), 20UL);
    auto witcom = cbdc::transaction::validation::get_p2pk_witness_commitment(
        target_addr);
    for(size_t i = 1; i < send_tx.m_outputs.size(); i++) {
        ASSERT_EQ(send_tx.m_outputs[i].m_value, 5UL);
        ASSERT_EQ(send_tx.m_outputs[i].m_witness_program_commitment, witcom);
    }
    ASSERT_EQ(send_tx.m_outputs[0].m_value, 5UL);
    ASSERT_NE(send_tx.m_outputs[0].m_witness_program_commitment, witcom);
    auto receiver_inputs
        = cbdc::transaction::wallet::export_send_inputs(send_tx, target_addr);
    ASSERT_EQ(receiver_inputs.size(), 19UL);
}

class WalletTxTest : public ::testing::Test {
  protected:
    void SetUp() override {
        auto mint_tx = m_sender.mint_new_coins(1, 100);
        m_sender.confirm_transaction(mint_tx);
    }

    cbdc::transaction::wallet m_sender{};
    cbdc::transaction::wallet m_receiver{};
};

TEST_F(WalletTxTest, basic) {
    auto target_key = m_receiver.generate_key();

    auto send_tx = m_sender.send_to(20, target_key, true).value();
    m_sender.confirm_transaction(send_tx);
    m_receiver.confirm_transaction(send_tx);

    ASSERT_EQ(m_sender.balance(), uint32_t{80});
    ASSERT_EQ(m_receiver.balance(), uint32_t{20});
}

class WalletMultiTxTest : public ::testing::Test {
  protected:
    void SetUp() override {
        auto mint_tx = m_sender.mint_new_coins(100, 100);
        m_sender.confirm_transaction(mint_tx);
    }

    cbdc::transaction::wallet m_sender{};
};

TEST_F(WalletMultiTxTest, inp_out_count) {
    auto target_key = m_sender.generate_key();
    auto balance = m_sender.balance();
    auto count = m_sender.count();
    for(size_t inps{1}; inps < 5; inps++) {
        for(size_t outs{1}; outs < 5; outs++) {
            auto send_tx = m_sender.send_to(inps, outs, target_key, true);
            ASSERT_TRUE(send_tx);
            ASSERT_EQ(m_sender.count(), count - inps);
            ASSERT_EQ(send_tx.value().m_inputs.size(), inps);
            ASSERT_EQ(send_tx.value().m_outputs.size(), outs);
            m_sender.confirm_transaction(send_tx.value());
            ASSERT_EQ(m_sender.count(), count - inps + outs);
            ASSERT_EQ(m_sender.balance(), balance);
            count -= inps;
            count += outs;
        }
    }
}

TEST_F(WalletMultiTxTest, insufficient_utxos) {
    auto target_key = m_sender.generate_key();
    auto send_tx = m_sender.send_to(101, 5, target_key, true);
    ASSERT_EQ(send_tx, std::nullopt);
}

TEST_F(WalletMultiTxTest, too_many_outputs) {
    auto target_key = m_sender.generate_key();
    auto send_tx = m_sender.send_to(50, 10001, target_key, true);
    ASSERT_EQ(send_tx, std::nullopt);
}

TEST_F(WalletTest, spend_order) {
    cbdc::transaction::input in0;
    in0.m_prevout.m_tx_id = {'e'};
    in0.m_prevout.m_index = 1;
    in0.m_prevout_data.m_value = 14;
    in0.m_prevout_data.m_witness_program_commitment = {'a'};

    cbdc::transaction::input in1;
    in1.m_prevout.m_tx_id = {'p'};
    in1.m_prevout.m_index = 0;
    in1.m_prevout_data.m_value = 22;
    in1.m_prevout_data.m_witness_program_commitment = {'j'};

    cbdc::transaction::input in2;
    in2.m_prevout.m_tx_id = {'j'};
    in2.m_prevout.m_index = 0;
    in2.m_prevout_data.m_value = 33;
    in2.m_prevout_data.m_witness_program_commitment = {'j'};

    m_wallet.confirm_inputs({in0});
    m_wallet.confirm_inputs({in1});
    m_wallet.confirm_inputs({in2});

    auto pubkey = m_wallet.generate_key();
    m_wallet.send_to(1, 1, pubkey, false);
    auto tx1 = m_wallet.send_to(1, 1, pubkey, false);
    ASSERT_EQ(tx1->m_inputs[0], in0);
    auto tx2 = m_wallet.send_to(1, pubkey, false);
    ASSERT_EQ(tx2->m_inputs[0], in1);
    auto tx3 = m_wallet.send_to(1, 1, pubkey, false);
    ASSERT_EQ(tx3->m_inputs[0], in2);
}

TEST_F(WalletTest, load_save) {
    m_wallet.save(m_wallet_file);
    auto new_wal = cbdc::transaction::wallet();
    new_wal.load(m_wallet_file);
    ASSERT_EQ(m_wallet.balance(), new_wal.balance());
    ASSERT_EQ(m_wallet.count(), new_wal.count());
}

TEST_F(WalletTest, mint_and_verify_balance) {
    cbdc::transaction::wallet w;
    ASSERT_EQ(w.balance(), uint64_t{0});
    ASSERT_EQ(w.count(), 0UL);

    auto mint = w.mint_new_coins(5, 200);
    w.confirm_transaction(mint);

    ASSERT_EQ(w.balance(), uint64_t{1000});
    ASSERT_EQ(w.count(), 5UL);
}

TEST_F(WalletTest, send_and_verify_change) {
    cbdc::transaction::wallet receiver;
    auto target = receiver.generate_key();
    auto send_tx = m_wallet.send_to(30, target, true).value();
    m_wallet.confirm_transaction(send_tx);

    ASSERT_EQ(m_wallet.balance(), uint64_t{70});

    receiver.confirm_transaction(send_tx);
    ASSERT_EQ(receiver.balance(), uint64_t{30});
}

TEST_F(WalletTest, overspend_insufficient_funds) {
    auto target = m_wallet.generate_key();
    auto send_tx = m_wallet.send_to(200, target, true);
    ASSERT_FALSE(send_tx.has_value());
    ASSERT_EQ(m_wallet.balance(), uint64_t{100});
}

TEST_F(WalletTest, multiple_sequential_transactions) {
    cbdc::transaction::wallet receiver;

    auto tx1 = m_wallet.send_to(10, receiver.generate_key(), true).value();
    m_wallet.confirm_transaction(tx1);
    receiver.confirm_transaction(tx1);
    ASSERT_EQ(m_wallet.balance(), uint64_t{90});
    ASSERT_EQ(receiver.balance(), uint64_t{10});

    auto tx2 = m_wallet.send_to(20, receiver.generate_key(), true).value();
    m_wallet.confirm_transaction(tx2);
    receiver.confirm_transaction(tx2);
    ASSERT_EQ(m_wallet.balance(), uint64_t{70});
    ASSERT_EQ(receiver.balance(), uint64_t{30});

    auto tx3 = m_wallet.send_to(30, receiver.generate_key(), true).value();
    m_wallet.confirm_transaction(tx3);
    receiver.confirm_transaction(tx3);
    ASSERT_EQ(m_wallet.balance(), uint64_t{40});
    ASSERT_EQ(receiver.balance(), uint64_t{60});
}

TEST_F(WalletTest, key_generation_uniqueness) {
    auto key1 = m_wallet.generate_key();
    auto key2 = m_wallet.generate_key();
    auto key3 = m_wallet.generate_key();

    ASSERT_NE(key1, key2);
    ASSERT_NE(key1, key3);
    ASSERT_NE(key2, key3);
}

TEST_F(WalletTest, send_exact_balance) {
    cbdc::transaction::wallet receiver;
    auto tx
        = m_wallet.send_to(100, receiver.generate_key(), true).value();
    m_wallet.confirm_transaction(tx);
    receiver.confirm_transaction(tx);

    ASSERT_EQ(m_wallet.balance(), uint64_t{0});
    ASSERT_EQ(m_wallet.count(), 0UL);
    ASSERT_EQ(receiver.balance(), uint64_t{100});
}

TEST_F(WalletTest, send_validated_transaction) {
    cbdc::transaction::wallet receiver;
    auto tx
        = m_wallet.send_to(50, receiver.generate_key(), true).value();

    auto err = cbdc::transaction::validation::check_tx(tx);
    ASSERT_FALSE(err.has_value());
}
