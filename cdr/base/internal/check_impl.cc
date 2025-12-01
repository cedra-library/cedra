#include <iostream>
#include <sstream>
#include <cdr/base/internal/check_impl.h>

namespace cdr::internal {

CheckImpl::CheckImpl(bool result, std::string_view condition, std::string_view file, u32 line)
    : result_(result)
{
    if (result_) {
        return;
    }

    new(&termination_) TerminationInfo{
        .stream_ = std::stringstream(),
        .condition_ = condition,
        .file_ = file,
        .line_ = line
    };
}


std::ostream& CheckImpl::Stream() {
    return termination_.stream_;
}

CheckImpl::~CheckImpl() {
    if (!result_) {
        std::cerr << "*** CDR CHECK FAILURE ***\n"
          << "Condition: " << termination_.condition_
          << "\nFile: " << termination_.file_
          << "\nLine: " << termination_.line_
          << "\nMessage:\n" << termination_.stream_.str()
          << std::endl;
        std::abort();
    }
}

} // namespace cdr::internal
