module;
export module SyscallHelper;
import std;
import Exception;
import <errno.h>;

using Exception::Severity;
#define SException(m, l, f) Exception::Exception(((errno == EINTR) ? Severity::Temporary : Severity::Fatal), m + std::strerror(errno), l, f)

export namespace SyscallHelper {
  template<bool again = true, typename Function, typename... Args>
  inline auto Syscall(Function *function, int LineNumber, const char *filename, std::string message, Args... args) {
    typename std::invoke_result<Function, Args...>::type result;
    //r result;
    do {
      errno = 0;
      result = function(args...);
      if constexpr (!again) {
        if (errno == EAGAIN) {
          return result;
        }
      }
    } while (errno == EAGAIN || errno == EINTR);
    if (result < 0) {
      throw SException(message, LineNumber, filename);
    }
    return result;
  }
} // namespace SyscallHelper
