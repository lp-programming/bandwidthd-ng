export module Syslog;

import <syslog.h>;
import <string>;
import <format>;
import <print>;
import <vector>;

namespace Syslog {

  export
  enum class Priority : int {
    EMERG   = LOG_EMERG,
    ALERT   = LOG_ALERT,
    CRIT    = LOG_CRIT,
    ERR     = LOG_ERR,
    WARNING = LOG_WARNING,
    NOTICE  = LOG_NOTICE,
    INFO    = LOG_INFO,
    DEBUG   = LOG_DEBUG

  };

  export
  enum class Facility : int {
    USER     = LOG_USER,
    DAEMON   = LOG_DAEMON,
    LOCAL0   = LOG_LOCAL0,
    LOCAL1   = LOG_LOCAL1,
    LOCAL2   = LOG_LOCAL2,
    LOCAL3   = LOG_LOCAL3,
    LOCAL4   = LOG_LOCAL4,
    LOCAL5   = LOG_LOCAL5,
    LOCAL6   = LOG_LOCAL6,
    LOCAL7   = LOG_LOCAL7
  };

  export
  enum class Option : int {
    PID     = LOG_PID,
    CONS    = LOG_CONS,
    NDELAY  = LOG_NDELAY,
    ODELAY  = LOG_ODELAY,
    NOWAIT  = LOG_NOWAIT,
    PERROR  = LOG_PERROR
  };

  inline constexpr static std::string formatPriority(const Priority p) {
    switch (p) {
    case Priority::EMERG:
      return "EMERG";
    case Priority::ALERT:
      return "ALERT";
    case Priority::CRIT:
      return "CRIT";
    case Priority::ERR:
        return "ERR";
      case Priority::WARNING:
        return "WARNING";
    case Priority::NOTICE:
      return "NOTICE";
    case Priority::INFO:
      return "INFO";
    default:
      return "DEBUG";
    }
  }
  
  export
  inline Priority constexpr operator&(const Priority a, const Priority b) {
    return static_cast<Priority>(static_cast<int>(a) & static_cast<int>(b));
  }


  export
  inline constexpr Option operator|(Option a, Option b) {
    return static_cast<Option>(static_cast<int>(a) | static_cast<int>(b));
  }

  export
  class Logger {
  public:
    static void init(const std::string& ident,
                     const Option options = Option::PID,
                     const Facility facility = Facility::USER) noexcept
    {
      if (initialized_) {
        return;
      }
      if (ident.size() > 0) {
        ::openlog(ident.c_str(), static_cast<int>(options), static_cast<int>(facility));
        initialized_ = true;
      }
    }

    static void shutdown() noexcept {
      if (!initialized_) {
        return;
      }

      ::closelog();
      initialized_ = false;
    }

    static void log(Priority priority, std::string_view message) noexcept {
      std::println("{}: {}", formatPriority(priority), message);
      if (initialized_) {
        ::syslog(static_cast<int>(priority), "%.*s", static_cast<int>(message.size()), message.data());
      }
    }
    static inline void info(std::string_view message) noexcept {
      log(Priority::INFO, message);
    }
    static inline void err(std::string_view message) noexcept {
      log(Priority::ERR, message);
    }
    using Priority::ERR;
    using Priority::INFO;

  private:
    inline static bool initialized_ = false;
  };
  
}

