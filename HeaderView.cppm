export module HeaderView;

import <string>;
import <string_view>;
import <bit>;
import <type_traits>;
import <stdexcept>;
import <cstring>;

namespace {

  // Platform detection for alignment requirements
  // Known platforms with unaligned safe access:
  constexpr bool platform_allows_unaligned_access =
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86) || defined(__amd64__) || defined(__aarch64__) || defined(__arm64__)
    true;
#elif defined(__arm__) || defined(__aarch64__) || defined(__mips__) || defined(__sparc__)
    false;
#else
    false;  // Unknown platform, assume strict alignment
#endif

  // Helper alias: on unaligned-safe platforms return const T&,
  // else return a copied T
  template <typename T>
  using maybe_ref_t = std::conditional_t<
    platform_allows_unaligned_access,
    const T&,
    T>;

} 

export
class HeaderView {
private:
  std::string_view buffer_;
  size_t offset_ = 0;

public:
  constexpr explicit HeaderView(std::string_view buffer) noexcept
    : buffer_(buffer), offset_(0) {}

  // Reset position to start
  constexpr void reset() noexcept { offset_ = 0; }

  // Current offset getter
  constexpr size_t offset() const noexcept { return offset_; }

  // Size of remaining buffer
  constexpr size_t remaining() const noexcept { return buffer_.size() - offset_; }

  // Check if enough data remains for header T
  template <typename T>
  constexpr bool can_read() const noexcept {
    return remaining() >= sizeof(T);
  }

  // Consume and return next header of type T
  // Returns const T& if platform allows unaligned access,
  // else returns a copied T.
  template <typename T>
  constexpr const maybe_ref_t<T> next_header() {
    if (!can_read<T>()) {
      throw std::out_of_range("HeaderView: insufficient buffer for header");
    }

    const char* ptr = buffer_.data() + offset_;

    if constexpr (platform_allows_unaligned_access) {
      // Safe to reinterpret as ref
      const T& header_ref = *reinterpret_cast<const T*>(ptr);
      offset_ += sizeof(T);
      return header_ref;
    } else {
      // Strict alignment, must copy
      T header_copy{};
      std::memcpy(&header_copy, ptr, sizeof(T));
      offset_ += sizeof(T);
      return header_copy;
    }
  }

  // Peek at next header without advancing offset
  template <typename T>
  constexpr const maybe_ref_t<T> peek_header() const {
    if (!can_read<T>()) {
      throw std::out_of_range("HeaderView: insufficient buffer to peek header");
    }

    const char* ptr = buffer_.data() + offset_;

    if constexpr (platform_allows_unaligned_access) {
      const T& header_ref = *reinterpret_cast<const T*>(ptr);
      return header_ref;
    } else {
      T header_copy{};
      std::memcpy(&header_copy, ptr, sizeof(T));
      return header_copy;
    }
  }

  constexpr void skip_forward(size_t b) {
    offset_ += b;
  }

  // Skip next header of size T
  template <typename T>
  constexpr void skip_header() {
    if (!can_read<T>()) {
      throw std::out_of_range("HeaderView: cannot skip, insufficient buffer");
    }
    offset_ += sizeof(T);
  }
};
