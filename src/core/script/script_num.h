#pragma once

#include <cstdint>
#include <vector>
#include <stdexcept>
#include <limits>

namespace script {

    class ScriptNumException : public std::runtime_error {
        public:
            explicit ScriptNumException(const std::string &str)
                : std::runtime_error(str) {}
    };

    /**
     * Numeric opcodes (OP_1ADD, etc) are restricted to operating on 4-byte
     * integers. The semantics are subtle, though: operands must be in the range
     * [-2^31 +1...2^31 -1], but results may overflow (and are valid as long as
     * they are not used in a subsequent numeric operation). CScriptNum enforces
     * those semantics by storing results as an int64 and allowing out-of-range
     * values to be returned as a vector of bytes but throwing an exception if
     * arithmetic is done or the result is interpreted as an integer.
     */
    class ScriptNum {
        protected:
            int64_t mValue;

            static int64_t _SetVector(const std::vector<uint8_t> &vch) {
                if (vch.empty()) return 0;

                int64_t result = 0;
                for (size_t i = 0; i != vch.size(); ++i)
                    result |= static_cast<int64_t>(vch[i]) << 8 * i;

                // If the input vector's most significant byte is 0x80, remove it from
                // the result's msb and return a negative.
                if (vch.back() & 0x80)
                    return -((int64_t)(result & ~(0x80ULL << (8 * (vch.size() - 1)))));

                return result;
            }

            static std::vector<uint8_t> _GetVector(const int64_t &value) {
                if (value == 0) return std::vector<uint8_t>();

                std::vector<uint8_t> result;
                const bool neg = value < 0;
                uint64_t absvalue = neg ? -value : value;

                while (absvalue) {
                    result.push_back(absvalue & 0xff);
                    absvalue >>= 8;
                }

                // - If the most significant byte is >= 0x80 and the value is positive,
                // push a new zero-byte to make the significant byte < 0x80 again.
                // - If the most significant byte is >= 0x80 and the value is negative,
                // push a new 0x80 byte that will be popped off when converting to an
                // integral.
                // - If the most significant byte is < 0x80 and the value is negative,
                // add 0x80 to it, since it will be subtracted and interpreted as a
                // negative when converting to an integral.
                if (result.back() & 0x80) {
                    result.push_back(neg ? 0x80 : 0);
                } else if (neg) {
                    result.back() |= 0x80;
                }

                return std::move(result);
            }

        public:
            static bool IsMinimallyEncoded(const std::vector<uint8_t> &vch,
                                    const size_t nMaxNumSize) {
                if (vch.size() > nMaxNumSize) {
                    return false;
                }

                if (vch.size() > 0) {
                    // Check that the number is encoded with the minimum possible number
                    // of bytes.
                    //
                    // If the most-significant-byte - excluding the sign bit - is zero
                    // then we're not minimal. Note how this test also rejects the
                    // negative-zero encoding, 0x80.
                    if ((vch.back() & 0x7f) == 0) {
                        // One exception: if there's more than one byte and the most
                        // significant bit of the second-most-significant-byte is set it
                        // would conflict with the sign bit. An example of this case is
                        // +-255, which encode to 0xff00 and 0xff80 respectively.
                        // (big-endian).
                        if (vch.size() <= 1 || (vch[vch.size() - 2] & 0x80) == 0) {
                            return false;
                        }
                    }
                }

                return true;
            }
            static bool MinimallyEncode(std::vector<uint8_t> &data) {
                if (data.size() == 0) {
                    return false;
                }

                // If the last byte is not 0x00 or 0x80, we are minimally encoded.
                uint8_t last = data.back();
                if (last & 0x7f) {
                    return false;
                }

                // If the script is one byte long, then we have a zero, which encodes as an
                // empty array.
                if (data.size() == 1) {
                    data = {};
                    return true;
                }

                // If the next byte has it sign bit set, then we are minimaly encoded.
                if (data[data.size() - 2] & 0x80) {
                    return false;
                }

                // We are not minimally encoded, we need to figure out how much to trim.
                for (size_t i = data.size() - 1; i > 0; i--) {
                    // We found a non zero byte, time to encode.
                    if (data[i - 1] != 0) {
                        if (data[i - 1] & 0x80) {
                            // We found a byte with it sign bit set so we need one more
                            // byte.
                            data[i++] = last;
                        } else {
                            // the sign bit is clear, we can use it.
                            data[i - 1] |= last;
                        }

                        data.resize(i);
                        return true;
                    }
                }

                // If we the whole thing is zeros, then we have a zero.
                data = {};
                return true;
            }
        public:
            static const size_t MAXIMUM_ELEMENT_SIZE = 4;

            explicit ScriptNum(const int64_t &n) { mValue = n; }
            explicit ScriptNum(const std::vector<uint8_t> &vch, bool fRequireMinimal,
                                const size_t nMaxNumSize = MAXIMUM_ELEMENT_SIZE) {
                if (vch.size() > nMaxNumSize) {
                    throw ScriptNumException("script number overflow");
                }
                if (fRequireMinimal && !IsMinimallyEncoded(vch, nMaxNumSize)) {
                    throw ScriptNumException("non-minimally encoded script number");
                }
                mValue = _SetVector(vch);
            }

            // logic int64
            inline bool operator==(const int64_t &rhs) const { return mValue == rhs; }
            inline bool operator!=(const int64_t &rhs) const { return mValue != rhs; }
            inline bool operator<=(const int64_t &rhs) const { return mValue <= rhs; }
            inline bool operator<(const int64_t &rhs) const { return mValue < rhs; }
            inline bool operator>=(const int64_t &rhs) const { return mValue >= rhs; }
            inline bool operator>(const int64_t &rhs) const { return mValue > rhs; }

            // logic ScriptNum
            inline bool operator==(const ScriptNum &rhs) const { return operator==(rhs.mValue); }
            inline bool operator!=(const ScriptNum &rhs) const { return operator!=(rhs.mValue); }
            inline bool operator<=(const ScriptNum &rhs) const { return operator<=(rhs.mValue); }
            inline bool operator<(const ScriptNum &rhs) const { return operator<(rhs.mValue); }
            inline bool operator>=(const ScriptNum &rhs) const { return operator>=(rhs.mValue); }
            inline bool operator>(const ScriptNum &rhs) const { return operator>(rhs.mValue); }

            // int64
            inline ScriptNum operator+(const int64_t &rhs) const { return ScriptNum(mValue + rhs); }
            inline ScriptNum operator-(const int64_t &rhs) const { return ScriptNum(mValue - rhs); }
            inline ScriptNum operator/(const int64_t &rhs) const { return ScriptNum(mValue / rhs); }
            inline ScriptNum operator%(const int64_t &rhs) const { return ScriptNum(mValue % rhs); }
            inline ScriptNum operator&(const int64_t &rhs) const { return ScriptNum(mValue & rhs); }
            inline ScriptNum &operator=(const int64_t &rhs) { mValue = rhs; return *this; }
            inline ScriptNum &operator+=(const int64_t &rhs) { mValue += rhs; return *this; }
            inline ScriptNum &operator-=(const int64_t &rhs) { mValue -= rhs; return *this; }
            inline ScriptNum &operator&=(const int64_t &rhs) { mValue &= rhs; return *this; }

            inline ScriptNum operator-() const { return ScriptNum(-mValue); }
            inline ScriptNum operator+(const ScriptNum &rhs) const { return operator+(rhs.mValue); }
            inline ScriptNum operator-(const ScriptNum &rhs) const { return operator-(rhs.mValue); }
            inline ScriptNum operator/(const ScriptNum &rhs) const { return operator/(rhs.mValue); }
            inline ScriptNum operator%(const ScriptNum &rhs) const { return operator%(rhs.mValue); }
            inline ScriptNum &operator+=(const ScriptNum &rhs) { return operator+=(rhs.mValue); }
            inline ScriptNum &operator-=(const ScriptNum &rhs) { return operator-=(rhs.mValue); }
            inline ScriptNum operator&(const ScriptNum &rhs) const { return operator&(rhs.mValue); }
            inline ScriptNum &operator&=(const ScriptNum &rhs) { return operator&=(rhs.mValue); }

            int GetValue() const {
                if (mValue > std::numeric_limits<int>::max())
                    return std::numeric_limits<int>::max();
                else if (mValue < std::numeric_limits<int>::min())
                    return std::numeric_limits<int>::min();
                return mValue;
            }
            std::vector<uint8_t> GetVector() const { return _GetVector(mValue); }
    };
}
