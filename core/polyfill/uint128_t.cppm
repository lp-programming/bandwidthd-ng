module;
export module uint128_t;


#ifdef __SIZEOF_INT128__
export using uint128_t = __uint128_t;

#else
import <format>;
import <string>;
import <ostream>;
import <iostream>;
import <iomanip>;
import <sstream>;
import <algorithm>;
import <cstdint>;
import <limits>;

constexpr MASK32 = std::numeric_limits<uint32_t>::max();

/*
  128bit polyfill for 32bit platforms. We rely on the compiler native 64bit longs. This requires 4 ints, so we hint to
  the compiler to pass by const ref wherever possible. 

  If working with 32bit numbers, we tell the compiler to pass by const val.

  At -O3, the compiler will do what it thinks is best, anyway.
 */
export
struct uint128_t {
  uint64_t hi;
  uint64_t lo;

public:
  constexpr uint128_t() : hi(0), lo(0) noexcept {}
  constexpr uint128_t(const uint64_t& low) : hi(0), lo(low) noexcept {}
  constexpr uint128_t(const uint64_t& high, const uint64_t& low) : hi(high), lo(low) noexcept {}

  [[nodiscard]] inline
  constexpr uint128_t operator&(const uint128_t& other) const noexcept {
    return uint128_t(hi & other.hi, lo & other.lo);
  }

  [[nodiscard]] inline
  constexpr uint128_t operator&(const uint64_t& rhs) const noexcept {
    return uint128_t(0, lo & rhs);
  }

   inline uint128_t& operator&=(const uint128_t& other) noexcept {
    hi &= other.hi;
    lo &= other.lo;
    return *this;
  }

   inline uint128_t& operator&=(const uint64_t& rhs) noexcept {
    lo &= rhs;
    return *this;
  }

  [[nodiscard]] inline
  constexpr uint128_t operator+(const uint128_t& other) const noexcept {
    const uint64_t new_lo = lo + other.lo;
    return uint128_t{
      hi + other.hi + static_cast<uint64_t>(new_lo < lo),
      new_lo};
  }

  [[nodiscard]] inline
  constexpr uint128_t operator-(const uint128_t other) const noexcept {
    const uint64_t new_lo = lo - other.lo;
    return uint128_t{
      hi - other.hi - static_cast<uint64_t>(new_lo > lo),
      new_lo};
  }

  [[nodiscard]] inline
  constexpr auto operator<=>(const uint64_t& other) const noexcept{
    if (hi != 0) {
      return std::strong_ordering::greater;
    }
    return lo <=> other.lo;
  }

  [[nodiscard]] inline
  constexpr auto operator<=>(const uint128_t& other) const noexcept{
    if (hi != other.hi) {
      return hi <=> other.hi;
    }
    return lo <=> other.lo;
  }

  [[nodiscard]] inline
  constexpr uint128_t operator<<(const int32_t shift) const noexcept {
    if (shift < 0) {
      return (*this) >> (-shift);
    }
    if (shift == 0) {
      return *this;
    }
    if (shift >= 128) {
      return uint128_t(0);
    }

    if (shift >= 64) {
      return uint128_t(lo << (shift - 64), 0);
    }
    else {
      const uint64_t new_hi = (hi << shift) | (lo >> (64 - shift));
      const uint64_t new_lo = lo << shift;
      return uint128_t(new_hi, new_lo);
    }
  }

  [[nodiscard]] inline
  constexpr uint128_t operator>>(const int32_t shift) const noexcept {
    if (shift < 0) {
      return (*this) << (-shift);
    }
    if (shift == 0) {
      return *this;
    }
    if (shift >= 128) {
      return uint128_t(0);
    }
    
    if (shift >= 64) {
      return uint128_t(0, hi >> (shift - 64));
    }
    else {
      const uint64_t new_lo = (lo >> shift) | (hi << (64 - shift));
      const uint64_t new_hi = hi >> shift;
      return uint128_t(new_hi, new_lo);
    }
  }

  [[nodiscard]] inline
  constexpr uint128_t operator*(const uint128_t& other) const noexcept {
    /* This is classic "grade school" multiplication of two "2-digit" numbers (just 2 digits in base 2⁶⁴)
           IJ    (This notation is a litle sloppy, in the input IJ is the number IJ, in the output JY is  
       *   XY    the digit J * the digit Y). 
       ------    Note the output is limited to the same two digits as the input, so the XI term can be 
           JY    discarded, as can the upper bits of IY and XJ. We only drop the XI term and let the compiler
       +  IY0    handle dropping the upper bits of IY and XJ if it can.
       +  XJ0    Last, note that JY can overflow from the low bits of the output to the high bits, and we need
       + XI00    to capture this overflow. So we'll do *it* piecewise using 32bit parts in 64bit variables.


       This follows the same method as the overall multiplication, but now we're computing two 2-digit numbers
       in base³². Also we can't discard any of it, as anything that overflows lands in the high bits of our final
       result.
     */
    const uint64_t I = hi; // bits 65-128 of this
    const uint64_t J = lo; // bits 1-64 of this
    const uint64_t X = other.hi; // bits 65-128 of other: the X term
    const uint64_t Y = other.lo; // bits 1-64 of other: the Y term

    // Now we compute JY. This is done with 4 32bit digits, expanded into 64 bit variables
    const uint64_t J_low  = (J & MASK32);
    const uint64_t J_high = (J >> 32);

    const uint64_t Y_low  = (Y & MASK32);
    const uint64_t Y_high = (Y >> 32);

    const uint64_t low_low = J_low * Y_low; // will be << 0
    const uint64_t low_high = J_low * Y_high; // will be << 32
    const uint64_t high_low = J_high * Y_low; // will be << 32
    const uint64_t high_high = J_high * Y_high; // will be << 64

    // The new low value is the low 64bits of JY.  We skip high_high since it *only* contains overflow.
    const uint64_t new_lo = low_low + (low_high << 32) + (high_low << 32);

    // Now we collect the carry bits.  These are implicitly << 64, as they'll be applied to new_hi
    const uint64_t carry = high_high + (low_high >> 32) + (high_low >> 32);

    // We skip this one, since we computed the carry part already
    // uint64_t JY = J * Y;

    const uint64_t IY = I * X; // implicitly << 64
    const uint64_t JX = J * X; // implicitly << 64
    
    // We skip this one, since it's << 128
    // uint64_t IX = I * X; // << 128

    const uint64_t new_hi = carry + IY + JX;
    return uint128_t(new_hi, new_lo);
  }

  [[nodiscard]] inline
  constexpr explicit operator uint64_t() const noexcept {
    return lo; // Truncates to lower 64 bits
  }

  [[nodiscard]] inline
  constexpr explicit operator uint32_t() const noexcept {
    return static_cast<uint32_t>(lo); // Truncates to lowest 32 bits
  }

  [[nodiscard]] inline
  constexpr uint128_t operator|(const uint128_t& other) const noexcept{
    return uint128_t(hi | other.hi, lo | other.lo);
  }

  [[nodiscard]] inline
  constexpr uint128_t operator|(uint64_t rhs) const noexcept {
    return uint128_t(hi, lo | rhs);
  }

  inline uint128_t& operator|=(const uint128_t& other) noexcept {
    hi |= other.hi;
    lo |= other.lo;
    return *this;
  }

  inline uint128_t& operator|=(uint64_t rhs) noexcept {
    lo |= rhs;
    return *this;
  }

  inline uint128_t& operator+=(const uint128_t& rhs) noexcept {
    const uint64_t old_lo = lo;
    lo += rhs.lo;
    hi += rhs.hi;
    if (lo < old_lo) {
      hi++;
    }
    return *this;
  }

  inline uint128_t& operator-=(const uint128_t& rhs) noexcept {
    const uint64_t old_lo = lo;
    lo -= rhs.lo;
    hi -= rhs.hi;
    if (lo > old_lo) {
      hi--;
    }
    return *this;
  }

  [[nodiscard]] inline
  constexpr uint128_t operator~() const noexcept {
    return uint128_t{ ~hi, ~lo };
  }

};

export std::ostream& operator<<(std::ostream& os, const uint128_t& value) {
    if (value.hi != 0) {
        os << std::hex << value.hi << std::setw(16) << std::setfill('0') << value.lo;
    } else {
        os << std::hex << value.lo;
    }
    return os;
}

export
template<>
struct std::formatter<uint128_t> {
    // Stores format style
    enum class fmt_style { default_, hex_lower, hex_upper } style = fmt_style::default_;

    // Parse format specifiers like ":x" or ":X"
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        auto end = ctx.end();

        if (it != end) {
            if (*it == 'x') {
                style = fmt_style::hex_lower;
                ++it;
            } else if (*it == 'X') {
                style = fmt_style::hex_upper;
                ++it;
            }
        }

        // Must consume the closing '}'
        if (it != end && *it == '}') {
            return it;
        }
        // Otherwise delegate to end (or throw error)
        return it;
    }

    // Format the uint128_t according to style
    template <typename FormatContext>
    auto format(const uint128_t& value, FormatContext& ctx) const {
        std::ostringstream oss;
        oss << std::setfill('0');
        if (style == fmt_style::hex_lower) {
            oss << std::hex << std::nouppercase;
        } else if (style == fmt_style::hex_upper) {
            oss << std::hex << std::uppercase;
        } else {
            // default formatting - decimal maybe
            oss << std::dec;
        }

        if (value.hi != 0) {
            oss << value.hi << std::setw(16) << value.lo;
        } else {
            oss << value.lo;
        }

        auto s = oss.str();
        return std::copy(s.begin(), s.end(), ctx.out());
    }
};
#endif
