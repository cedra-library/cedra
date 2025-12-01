#pragma once

#include <string_view>
#include <sstream>
#include <cdr/types/integers.h>

namespace cdr::internal {

class CheckImpl {
public:

    CheckImpl(std::string_view condition, std::string_view file, int line)
        : condition_(condition)
        , file_(file)
        , line_(line)
    {}


    std::ostream& Stream() {
        return stream_;
    }

    template<typename T>
    CheckImpl& operator<<(T&& value) {
        stream_ << value;
        return *this;
    }

private:
    std::stringstream stream_;
    std::string_view condition_;
    std::string_view file_;
    u32 line_;
};

} // namespace cdr::internal

