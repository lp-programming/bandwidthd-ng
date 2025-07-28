module;

export module Exception;
import <string>;
import <exception>;
import <cstdlib>;
export import <iostream>;



export namespace Exception {
  enum class Severity {
    Temporary=0,
    Recoverable,
    Fatal,
    Error
  };
    class Exception : public std::exception {
    public:
    const Severity severity;
    
    const std::string message;
    const unsigned long lineno;
    const char* file;

    Exception(std::string message, Severity severity=Severity::Recoverable, unsigned long LNO=0, const char* file=""): severity(severity), message(message), lineno(LNO), file(file) {}

    template<typename... Args>
    Exception(Severity severity, std::string message, Args... args): Exception(message, severity, args...) {}
    const char* what() const noexcept {
      static std::string description = Describe();
      return description.c_str();
    }
    
    const std::string Describe() const {
      std::string sev;
      switch(severity) {
	case Severity::Temporary:
	  sev = "<Temporary: ";
	  break;
	case Severity::Recoverable:
	  sev = "<Recoverable: ";
	  break;
	case Severity::Fatal:
	  sev = "<Fatal: ";
	  break;
	case Severity::Error:
	  sev = "<Error: ";
	  break;
      }
      if (lineno) {
	sev += std::string{"(at "} + file + ":" + std::to_string(lineno) + ") ";
      }
      return sev + message + " />";
    }
  };

	   

  void terminate() {
    std::exception_ptr exc = std::current_exception();
    try {
      std::rethrow_exception(exc);
    }
    catch (Exception e) {
      printf("Unhandled Exception\n%s\n", e.Describe().c_str());
    }
    catch (...) {
    }
    abort();

  }
  
  void set_terminate() {
    std::set_terminate(&terminate);
  }
  
} // namespace Exception
