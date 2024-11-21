#pragma once

#include <biscuit/assert.hpp>

#include <cstddef>
#include <optional>
#include <set>
#include <type_traits>

namespace biscuit {

/**
 * A Literal is a representation of a constant value that can be loaded into a register.
 * This is useful for avoiding multiple instructions for loading big constants.
 *
 * Literals, like Labels, don't need to be placed immediately. They can be created
 * and used with loads that require a Literal, and placed in the buffer at a later point.
 *
 * @note Any literal that is created, is used with a load instruction,
 *       but is *not* placed to a location (via Place() in the assembler)
 *       will result in an assertion being invoked when the literal instance's
 *       destructor is executed.
 *
 * @note A literal may only be placed to one location. Any attempt to place
 *       a literal that is already placed will result in an assertion being
 *       invoked.
 *
 * @par
 * An example of placing a literal:
 * @code{.cpp}
 * Assembler as{...};
 * Literal literal(0x1234567890ABCDEF);
 *
 * as.LD(x2, &literal);     // Load the literal (emits a AUIPC+LD sequence)
 * as.JR(x2);               // Execution continues elsewhere
 * as.Place(&literal);      // Place the literal at this location in the buffer
 * @endcode
*/
template<class T>
class Literal {
public:
    using Location = std::optional<ptrdiff_t>;
    using LocationOffset = Location::value_type;

    /**
     * This constructor results in a literal being constructed that is not
     * placed at a particular location yet.
     *
     * @param value The value that this literal represents.
     */
    explicit Literal(T value) : m_value{value} {}

    /// Destructor
    ~Literal() noexcept {
        // It's a logic bug if something references a literal and hasn't been handled.
        //
        // This is usually indicative of a scenario where a literal is referenced but
        // hasn't been placed at a location.
        //
        BISCUIT_ASSERT(IsResolved());
    }

    // Copying disabled for the same reasons as Labels.
    Literal(const Literal&) = delete;
    Literal& operator=(const Literal&) = delete;

    Literal(Literal&&) noexcept = default;
    Literal& operator=(Literal&&) noexcept = default;

    /**
     * Determines whether or not this literal instance has a location assigned to it.
     *
     * A literal is considered placed if it has an assigned location.
     */
    [[nodiscard]] bool IsPlaced() const noexcept {
        return m_location.has_value();
    }

    /**
     * Determines whether or not this literal is resolved.
     *
     * A literal is considered resolved when all referencing offsets have been handled.
     */
    [[nodiscard]] bool IsResolved() const noexcept {
        return m_offsets.empty();
    }

    /**
     * Determines whether or not this literal is unresolved.
     *
     * A literal is considered unresolved if it still has any unhandled referencing offsets.
     */
    [[nodiscard]] bool IsUnresolved() const noexcept {
        return !IsResolved();
    }

    /**
     * Retrieves the location for this literal.
     *
     * @note If the returned location is empty, then this literal has not been assigned
     *       a location yet.
     */
    [[nodiscard]] Location GetLocation() const noexcept {
        return m_location;
    }

private:
    // A literal instance is inherently bound to the assembler it's
    // used with, as the offsets within the literal set depend on
    // said assemblers code buffer.
    friend class Assembler;

    /**
     * Places a literal to the given location.
     *
     * @param offset The offset to place this literal at.
     *
     * @returns The literal value so it can be copied to memory by the assembler.
     *
     * @pre The literal must not have already been placed at a previous location.
     *      Attempting to place a literal multiple times is typically, in almost all scenarios,
     *      the source of bugs.
     *      Attempting to place an already placed literal will result in an assertion
     *      being triggered.
     */
    [[nodiscard]] const T& Place(LocationOffset offset) noexcept {
        BISCUIT_ASSERT(!IsPlaced());
        m_location = offset;
        return m_value;
    }

    /**
     * Marks the given address as dependent on this literal.
     *
     * This is used in scenarios where a literal exists, but has not yet been
     * placed at a location yet. It's important to track these addresses,
     * as we'll need to patch the dependent load instructions with the
     * proper offset once the literal is finally placed by the assembler.
     *
     * During literal placement, the offset will be calculated and inserted
     * into dependent instructions.
     */
    void AddOffset(LocationOffset offset) {
        // If a literal is already placed at a location, then offset tracking
        // isn't necessary. Tripping this assert means we have a bug somewhere.
        BISCUIT_ASSERT(!IsPlaced());
        BISCUIT_ASSERT(IsNewOffset(offset));

        m_offsets.insert(offset);
    }

    // Clears all the underlying offsets for this literal.
    void ClearOffsets() noexcept {
        m_offsets.clear();
    }

    // Determines whether or not this address has already been added before.
    [[nodiscard]] bool IsNewOffset(LocationOffset offset) const noexcept {
        return m_offsets.find(offset) == m_offsets.cend();
    }

    std::set<LocationOffset> m_offsets;
    Location m_location;
    const T m_value;

    // Literals are provided as a way to avoid long instruction sequences for loading
    // immediates to registers. As such, the Literal type is not useful for
    // types <= uint32_t, as those can be loaded directly with at most a couple instructions.
    // (e.g. via the LI() function in the assembler)
    static_assert(sizeof(T) >= 8, "Literal type must be at least 64 bits wide.");

    // For the assembler to be able to emit the literal value, it must be trivially copyable.
    static_assert(std::is_trivially_copyable_v<T>, "Literal type must be trivially copyable.");
};

} // namespace biscuit
