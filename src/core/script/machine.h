#pragma once

#include <cstdint>
#include <vector>
#include <utility>
#include <stdexcept>
#include "script_error.h"
#include "script_def.h"
#include "machine_env.h"
#include "script_num.h"
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

    class Machine {
        public:
            using StackData = std::vector<uint8_t>;
            
            class StackType {
                protected:
                    std::vector<StackData> mStack;
                    std::vector<StackData> mLocalStack;

                    static StackData _PopStack(std::vector<StackData> &stack);
                public:
                    StackType();
                    ~StackType();
                    StackType(const StackType &) = delete;
                    StackType(StackType &&) = delete;
                    inline void Push(const StackData &data) { mStack.push_back(data); }
                    inline void Push(StackData &&data) { mStack.push_back(data); }
                    inline StackData Pop() { return _PopStack(mStack); }
                    inline size_t Size() { return mStack.size() + mLocalStack.size(); }
                    inline void Clear() { mLocalStack.clear(); mStack.clear(); }
                    // unique local stack per script
                    inline void ClearLocal() { mLocalStack.clear(); }
                    // OP_TOALTSTACK
                    inline void ToLocal() { mLocalStack.push_back(std::move(_PopStack(mStack))); }
                    // OP_FROMALTSTACK
                    inline void FromLocal() { mStack.push_back(std::move(_PopStack(mLocalStack))); }
            };

            class ConditionType {
                protected:
                    std::vector<bool> mConditions;
                    bool mCachedResult;
                public:
                    ConditionType();
                    ~ConditionType();
                    // OP_IF
                    void If(bool condition);
                    // OP_NOTIF
                    inline void NotIf(bool condition) { If(!condition); }
                    // OP_ELSE
                    void Else();
                    // OP_ENDIF
                    void EndIf();
                    inline bool Result() { return mCachedResult; }
                    inline bool Empty() { return mConditions.empty(); }
                    inline void Clear() { mConditions.clear(); mCachedResult = true; }
            };

        protected:
            std::vector<uint8_t> mProgram;
            size_t mCounter; // program counter
            StackType mStack; // program stack
            ConditionType mCondition; // IF NOTIF ELSE ENDIF
            ScriptError mError; // machine error
            size_t mOpCounter; // executed opcode counter
            MachineEnv *mEnv;

            inline ScriptError _SetError(ScriptError err) {
                mError = err;
                return mError;
            }
        public:
            Machine();
            ~Machine();

            void SetEnv(MachineEnv *env);
            ScriptError SetProgram(const std::vector<uint8_t> &program, bool clearStack);
            ScriptError Step();
            ScriptError Continue();

            ScriptError Fetch(OpCodeType &opcode, std::vector<uint8_t> *data);
        protected:
            ScriptError OpPush(OpCodeType opcode, const std::vector<uint8_t> &data);
            ScriptError OpPush(OpCodeType opcode);
            ScriptError OpNonCondition(OpCodeType opcode);
            ScriptError OpCondition(OpCodeType opcode);

            bool _IfPopBool();
            static bool _CastToBool(const StackData &data);
    };
}

