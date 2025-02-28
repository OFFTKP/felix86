// Copyright (c), 2022, KNS Group LLC (YADRO)
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#pragma once

#include <biscuit/assembler.hpp>
#include <biscuit/registers.hpp>
#include <biscuit/isa.hpp>
#include <cstddef>
#include <cstdint>

namespace biscuit {

// For backwards compatibility
using RISCVExtension = Extension;

template <CSR csr>
struct CSRReader : public biscuit::Assembler {
    // Buffer capacity exactly for 2 instructions.
    static constexpr size_t capacity = 8;

    CSRReader() : biscuit::Assembler{CSRReader::capacity} {
        CSRR(a0, csr);
        RET();
    }

    // Copy constructor and assignment.
    CSRReader(const CSRReader&) = delete;
    CSRReader& operator=(const CSRReader&) = delete;

    // Move constructor and assignment.
    CSRReader(CSRReader&&) = default;
    CSRReader& operator=(CSRReader&&) = default;

    template <typename CSRReaderFunc>
    CSRReaderFunc GetCode() {
        this->GetCodeBuffer().SetExecutable();
        return reinterpret_cast<CSRReaderFunc>(this->GetBufferPointer(0));
    }
};

/**
 * Class that detects information about a RISC-V CPU.
 */
class CPUInfo {
public:
    /**
     * Checks if a particular RISC-V extension is available.
     *
     * @param extension The extension to check.
     */
    bool Has(Extension extension) const;

    /// Returns the vector register length in bytes.
    uint32_t GetVlenb() const;
};

} // namespace biscuit
