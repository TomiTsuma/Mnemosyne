// src/Common/exceptions.cpp — Exception class implementation
// Mnemosyne: A column-oriented analytical DBMS

#include "exceptions.h"

namespace mnesso::common {

Exception::Exception(std::string message, int error_code)
    : std::runtime_error{std::move(message)}, code_{error_code} {}

std::string Exception::what() const noexcept {
    if (!trace_.empty()) {
        return std::runtime_error::what() + "\n" + trace_;
    }
    return std::runtime_error::what();
}

int Exception::error_code() const { return code_; }

std::string Exception::backtrace() const { return trace_; }

} // namespace mnesso::common
