// Copyright (c) 2012-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "../machine.h"
#include "scriptnum10.h"


#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>
#include <climits>
#include <cstdint>

using namespace script;

static const unsigned int flags =
    SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_STRICTENC |
    SCRIPT_ENABLE_SIGHASH_FORKID | SCRIPT_VERIFY_LOW_S | SCRIPT_VERIFY_NULLFAIL | SCRIPT_VERIFY_DERSIG |
    SCRIPT_VERIFY_MINIMALDATA | SCRIPT_VERIFY_NULLDUMMY |
    SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS | SCRIPT_VERIFY_CLEANSTACK |
    SCRIPT_VERIFY_NULLFAIL | SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY |
    SCRIPT_VERIFY_CHECKSEQUENCEVERIFY | SCRIPT_VERIFY_LOW_S |
    SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM;

BOOST_AUTO_TEST_SUITE(machine_test)

BOOST_AUTO_TEST_CASE(base) {
    Machine machine;
    MachineEnv env(flags);
    machine.SetEnv(&env);
    machine.SetProgram({
        OP_5, OP_4, OP_ADD, OP_3, OP_EQUALVERIFY
    }, true);
    // PUSH5
    BOOST_CHECK(machine.Step() == SCRIPT_ERR_OK);
    BOOST_CHECK(machine.StackSize() == 1);
    // PUSH4
    BOOST_CHECK(machine.Step() == SCRIPT_ERR_OK);
    BOOST_CHECK(machine.StackSize() == 2);
    // ADD
    BOOST_CHECK(machine.Step() == SCRIPT_ERR_OK);
    BOOST_CHECK(machine.StackSize() == 1);
    // PUSH3
    BOOST_CHECK(machine.Step() == SCRIPT_ERR_OK);
    BOOST_CHECK(machine.StackSize() == 2);
    // EQUALVERIFY
    BOOST_CHECK(machine.Step() == SCRIPT_ERR_EQUALVERIFY);
    BOOST_CHECK(machine.StackSize() == 0);

    machine.SetProgram({
        // if (5+4==9) push5 else push1 endif verify(5)
        OP_5, OP_4, OP_ADD, OP_9, OP_EQUAL,
        OP_IF, OP_5, OP_ELSE, OP_1, OP_ENDIF,
        OP_5, OP_EQUALVERIFY
    }, true);
    BOOST_CHECK(machine.Continue() == SCRIPT_ERR_OK);
    BOOST_CHECK(machine.StackSize() == 0);
}

BOOST_AUTO_TEST_SUITE_END()
