#pragma once

#include <cstddef>
#include <cstdint>
#include <variant>
#include <biscuit/isa.hpp>
#include <biscuit/registers.hpp>
#include <biscuit/vector.hpp>

namespace biscuit {

enum class DecoderStatus : int32_t {
    Ok = 0,
    OutOfLength = -1,
    InvalidBuffer = -2,
    UnknownInstruction = -3,
    UnknownInstructionCompressed = -4,
};

enum class OperandType : uint32_t { Null, GPR, FPR, Vec, Immediate };

namespace Attributes {
enum : uint64_t {
    IsBranch = 1ull << 0,
};
}

struct DecodedInstruction {
    uint64_t attributes;

    Extension extension;

    ArchFeature arch;

    Mnemonic mnemonic;

    VecMask masked = VecMask::No;

    Ordering ordering = Ordering::None;

    RMode rmode = RMode::DYN;

    uint8_t raw[4];

    uint8_t length = 0;

    uint8_t operand_count = 0;
};

struct DecodedOperand {
    DecodedOperand() = default;

    explicit constexpr DecodedOperand(biscuit::GPR gpr, uint32_t width, bool is_write) : value(gpr), width(width), is_write(is_write) {}

    explicit constexpr DecodedOperand(biscuit::FPR fpr, uint32_t width, bool is_write) : value(fpr), width(width), is_write(is_write) {}

    explicit constexpr DecodedOperand(biscuit::Vec vec, uint32_t width, bool is_write) : value(vec), width(width), is_write(is_write) {}

    explicit constexpr DecodedOperand(int32_t immediate) : value(immediate) {}

    biscuit::GPR GPR() const {
        BISCUIT_ASSERT(IsGPR());
        return std::get<biscuit::GPR>(value);
    }

    biscuit::FPR FPR() const {
        BISCUIT_ASSERT(IsFPR());
        return std::get<biscuit::FPR>(value);
    }

    biscuit::Vec Vec() const {
        BISCUIT_ASSERT(IsVec());
        return std::get<biscuit::Vec>(value);
    }

    int32_t Immediate() const {
        BISCUIT_ASSERT(IsImmediate());
        return std::get<int32_t>(value);
    }

    bool IsGPR() const {
        return GetType() == OperandType::GPR;
    }

    bool IsFPR() const {
        return GetType() == OperandType::FPR;
    }

    bool IsVec() const {
        return GetType() == OperandType::Vec;
    }

    bool IsImmediate() const {
        return GetType() == OperandType::Immediate;
    }

    OperandType GetType() const {
        if (std::holds_alternative<biscuit::GPR>(value)) {
            return OperandType::GPR;
        } else if (std::holds_alternative<biscuit::FPR>(value)) {
            return OperandType::FPR;
        } else if (std::holds_alternative<biscuit::Vec>(value)) {
            return OperandType::Vec;
        } else if (std::holds_alternative<int32_t>(value)) {
            return OperandType::Immediate;
        } else {
            return OperandType::Null;
        }
    }

    bool IsWrite() const {
        return is_write;
    }

    uint32_t Width() const {
        return width;
    }

private:
    std::variant<std::monostate, biscuit::GPR, biscuit::FPR, biscuit::Vec, int32_t> value;
    uint32_t width = 0;
    bool is_write = false;
};

class Decoder {
public:
    [[nodiscard]] explicit Decoder(ArchFeature features = ArchFeature::RV64) : m_features(features) {}

    // Copy constructor and assignment.
    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;

    // Move constructor and assignment.
    Decoder(Decoder&&) = default;
    Decoder& operator=(Decoder&&) = default;

    // Destructor
    virtual ~Decoder();

    void SetArchFeatures(ArchFeature features) noexcept {
        m_features = features;
    }

    [[nodiscard]] DecoderStatus Decode(const void* buffer, size_t length, DecodedInstruction& instruction, DecodedOperand* operands) const;

private:
    [[nodiscard]] DecoderStatus DecodeCompressed(uint16_t data, DecodedInstruction& instruction, DecodedOperand* operands) const;

    [[nodiscard]] DecoderStatus DecodeRegular(uint32_t data, DecodedInstruction& instruction, DecodedOperand* operands) const;

    ArchFeature m_features = ArchFeature::RV64;
};

} // namespace biscuit
