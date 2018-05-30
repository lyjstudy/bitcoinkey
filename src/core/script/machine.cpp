
#include <algorithm>
#include <cassert>
#include "machine.h"
#include "crypto/ripemd160.h"
#include "crypto/sha1.h"
#include "crypto/sha256.h"

namespace script {
    /** Encode/decode small integers: */
    static int DecodeOP_N(OpCodeType opcode) {
        if (opcode == OP_0) return 0;
        assert(opcode >= OP_1 && opcode <= OP_16);
        return (int)opcode - (int)(OP_1 - 1);
    }
    /*
    static OpCodeType EncodeOP_N(int n) {
        assert(n >= 0 && n <= 16);
        if (n == 0) return OP_0;
        return (OpCodeType)(OP_1 + n - 1);
    }
    */

    Machine::StackType::StackType() {
    }

    Machine::StackType::~StackType() {
    }

    Machine::StackData Machine::StackType::_PopStack(std::vector<StackData> &stack) {
        if (stack.empty()) {
            throw ExecuteException("Machine::StackType::_PopStack()",
                            SCRIPT_ERR_INVALID_STACK_OPERATION);
        }
        StackData result = std::move(stack.back());
        stack.pop_back();
        return std::move(result);
    }

    Machine::ConditionType::ConditionType()
        : mCachedResult(true) {
    }

    Machine::ConditionType::~ConditionType() {
    }

    void Machine::ConditionType::If(bool condition) {
        mConditions.push_back(condition);
        if (!condition) {
            mCachedResult = false;
        }
    }
    void Machine::ConditionType::Else() {
        if (mConditions.empty()) {
            throw ExecuteException("Machine::ConditionType::Else()", SCRIPT_ERR_UNBALANCED_CONDITIONAL);
        }
        mConditions.back() = !mConditions.back();
        mCachedResult = (std::find(mConditions.begin(), mConditions.end(), false) == mConditions.end());
    }
    void Machine::ConditionType::EndIf() {
        if (mConditions.empty()) {
            throw ExecuteException("Machine::ConditionType::EndIf()", SCRIPT_ERR_UNBALANCED_CONDITIONAL);
        }
        mConditions.pop_back();
        mCachedResult = (std::find(mConditions.begin(), mConditions.end(), false) == mConditions.end());
    }


    Machine::Machine()
        : mCounter(0), mError(SCRIPT_ERR_OK), mOpCounter(0), mEnv(nullptr) {
    }
    Machine::~Machine() {
    }
    void Machine::SetEnv(MachineEnv *env) {
        mEnv = env;
    }
    ScriptError Machine::SetProgram(const std::vector<uint8_t> &program, bool clearStack) {
        mProgram = program;
        mCounter = 0;
        mOpCounter = 0;
        if (clearStack) {
            mStack.Clear();
        } else {
            mStack.ClearLocal();
        }
        mCondition.Clear();
        if (mEnv == nullptr) {
            mError = SCRIPT_ERR_ENV_NOTSET;
        } else if (mProgram.size() > MAX_SCRIPT_SIZE) {
            mError = SCRIPT_ERR_SCRIPT_SIZE;
        } else {
            mError = SCRIPT_ERR_OK;
        }
        return mError;
    }
    ScriptError Machine::Continue() {
        if (mError != SCRIPT_ERR_OK) {
            return mError;
        }

        ScriptError err;
        do {
            err = Step();
        } while (err == SCRIPT_ERR_OK);

        if (err == SCRIPT_ERR_PROGRAM_ENDED) {
            return SCRIPT_ERR_OK;
        }
        return err;
    }

    void Machine::Reset() {
        mCounter = 0;
        mOpCounter = 0;
        mStack.Clear();
        mCondition.Clear();
        if (mEnv == nullptr) {
            mError = SCRIPT_ERR_ENV_NOTSET;
        } else if (mProgram.size() > MAX_SCRIPT_SIZE) {
            mError = SCRIPT_ERR_SCRIPT_SIZE;
        } else {
            mError = SCRIPT_ERR_OK;
        }
    }
    bool Machine::IsResetStatus() {
        return mCounter == 0 && mOpCounter == 0 &&
                mStack.Empty() && mCondition.Empty();
    }
    int Machine::GetSigOpCount(bool fAccurate) {
        if (!IsResetStatus()) return -1;
        
        OpCodeType opcode;
        OpCodeType lastOpcode = OP_INVALIDOPCODE;
        int count = 0;
        while (Fetch(opcode, nullptr) != SCRIPT_ERR_OK) {

            if (opcode == OP_CHECKSIG || opcode == OP_CHECKSIGVERIFY) {
                count++;
            } else if (opcode == OP_CHECKMULTISIG || opcode == OP_CHECKMULTISIGVERIFY) {
                if (fAccurate && lastOpcode >= OP_1 && lastOpcode <= OP_16)
                    count += DecodeOP_N(lastOpcode);
                else
                    count += MAX_PUBKEYS_PER_MULTISIG;
            }
            lastOpcode = opcode;
        }
        return count;
    }

    ScriptError Machine::Step() {
        OpCodeType opcode;
        std::vector<uint8_t> data;
        if (Fetch(opcode, &data) != SCRIPT_ERR_OK) {
            return mError;
        }
        if (data.size() > MAX_SCRIPT_ELEMENT_SIZE) {
            return _SetError(SCRIPT_ERR_PUSH_SIZE);
        }
        if (opcode > OP_16 && ++mOpCounter > MAX_OPS_PER_SCRIPT) {
            return _SetError(SCRIPT_ERR_OP_COUNT);
        }

        if (mEnv->IsOpcodeDisabled(opcode)) {
            return _SetError(SCRIPT_ERR_DISABLED_OPCODE);
        }

        try {
            if (opcode >= OP_IF && opcode <= OP_ENDIF) {
                OpCondition(opcode);
            } else if (mCondition.Result()) {
                if (opcode <= OP_PUSHDATA4) {
                    OpPush(opcode, data);
                } else if (opcode <= OP_16) {
                    OpPush(opcode);
                } else {
                    OpNonCondition(opcode);
                }
            }
            if (mError == SCRIPT_ERR_OK && mStack.Size() > 1000) {
                mError = SCRIPT_ERR_STACK_SIZE;
            }
        } catch (ExecuteException &e) {
            mError = e.ErrCode();
        } catch (ScriptNumException &e) {
            mError = SCRIPT_ERR_INVALID_NUMBER_RANGE;
        } catch (...) {
            mError = SCRIPT_ERR_UNKNOWN_ERROR;
        }

        return mError;
    }


    ScriptError Machine::Fetch(OpCodeType &opcode, std::vector<uint8_t> *data) {
        if (mError != SCRIPT_ERR_OK) {
            return mError;
        }

        opcode = OP_INVALIDOPCODE;
        if (data) data->clear();

        if (mCounter >= mProgram.size()) {
            if (mCondition.Empty()) {
                return _SetError(SCRIPT_ERR_PROGRAM_ENDED);
            } else {
                return _SetError(SCRIPT_ERR_UNBALANCED_CONDITIONAL);
            }
        }

        uint8_t op = mProgram[mCounter++];

        if (op <= OP_PUSHDATA4) {
            unsigned int nSize = 0;
            if (op < OP_PUSHDATA1) {
                nSize = op;
            } else if (op == OP_PUSHDATA1) {
                if (mCounter + 1 >= mProgram.size()) {
                    return _SetError(SCRIPT_ERR_BAD_OPCODE);
                }
                nSize = mProgram[mCounter++];
            } else if (opcode == OP_PUSHDATA2) {
                if (mCounter + 2 >= mProgram.size()) {
                    return _SetError(SCRIPT_ERR_BAD_OPCODE);
                }
                nSize = (mProgram[mCounter++] << 8);
                nSize |= mProgram[mCounter++];
            } else if (opcode == OP_PUSHDATA4) {
                if (mCounter + 4 >= mProgram.size()) {
                    return _SetError(SCRIPT_ERR_BAD_OPCODE);
                }
                nSize = (mProgram[mCounter++] << 24);
                nSize |= (mProgram[mCounter++] << 16);
                nSize |= (mProgram[mCounter++] << 8);
                nSize |= mProgram[mCounter++];
            }
            if (mCounter + nSize >= mProgram.size()) {
                return _SetError(SCRIPT_ERR_BAD_OPCODE);
            }
            if (data) data->assign(&mProgram[mCounter], &mProgram[mCounter + nSize]);
            mCounter += nSize;
        }

        opcode = (OpCodeType)op;
        return SCRIPT_ERR_OK;
    }

    ScriptError Machine::OpPush(OpCodeType opcode, const std::vector<uint8_t> &data) {
        if (!mEnv->CheckMinimalPush(opcode, data)) {
            return _SetError(SCRIPT_ERR_MINIMALDATA);
        }
        mStack.Push(data);
        return SCRIPT_ERR_OK;
    }
    ScriptError Machine::OpPush(OpCodeType opcode) {
        if (opcode == OP_RESERVED) return _SetError(SCRIPT_ERR_BAD_OPCODE);
        ScriptNum bn((int)opcode - (int)(OP_1 - 1));
        mStack.Push(bn.GetVector());
        return SCRIPT_ERR_OK;
    }
    ScriptError Machine::OpNonCondition(OpCodeType opcode) {

        static const std::vector<uint8_t> vchFalse(0);
        static const std::vector<uint8_t> vchTrue(1, 1);
        static const ScriptNum bnZero(0);
        static const ScriptNum bnOne(1);

        switch (opcode) {
            case OP_NOP:
                break;
            case OP_CHECKLOCKTIMEVERIFY:
                mEnv->CheckLockTime(mStack.Top());
                break;
            case OP_CHECKSEQUENCEVERIFY:
                mEnv->CheckSequence(mStack.Top());
                break;
            case OP_NOP1:
            case OP_NOP4:
            case OP_NOP5:
            case OP_NOP6:
            case OP_NOP7:
            case OP_NOP8:
            case OP_NOP9:
            case OP_NOP10:
                if (mEnv->DisUpgradableNops()) {
                    return _SetError(SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS);
                }
                break;
            case OP_VERIFY:
                if (!_CastToBool(mStack.Pop())) {
                    return _SetError(SCRIPT_ERR_VERIFY);
                }
                break;
            case OP_RETURN:
                return _SetError(SCRIPT_ERR_OP_RETURN);
            case OP_TOALTSTACK:
                mStack.ToLocal();
                break;
            case OP_FROMALTSTACK:
                mStack.FromLocal();
                break;
            case OP_2DROP:
                mStack.Pop();
                mStack.Pop();
                break;
            case OP_2DUP:
                mStack.Push(mStack.Top(-2));
                mStack.Push(mStack.Top(-2));
                break;
            case OP_3DUP:
                mStack.Push(mStack.Top(-3));
                mStack.Push(mStack.Top(-3));
                mStack.Push(mStack.Top(-3));
                break;
            case OP_2OVER:
                mStack.Push(mStack.Top(-4));
                mStack.Push(mStack.Top(-4));
                break;
            case OP_2ROT:
                mStack.Push(mStack.Top(-6));
                mStack.Push(mStack.Top(-6));
                mStack.EraseEnd(-8, -6);
                break;
            case OP_2SWAP:
                mStack.SwapEnd(-4, -2);
                mStack.SwapEnd(-3, -1);
                break;
            case OP_IFDUP:
                if (_CastToBool(mStack.Top())) {
                    mStack.Push(mStack.Top());
                }
                break;
            case OP_DEPTH: {
                    ScriptNum bn(mStack.Size());
                    mStack.Push(bn.GetVector());
                }
                break;
            case OP_DROP:
                mStack.Pop();
                break;
            case OP_DUP:
                mStack.Push(mStack.Top());
                break;
            case OP_NIP:
                mStack.EraseEnd(-2, -1);
                break;
            case OP_OVER:
                mStack.Push(mStack.Top(-2));
                break;
            case OP_PICK:
            case OP_ROLL: {
                    int val = ScriptNum(mStack.Pop(), mEnv->GetMinimalPush()).GetValue();
                    auto vch = mStack.Top(-val - 1);
                    if (opcode == OP_ROLL) {
                        mStack.EraseEnd(-val - 1, -val);
                    }
                    mStack.Push(vch);
                }
                break;
            case OP_ROT:
                mStack.SwapEnd(-3, -2);
                mStack.SwapEnd(-2, -1);
                break;
            case OP_SWAP:
                mStack.SwapEnd(-2, -1);
                break;
            case OP_TUCK:
                mStack.Push(mStack.Top(-2));
                mStack.SwapEnd(-3, -1);
                break;
            case OP_SIZE:
                mStack.Push(ScriptNum(mStack.Top(-1).size()).GetVector());
                break;
            case OP_EQUAL:
            case OP_EQUALVERIFY: {
                    bool fEqual = (mStack.Pop() == mStack.Pop());
                    if (opcode == OP_EQUALVERIFY) {
                        if (!fEqual) {
                            return _SetError(SCRIPT_ERR_EQUALVERIFY);
                        }
                    } else {
                        mStack.Push(fEqual ? vchTrue : vchFalse);
                    }
                }
                break;
            case OP_AND:
                _StackLogicOp([] (uint8_t &a, uint8_t &b) {
                    a &= b;
                });
                break;
            case OP_OR:
                _StackLogicOp([] (uint8_t &a, uint8_t &b) {
                    a |= b;
                });
                break;
            case OP_XOR:
                _StackLogicOp([] (uint8_t &a, uint8_t &b) {
                    a ^= b;
                });
                break;
            case OP_1ADD:
                _StackTopOp([] (ScriptNum &bn) {
                    bn += bnOne;
                });
                break;
            case OP_1SUB:
                _StackTopOp([] (ScriptNum &bn) {
                    bn -= bnOne;
                });
                break;
            case OP_NEGATE:
                _StackTopOp([] (ScriptNum &bn) {
                    bn = -bn;
                });
                break;
            case OP_ABS:
                _StackTopOp([] (ScriptNum &bn) {
                    if (bn < bnZero) {
                        bn = -bn;
                    }
                });
                break;
            case OP_NOT:
                _StackTopOp([] (ScriptNum &bn) {
                    bn = (bn == bnZero);
                });
                break;
            case OP_0NOTEQUAL:
                _StackTopOp([] (ScriptNum &bn) {
                    bn = (bn != bnZero);
                });
                break;
            case OP_ADD:
                _StackNumericOp([] (ScriptNum &bn1, ScriptNum &bn2) {
                    return bn1 + bn2;
                });
                break;
            case OP_SUB:
                _StackNumericOp([] (ScriptNum &bn1, ScriptNum &bn2) {
                    return bn1 - bn2;
                });
                break;
            case OP_DIV:
                _StackNumericOp([] (ScriptNum &bn1, ScriptNum &bn2) {
                    if (bn2 == 0) throw ExecuteException("OP_DIV", SCRIPT_ERR_DIV_BY_ZERO);
                    return bn1 / bn2;
                });
                break;
            case OP_MOD:
                _StackNumericOp([] (ScriptNum &bn1, ScriptNum &bn2) {
                    if (bn2 == 0) throw ExecuteException("OP_MOD", SCRIPT_ERR_MOD_BY_ZERO);
                    return bn1 % bn2;
                });
                break;
            case OP_BOOLAND:
                _StackNumericOp([] (ScriptNum &bn1, ScriptNum &bn2) {
                    return ScriptNum(bn1 != bnZero && bn2 != bnZero);
                });
                break;
            case OP_BOOLOR:
                _StackNumericOp([] (ScriptNum &bn1, ScriptNum &bn2) {
                    return ScriptNum(bn1 != bnZero || bn2 != bnZero);
                });
                break;
            case OP_NUMEQUAL:
                _StackNumericOp([] (ScriptNum &bn1, ScriptNum &bn2) {
                    return ScriptNum(bn1 == bn2);
                });
                break;
            case OP_NUMEQUALVERIFY:
                _StackNumericOp([] (ScriptNum &bn1, ScriptNum &bn2) {
                    return ScriptNum(bn1 == bn2);
                });
                if (!_CastToBool(mStack.Pop())) {
                    throw ExecuteException("", SCRIPT_ERR_NUMEQUALVERIFY);
                }
                break;
            case OP_NUMNOTEQUAL:
                _StackNumericOp([] (ScriptNum &bn1, ScriptNum &bn2) {
                    return ScriptNum(bn1 != bn2);
                });
                break;
            case OP_LESSTHAN:
                _StackNumericOp([] (ScriptNum &bn1, ScriptNum &bn2) {
                    return ScriptNum(bn1 < bn2);
                });
                break;
            case OP_GREATERTHAN:
                _StackNumericOp([] (ScriptNum &bn1, ScriptNum &bn2) {
                    return ScriptNum(bn1 > bn2);
                });
                break;
            case OP_LESSTHANOREQUAL:
                _StackNumericOp([] (ScriptNum &bn1, ScriptNum &bn2) {
                    return ScriptNum(bn1 <= bn2);
                });
                break;
            case OP_GREATERTHANOREQUAL:
                _StackNumericOp([] (ScriptNum &bn1, ScriptNum &bn2) {
                    return ScriptNum(bn1 >= bn2);
                });
                break;
            case OP_MIN:
                _StackNumericOp([] (ScriptNum &bn1, ScriptNum &bn2) {
                    return (bn1 < bn2 ? bn1 : bn2);
                });
                break;
            case OP_MAX:
                _StackNumericOp([] (ScriptNum &bn1, ScriptNum &bn2) {
                    return (bn1 > bn2 ? bn1 : bn2);
                });
                break;
            case OP_WITHIN: {
                    ScriptNum bn3(mStack.Pop(), mEnv->GetMinimalPush());
                    ScriptNum bn2(mStack.Pop(), mEnv->GetMinimalPush());
                    ScriptNum bn1(mStack.Pop(), mEnv->GetMinimalPush());
                    bool fValue = (bn2 <= bn1 && bn1 < bn3);
                    mStack.Push(fValue ? vchTrue : vchFalse);
                }
                break;
            case OP_RIPEMD160:
                _CalcHash<CRIPEMD160>();
                break;
            case OP_SHA1:
                _CalcHash<CSHA1>();
                break;
            case OP_SHA256:
                _CalcHash<CSHA256>();
                break;
                // FIXME: hash.h to crypto
            case OP_HASH160:
                // _CalcHash<CHash160>();
                break;
            case OP_HASH256:
                // _CalcHash<CHash256>();
                break;
            case OP_CODESEPARATOR:
                break;
            case OP_CHECKSIG:
            case OP_CHECKSIGVERIFY:
                break;
            case OP_CHECKMULTISIG:
            case OP_CHECKMULTISIGVERIFY:
                break;
            case OP_CAT: {
                    auto vch2 = mStack.Pop();
                    auto &vch1 = mStack.Top();
                    if (vch1.size() + vch2.size() >
                        MAX_SCRIPT_ELEMENT_SIZE) {
                        return _SetError(SCRIPT_ERR_PUSH_SIZE);
                    }
                    vch1.insert(vch1.end(), vch2.begin(), vch2.end());
                }
                break;
            case OP_SPLIT: {
                    StackData &data = mStack.Top(-2);
                    StackData &vch = mStack.Top(-1);
                    uint64_t pos = (uint64_t)ScriptNum(vch, mEnv->GetMinimalPush()).GetValue();
                    if (pos > data.size()) {
                        return _SetError(SCRIPT_ERR_INVALID_SPLIT_RANGE);
                    }
                    StackData n1(data.begin(), data.begin() + pos);
                    StackData n2(data.begin() + pos, data.end());

                    data = std::move(n1);
                    vch = std::move(n2);
                }
                break;
            case OP_BIN2NUM:
                ScriptNum::MinimallyEncode(mStack.Top());
                if (!ScriptNum::IsMinimallyEncoded(mStack.Top())) {
                    return _SetError(SCRIPT_ERR_INVALID_NUMBER_RANGE);
                }
                break;
            case OP_NUM2BIN: {
                    uint64_t size = (uint64_t)ScriptNum(mStack.Pop(), mEnv->GetMinimalPush()).GetValue();
                    if (size > MAX_SCRIPT_ELEMENT_SIZE) {
                        return _SetError(SCRIPT_ERR_PUSH_SIZE);
                    }
                    StackData &rawnum = mStack.Top();
                    ScriptNum::MinimallyEncode(rawnum);
                    if (rawnum.size() > size) {
                        return _SetError(SCRIPT_ERR_IMPOSSIBLE_ENCODING);
                    }
                    if (rawnum.size() == size) {
                        break;
                    }
                    uint8_t signbit = 0x00;
                    if (rawnum.size() > 0) {
                        signbit = rawnum.back() & 0x80;
                        rawnum[rawnum.size() - 1] &= 0x7f;
                    }

                    rawnum.reserve(size);
                    while (rawnum.size() < size - 1) {
                        rawnum.push_back(0x00);
                    }

                    rawnum.push_back(signbit);
                }
                break;
            default:
                return _SetError(SCRIPT_ERR_BAD_OPCODE);
        }
        return SCRIPT_ERR_OK;
    }
    ScriptError Machine::OpCondition(OpCodeType opcode) {
        switch (opcode) {
            case OP_IF:
                if (mCondition.Result()) {
                    mCondition.If(_IfPopBool());
                } else {
                    mCondition.If(false);
                }
                break;
            case OP_NOTIF:
                if (mCondition.Result()) {
                    mCondition.NotIf(_IfPopBool());
                } else {
                    mCondition.NotIf(true);
                }
                break;
            case OP_ELSE:
                mCondition.Else();
                break;
            case OP_ENDIF:
                mCondition.EndIf();
                break;
            default:
                return _SetError(SCRIPT_ERR_BAD_OPCODE);
        }
        return SCRIPT_ERR_OK;
    }

    bool Machine::_IfPopBool() {
        auto data = mStack.Pop();
        if (!mEnv->CheckMinimalIf(data)) {
            throw ExecuteException("Machine::_IfPopBool() CheckMinimalIf", SCRIPT_ERR_MINIMALIF);
        }
        return _CastToBool(data);
    }
    bool Machine::_CastToBool(const StackData &vch) {
        for (size_t i = 0; i < vch.size(); i++) {
            if (vch[i] != 0) {
                // Can be negative zero
                if (i == vch.size() - 1 && vch[i] == 0x80) {
                    return false;
                }
                return true;
            }
        }
        return false;
    }
    void Machine::_StackLogicOp(std::function<void(uint8_t&, uint8_t&)> func) {
        StackData &d1 = mStack.Top(-2);
        StackData &d2 = mStack.Top(-1);
        if (d1.size() != d2.size()) {
            throw ExecuteException("Machine::_StackLogicOp()", SCRIPT_ERR_INVALID_OPERAND_SIZE);
        }
        for (size_t i = 0; i < d1.size(); i++) {
            func(d1[i], d2[i]);
        }
        mStack.Pop();
    }
    void Machine::_StackTopOp(std::function<void(ScriptNum&)> func) {
        ScriptNum bn(mStack.Pop(), mEnv->GetMinimalPush());
        func(bn);
        mStack.Push(bn.GetVector());
    }
    void Machine::_StackNumericOp(std::function<ScriptNum(ScriptNum &, ScriptNum &)> func) {
        ScriptNum bn2(mStack.Pop(), mEnv->GetMinimalPush());
        ScriptNum bn1(mStack.Pop(), mEnv->GetMinimalPush());
        ScriptNum bn = func(bn1, bn2);
        mStack.Push(bn.GetVector());
    }
}
