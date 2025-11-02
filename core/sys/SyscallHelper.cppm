module;
export module SyscallHelper;
import <errno.h>;
import <exception>;
import <format>;
import <system_error>;
import <functional>;
import <system_error>;


export
namespace SyscallHelper {
  template<bool again = true, typename Function, typename... Args>
  inline auto Syscall(Function *function, int LineNumber, const char *filename, std::string message, Args... args) {
    using result = std::invoke_result<Function, Args...>::type;
    result res;
    do {
      errno = 0;
      res = function(args...);
      if constexpr (!again) {
        if (errno == EAGAIN) {
          throw std::system_error({EAGAIN, std::system_category()}, std::format("EAGAIN: {} at {}:{}", message, filename, LineNumber).c_str());
        }
      }
    } while (errno == EAGAIN || errno == EINTR);
    if (res < 0) {
      throw std::system_error({errno, std::system_category()}, std::format("{} at {}:{}", message, filename, LineNumber).c_str());
    }
    return res;
  }
}
