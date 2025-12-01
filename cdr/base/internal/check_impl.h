#pragma once

#include <sstream>
#include <string_view>
#include <cdr/types/integers.h>

namespace cdr::internal {

class CheckImpl {
public:

    CheckImpl(bool result, std::string_view condition,
              std::string_view file, u32 line);


    std::ostream& Stream();

    template<typename T>
    CheckImpl& operator<<(T&& value) {
        if (!result_) {
            Stream() << value;
        }
        return *this;
    }

    ~CheckImpl();

private:

    struct TerminationInfo {
        std::stringstream stream_;
        std::string_view condition_;
        std::string_view file_;
        u32 line_;
    };

    union {
        TerminationInfo termination_;
        struct {} none_;
    };
    bool result_;
};

} // namespace cdr::internal

