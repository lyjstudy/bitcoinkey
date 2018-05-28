#pragma once

#include <cstdint>
#include <vector>
#include <stdexcept>
#include "script_error.h"
#include "script_def.h"
#include "opcode.h"

namespace script {

    class ExecuteException : public std::runtime_error {
        protected:
            ScriptError mErrCode;
        public:
            explicit ExecuteException(const char *err, ScriptError code)
                : std::runtime_error(err), mErrCode(code) {
            }
            inline ScriptError ErrCode() { return mErrCode; }
    };

    class MachineEnv {
        protected:
            uint32_t mFlags;
        public:
            MachineEnv(uint32_t flags) : mFlags(flags) {}
            virtual ~MachineEnv() {}
            inline uint32_t GetFlags() { return mFlags; }


            inline bool IsOpcodeDisabled(OpCodeType opcode) {
                switch (opcode) {
                    case OP_INVERT:
                    case OP_2MUL:
                    case OP_2DIV:
                    case OP_MUL:
                    case OP_LSHIFT:
                    case OP_RSHIFT:
                        // Disabled opcodes.
                        return true;

                    case OP_CAT:
                    case OP_SPLIT:
                    case OP_AND:
                    case OP_OR:
                    case OP_XOR:
                    case OP_NUM2BIN:
                    case OP_BIN2NUM:
                    case OP_DIV:
                    case OP_MOD:
                        // Opcodes that have been reenabled.
                        if ((mFlags & SCRIPT_ENABLE_MONOLITH_OPCODES) == 0) {
                            return true;
                        }

                    default:
                        break;
                }

                return false;
            }
            inline bool GetMinimalPush() {
                return (mFlags & SCRIPT_VERIFY_MINIMALDATA) == SCRIPT_VERIFY_MINIMALDATA;
            }
            inline bool CheckMinimalPush(OpCodeType opcode, const std::vector<uint8_t> &data) {
                if ((mFlags & SCRIPT_VERIFY_MINIMALDATA) == 0) {
                    return true;
                }
                if (data.size() == 0) {
                    // Could have used OP_0.
                    return opcode == OP_0;
                }
                if (data.size() == 1 && data[0] >= 1 && data[0] <= 16) {
                    // Could have used OP_1 .. OP_16.
                    return opcode == OP_1 + (data[0] - 1);
                }
                if (data.size() == 1 && data[0] == 0x81) {
                    // Could have used OP_1NEGATE.
                    return opcode == OP_1NEGATE;
                }
                if (data.size() <= 75) {
                    // Could have used a direct push (opcode indicating number of bytes
                    // pushed + those bytes).
                    return opcode == data.size();
                }
                if (data.size() <= 255) {
                    // Could have used OP_PUSHDATA.
                    return opcode == OP_PUSHDATA1;
                }
                if (data.size() <= 65535) {
                    // Could have used OP_PUSHDATA2.
                    return opcode == OP_PUSHDATA2;
                }
                return true;
            }
            inline bool VerifyMinimalIf(const std::vector<uint8_t> &data) {
                if (mFlags & SCRIPT_VERIFY_MINIMALIF) {
                    if (data.size() > 1) return false;
                    if (data.size() == 1 && data[0] != 1) return false;
                }
                return true;
            }
            inline bool DisUpgradableNops() {
                return (mFlags & SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS) == SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS;
            }

            virtual void CheckLockTime(const std::vector<uint8_t> &vLockTime) const {};
            virtual void CheckSequence(const std::vector<uint8_t> &nSequence) const {};

            virtual void CheckSig(const std::vector<uint8_t> &scriptSig,
                                const std::vector<uint8_t> &vchPubKey,
                                const std::vector<uint8_t> &scriptCode, 
                                uint32_t flags) const {}
    };
}
