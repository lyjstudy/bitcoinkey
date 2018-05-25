
#include <algorithm>
#include "machine.h"

namespace script {

    Machine::StackType::StackType() {
    }

    Machine::StackType::~StackType() {
    }

    Machine::StackData Machine::StackType::PopStack(std::vector<StackData> &stack) {
        if (stack.empty()) {
            throw ExecuteException("Machine::StackType::PopStack()",
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
        : mCounter(0), mError(SCRIPT_ERR_OK), mEnv(nullptr) {
    }
    Machine::~Machine() {
    }
    void Machine::SetEnv(MachineEnv *env) {
        mEnv = env;
    }
    ScriptError Machine::SetProgram(const std::vector<uint8_t> &program, bool clearStack) {
        mProgram = program;
        mCounter = 0;
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
        if (mError != SCRIPT_ERR_OK) {
            return mError;
        }
        // get opcode
        // get opcode data
    }
}
