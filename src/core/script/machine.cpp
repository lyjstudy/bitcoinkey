
#include <algorithm>
#include "machine.h"

namespace script {

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
        return SCRIPT_ERR_OK;
    }
    ScriptError Machine::OpCondition(OpCodeType opcode) {
        switch (opcode) {
            case OP_IF:
                mCondition.If(_IfPopBool());
                break;
            case OP_NOTIF:
                mCondition.NotIf(_IfPopBool());
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
        if (!mEnv->VerifyMinimalIf(data)) {
            throw ExecuteException("Machine::_IfPopBool() VerifyMinimalIf", SCRIPT_ERR_MINIMALIF);
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
}
