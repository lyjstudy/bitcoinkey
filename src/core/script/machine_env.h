#pragma once

#include <cstdint>
#include <vector>
#include "script_def.h"

namespace script {

    class MachineEnv {
        protected:
            uint32_t mFlags;
        public:
            MachineEnv(uint32_t flags) : mFlags(flags) {}
            virtual ~MachineEnv() {}
            inline uint32_t GetFlags() { return mFlags; }


            virtual bool CheckSig(const std::vector<uint8_t> &scriptSig,
                                const std::vector<uint8_t> &vchPubKey,
                                const std::vector<uint8_t> &scriptCode, 
                                uint32_t flags) const = 0;
            virtual bool CheckLockTime(const int64_t nLockTime) const = 0;
            virtual bool CheckSequence(const int64_t nSequence) const = 0;
    };

}
