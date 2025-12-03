export module PCap;
import <pcap/pcap.h>;
import <string>;
import <typeinfo>;
import <tuple>;
import <type_traits>;
import <functional>;
import <source_location>;
import <format>;
import <errno.h>;
import <array>;
import <concepts>;

import static_for;
import SyscallHelper;

using lpprogramming::SyscallHelper::failure_message;

template<typename Sig>
inline constexpr bool is_last_arg_char_v = false;

template<typename R, typename... Args>
requires (sizeof...(Args) > 0)
inline constexpr bool is_last_arg_char_v<R(Args...)> = 
  std::is_same_v<std::decay_t<std::tuple_element_t<sizeof...(Args)-1, std::tuple<Args...>>>, char*>;

// Now the concept
template<typename F>
concept LastArgChar = is_last_arg_char_v<std::remove_pointer_t<decltype(&std::declval<F&>())>>;

template<typename Sig>
inline constexpr bool is_first_arg_pcap_v = false;

template<typename R, typename... Args>
requires (sizeof...(Args) > 0)
inline constexpr bool is_first_arg_pcap_v<R(Args...)> = 
  std::is_same_v<std::decay_t<std::tuple_element_t<0, std::tuple<Args...>>>, pcap_t*>;

template<typename F>
concept FirstArgPCap = is_first_arg_pcap_v<std::remove_pointer_t<decltype(&std::declval<F&>())>>;

export
namespace bandwidthd {
  class PCap {
    pcap_t* pc;
    std::string errbuf = std::string(PCAP_ERRBUF_SIZE, 0);

    [[noreturn]]
    inline void pcap_throw(const failure_message& m, std::string_view cause) {
      throw std::runtime_error
        (std::format("{} at {}:{}\ncaused by {}",
                     m.message, m.loc.file_name(), m.loc.line(), cause));
    }

  public:
    enum class FailureMethod {
      NULLPTR,
      NEGATIVE,
      NONZERO,
      ERRNO
    };
    using FailureMethod::NULLPTR;
    using FailureMethod::NONZERO;
    using FailureMethod::NEGATIVE;
    using FailureMethod::ERRNO;
    template<FailureMethod FM=NONZERO, typename Function, typename ...Args>
    inline auto PCapCall(Function& function, const failure_message& message, Args&&... args) noexcept(false) {
      // Begin allocating result
      using result_t = std::conditional_t<
        FirstArgPCap<Function>,
        std::conditional_t<
          LastArgChar<Function>,
          std::invoke_result<Function, pcap_t*, Args..., char*>,
          std::invoke_result<Function, pcap_t*, Args...>
          >,
        std::conditional_t<
          LastArgChar<Function>,
          std::invoke_result<Function, Args..., char*>,
          std::invoke_result<Function, Args...>
          >
        >::type;
      result_t result;
      // End allocating result
      // Begin call underlying
      if constexpr (FM == ERRNO) {
        errno = 0;
      }
      if constexpr (FirstArgPCap<Function>) {
        if (!pc) {
          pcap_throw(message, std::format("Cannot invoke {} before pcap_create", typeid(Function).name()));
        }
        if constexpr (LastArgChar<Function>) {
          result = function(pc, std::forward<Args>(args)..., errbuf.data());
          errbuf.back() = '\0';
        }
        else {
          result = function(pc, std::forward<Args>(args)...);
        }
      }
      else if constexpr (LastArgChar<Function>) {
        result = function(std::forward<Args>(args)..., errbuf.data());
        errbuf.back() = '\0';
      }
      else {
        result = function(std::forward<Args>(args)...);
      }
      // End call underlying
      // Begin check result
      bool success;
      if constexpr (FM == ERRNO) {
        success = errno == 0;
      }
      else if constexpr (FM == NONZERO) {
        success = result == 0;
      }
      else if constexpr (FM == NEGATIVE) {
        success = result >= 0;
      }
      else {
        success = result != nullptr;
      }
      // End check result
      // Begin error propagation
      if (!success) {
        std::string error;
        if constexpr (FM == NULLPTR) {
          error = "Result: <nullptr>";
        }
        else {
          error = std::format("Result: {}\nError Code: {}", result, pcap_strerror(result));
        }
        if constexpr (FirstArgPCap<Function>) {
          if (pc) {
            error += std::format("\nError Message: {} ", pcap_geterr(pc));
          }
        }
        pcap_throw(message, error);
      }
      // End error propagation
      return result;
    }

    PCap(const std::string& dev) {
      PCapCall<NONZERO>(pcap_init, {"pcap_init failed"}, PCAP_CHAR_ENC_UTF_8);
      pc = PCapCall<NULLPTR>(pcap_create, {"pcap_create failed"}, dev.c_str());
    }
  
    ~PCap () {
      if (pc) {
        pcap_close(pc);
      }
    }
    
  };
}

namespace {
  using namespace bandwidthd;
  using namespace lpprogramming;

  template<class T>
  concept StringLiteral = requires(T s) {
    { std::extent_v<std::remove_reference_t<T>> } -> std::convertible_to<std::size_t>;
    requires std::same_as<std::remove_extent_t<std::remove_reference_t<T>>, char>;
  };

  template<size_t T, std::size_t N> requires(N <= T)
  constexpr std::array<char, T> to_array(const char (&lit)[N]) {
    std::array<char, T> result{};
    util::static_for<N>([&]<std::size_t n>(std::integral_constant<std::size_t, n>) {
        result[n] = lit[n];
      });
    return result;
  }

  
  struct test {
    const std::array<char, 128> label;
    const bool pcap_p;
    const bool char_p;
    const size_t label_length;
    

    template<StringLiteral L>
    constexpr test(const L& l, bool a, bool b, const auto& f)
        : label(to_array<128>(l))
        , pcap_p(a == FirstArgPCap<decltype(f)>)
        , char_p(b == LastArgChar<decltype(f)>)
        , label_length(sizeof(L) - 1) {}
  };
  constexpr std::array tests{
    test{"pcap_activate", true, false, pcap_activate}
    , test{"pcap_breakloop", true, false, pcap_breakloop}
    , test{"pcap_can_set_rfmon", true, false, pcap_can_set_rfmon}
    , test{"pcap_close", true, false, pcap_close}
    , test{"pcap_compile", true, false, pcap_compile}
    , test{"pcap_create", false, true, pcap_create}
    , test{"pcap_datalink", true, false, pcap_datalink}
    , test{"pcap_datalink_name_to_val", false, false, pcap_datalink_name_to_val}
    , test{"pcap_datalink_val_to_description", false, false, pcap_datalink_val_to_description}
    , test{"pcap_datalink_val_to_description_or_dlt", false, false, pcap_datalink_val_to_description_or_dlt}
    , test{"pcap_datalink_val_to_name", false, false, pcap_datalink_val_to_name}
    , test{"pcap_dispatch", true, false, pcap_dispatch}
    , test{"pcap_dump", false, false, pcap_dump}
    , test{"pcap_dump_close", false, false, pcap_dump_close}
    , test{"pcap_dump_file", false, false, pcap_dump_file}
    , test{"pcap_dump_flush", false, false, pcap_dump_flush}
    , test{"pcap_dump_fopen", true, false, pcap_dump_fopen}
    , test{"pcap_dump_ftell", false, false, pcap_dump_ftell}
    , test{"pcap_dump_open", true, false, pcap_dump_open}
    , test{"pcap_file", true, false, pcap_file}
    , test{"pcap_fileno", true, false, pcap_fileno}
    , test{"pcap_findalldevs", false, true, pcap_findalldevs}
    , test{"pcap_fopen_offline", false, true, pcap_fopen_offline}
    , test{"pcap_fopen_offline_with_tstamp_precision", false, true, pcap_fopen_offline_with_tstamp_precision}
    , test{"pcap_free_datalinks", false, false, pcap_free_datalinks}
    , test{"pcap_free_tstamp_types", false, false, pcap_free_tstamp_types}
    , test{"pcap_freealldevs", false, false, pcap_freealldevs}
    , test{"pcap_freecode", false, false, pcap_freecode}
    , test{"pcap_get_required_select_timeout", true, false, pcap_get_required_select_timeout}
    , test{"pcap_get_selectable_fd", true, false, pcap_get_selectable_fd}
    , test{"pcap_get_tstamp_precision", true, false, pcap_get_tstamp_precision}
    , test{"pcap_geterr", true, false, pcap_geterr}
    , test{"pcap_getnonblock", true, true, pcap_getnonblock}
    , test{"pcap_init", false, true, pcap_init}
    , test{"pcap_inject", true, false, pcap_inject}
    , test{"pcap_is_swapped", true, false, pcap_is_swapped}
    , test{"pcap_lib_version", false, false, pcap_lib_version}
    , test{"pcap_list_datalinks", true, false, pcap_list_datalinks}
    , test{"pcap_list_tstamp_types", true, false, pcap_list_tstamp_types}
// /*deprecated*/   , test{"pcap_lookupdev", false, true, pcap_lookupdev} 
    , test{"pcap_lookupnet", false, true, pcap_lookupnet}
    , test{"pcap_loop", true, false, pcap_loop}
    , test{"pcap_major_version", true, false, pcap_major_version}
    , test{"pcap_minor_version", true, false, pcap_minor_version}
    , test{"pcap_next", true, false, pcap_next}
    , test{"pcap_next_ex", true, false, pcap_next_ex}
    , test{"pcap_offline_filter", false, false, pcap_offline_filter}
    , test{"pcap_open_dead", false, false, pcap_open_dead}
    , test{"pcap_open_dead_with_tstamp_precision", false, false, pcap_open_dead_with_tstamp_precision}
    , test{"pcap_open_live", false, true, pcap_open_live}
    , test{"pcap_open_offline", false, true, pcap_open_offline}
    , test{"pcap_open_offline_with_tstamp_precision", false, true, pcap_open_offline_with_tstamp_precision}
    , test{"pcap_perror", true, false, pcap_perror}
    , test{"pcap_sendpacket", true, false, pcap_sendpacket}
    , test{"pcap_set_buffer_size", true, false, pcap_set_buffer_size}
    , test{"pcap_set_datalink", true, false, pcap_set_datalink}
    , test{"pcap_set_immediate_mode", true, false, pcap_set_immediate_mode}
    , test{"pcap_set_promisc", true, false, pcap_set_promisc}
    , test{"pcap_set_protocol_linux", true, false, pcap_set_protocol_linux}
    , test{"pcap_set_rfmon", true, false, pcap_set_rfmon}
    , test{"pcap_set_snaplen", true, false, pcap_set_snaplen}
    , test{"pcap_set_timeout", true, false, pcap_set_timeout}
    , test{"pcap_set_tstamp_precision", true, false, pcap_set_tstamp_precision}
    , test{"pcap_set_tstamp_type", true, false, pcap_set_tstamp_type}
    , test{"pcap_setdirection", true, false, pcap_setdirection}
    , test{"pcap_setfilter", true, false, pcap_setfilter}
    , test{"pcap_setnonblock", true, true, pcap_setnonblock}
    , test{"pcap_snapshot", true, false, pcap_snapshot}
    , test{"pcap_stats", true, false, pcap_stats}
    , test{"pcap_statustostr", false, false, pcap_statustostr}
    , test{"pcap_strerror", false, false, pcap_strerror}
    , test{"pcap_tstamp_type_name_to_val", false, false, pcap_tstamp_type_name_to_val}
    , test{"pcap_tstamp_type_val_to_description", false, false, pcap_tstamp_type_val_to_description}
    , test{"pcap_tstamp_type_val_to_name", false, false, pcap_tstamp_type_val_to_name}
    
  };

  // -----------------------------------------------------------------
  // Test runner
  // -----------------------------------------------------------------
  template <size_t index, auto label, size_t label_length, bool a, bool b>
  consteval void test() {
    static_assert(a, std::string_view{label.begin(), label.begin() + label_length});
    static_assert(b, std::string_view{label.begin(), label.begin() + label_length});
  }

  consteval bool run_tests() {
    constexpr auto tc = tests.size();
    util::static_for<tc>([&]<std::size_t n>(std::integral_constant<std::size_t, n>) {
        test<n,
             tests[n].label,
             tests[n].label_length,
             tests[n].pcap_p,
             tests[n].char_p>();
      });
    return true;
  }

  static_assert(run_tests());
}
