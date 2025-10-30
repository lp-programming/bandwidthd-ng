export module HeaderView;

import <string>;
import <string_view>;
import <bit>;
import <type_traits>;
import <stdexcept>;
import <cstring>;
import <vector>;

namespace {
  constexpr bool platform_allows_unaligned_access = 
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86) || defined(__amd64__) || defined(__aarch64__) || defined(__arm64__)
    true;
#else
    false;  // Unknown platform, assume strict alignment
#endif
  
} 

export
class HeaderView {
  const std::string_view buffer;
  size_t offset{0};
  
  std::vector<std::string> subheaders{};
  

public:
  constexpr explicit HeaderView(std::string_view buffer) noexcept: buffer(buffer) {}

  constexpr void reset() noexcept {
    offset = 0;
  }
  constexpr size_t tell() const noexcept {
    return offset;
  }

  constexpr size_t remaining() const noexcept {
    return buffer.size() - offset;
  }

  template <typename T>
  constexpr bool can_read() const noexcept {
    return remaining() >= sizeof(T);
  }

  template <typename T>
  constexpr const T& next_header() {
    if (!can_read<T>()) {
      throw std::out_of_range("HeaderView: insufficient buffer for header");
    }

    const char* ptr = buffer.data() + offset;
    offset += sizeof(T);

    if constexpr (platform_allows_unaligned_access) {
      return *reinterpret_cast<const T*>(ptr);
    }
    else {
      return reinterpret_cast<T&>(subheaders.emplace_back(ptr, ptr + sizeof(T)));
    }
  }

  // Peek at next header without advancing offset
  template <typename T>
  constexpr const T& peek_header() {
    if (!can_read<T>()) {
      throw std::out_of_range("HeaderView: insufficient buffer to peek header");
    }

    const char* ptr = buffer.data() + offset;

    if constexpr (platform_allows_unaligned_access) {
      return *reinterpret_cast<const T*>(ptr);
    }
    else {
      return reinterpret_cast<T&>(subheaders.emplace_back(ptr, ptr + sizeof(T)));
    }
  }

  constexpr void skip_forward(size_t b) {
    offset += b;
  }

  // Skip next header of size T
  template <typename T>
  constexpr void skip_header() {
    if (!can_read<T>()) {
      throw std::out_of_range("HeaderView: cannot skip, insufficient buffer");
    }
    offset += sizeof(T);
  }
};
