#include <chrono>
#include <string>
#include <sstream>
#include <vector>
#include <set>
#include <iterator>

namespace Bosma {
    using Clock = std::chrono::system_clock;

    inline void add(std::tm &tm, Clock::duration time) {
      auto tp = Clock::from_time_t(std::mktime(&tm));
      auto tp_adjusted = tp + time;
      auto tm_adjusted = Clock::to_time_t(tp_adjusted);
      tm = *std::localtime(&tm_adjusted);
    }

    class BadCronExpression : public std::exception {
    public:
        explicit BadCronExpression(std::string msg) : msg_(std::move(msg)) {}

        const char *what() const noexcept override { return (msg_.c_str()); }

    private:
        std::string msg_;
    };

    inline void
    verify_and_set(const std::string &token, const std::string &expression, std::set<int> &fields, const int lower_bound,
                   const int upper_bound, const bool adjust = false) {
      if (token != "*") {
        std::string sub_token;
        size_t last_after_comma_pos = 0;
        size_t comma_pos = token.find_first_of(',');
        while (comma_pos != std::string::npos) {
          sub_token = token.substr(last_after_comma_pos, comma_pos - last_after_comma_pos);
          int field;
          try {
            field = std::stoi(sub_token);
          } catch (const std::invalid_argument &) {
            throw BadCronExpression("malformed cron string (`" + sub_token + "` not an integer or *): " + expression);
          } catch (const std::out_of_range &) {
            throw BadCronExpression("malformed cron string (`" + sub_token + "` not convertable to int): " + expression);
          }
          if (field < lower_bound || field > upper_bound) {
            std::ostringstream oss;
            oss << "malformed cron string ('" << sub_token << "' must be <= " << upper_bound << " and >= " << lower_bound
                << "): " << expression;
            throw BadCronExpression(oss.str());
          }
          if (adjust)
            field--;
          fields.insert(field);
          last_after_comma_pos = comma_pos + 1;
          comma_pos = token.find_first_of(',', last_after_comma_pos);
        }
      }
    }

    class Cron {
    public:
        explicit Cron(const std::string &expression) {
          std::istringstream iss(expression);
          std::vector<std::string> tokens{std::istream_iterator<std::string>{iss},
                                          std::istream_iterator<std::string>{}};

          if (tokens.size() != 5) throw BadCronExpression("malformed cron string (must be 5 fields): " + expression);

          verify_and_set(tokens[0], expression, minute, 0, 59);
          verify_and_set(tokens[1], expression, hour, 0, 23);
          verify_and_set(tokens[2], expression, day, 1, 31);
          verify_and_set(tokens[3], expression, month, 1, 12, true);
          verify_and_set(tokens[4], expression, day_of_week, 0, 6);
        }

        // http://stackoverflow.com/a/322058/1284550
        Clock::time_point cron_to_next(const Clock::time_point from = Clock::now()) const {
          // get current time as a tm object
          auto now = Clock::to_time_t(from);
          std::tm next(*std::localtime(&now));
          // it will always at least run the next minute
          next.tm_sec = 0;
          add(next, std::chrono::minutes(1));
          while (true) {
            if (!month.empty() && month.find(next.tm_mon) == month.end()) {
              // add a month
              // if this will bring us over a year, increment the year instead and reset the month
              if (next.tm_mon + 1 > 11) {
                next.tm_mon = 0;
                next.tm_year++;
              } else
                next.tm_mon++;

              next.tm_mday = 1;
              next.tm_hour = 0;
              next.tm_min = 0;
              continue;
            }
            if (!day.empty() && day.find(next.tm_mday) == day.end()) {
              add(next, std::chrono::hours(24));
              next.tm_hour = 0;
              next.tm_min = 0;
              continue;
            }
            if (!day_of_week.empty() && day_of_week.find(next.tm_wday) == day_of_week.end()) {
              add(next, std::chrono::hours(24));
              next.tm_hour = 0;
              next.tm_min = 0;
              continue;
            }
            if (!hour.empty() && hour.find(next.tm_hour) == hour.end()) {
              add(next, std::chrono::hours(1));
              next.tm_min = 0;
              continue;
            }
            if (!minute.empty() && minute.find(next.tm_min) == minute.end()) {
              add(next, std::chrono::minutes(1));
              continue;
            }
            break;
          }

          // telling mktime to figure out dst
          next.tm_isdst = -1;
          return Clock::from_time_t(std::mktime(&next));
        }

        std::set<int> minute, hour, day, month, day_of_week;
    };
}