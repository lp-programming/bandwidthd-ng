module;

export module string_util;
import <string>;
import <vector>;
import <sstream>;
import <numeric>;
import <algorithm>;
import <ranges>;
import <functional>;
import <numeric>;
import <vector>;
import static_for;

export
namespace util {
  inline std::vector<std::string> string_split(const std::string_view s, const char sep) {
    std::vector<std::string> out{};
    
    std::istringstream stream(s);

    std::string token;

    while (std::getline(stream, token, sep)) {
        out.push_back(token);
    }
    return out;
  }
  inline constexpr std::string string_join(const std::string_view sep, const std::vector<std::string> &in) {
    if (in.size() < 1) {
      return "";
    }
    return std::accumulate(
        std::next(in.begin()),
        in.end(),
        in[0],
        [sep](const std::string& a, const std::string& b) {
          return a + sep + b;
        });
  }
  inline constexpr std::string iter_replace(const std::string_view query, const auto& sep, const auto& func) {
    auto parts = std::views::split(query, std::string{sep}) 
      | std::ranges::to<std::vector<std::string>>();
    
    
    if (parts.size() == 0) {
      return std::string{""};
    }

    return std::ranges::fold_left
      (
       parts | std::ranges::views::drop(1),
       parts[0],
       func
       );
    
  }
}
