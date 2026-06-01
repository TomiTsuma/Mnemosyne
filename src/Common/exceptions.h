#pragma once

#include <stdexcept>
#include <string>
#include <source_location>
#include <cstdint>

namespace mnesso::common {

// ── Base exception for all Mnemosyne errors ──
class Exception : public std::runtime_error {
public:
    explicit Exception(std::string message, int error_code = 0);

    [[nodiscard]] const char* what() const noexcept override;

    // Append a helpful backtrace and context
    std::string backtrace() const;

    [[nodiscard]] int error_code() const;

protected:
    int code_;
    // TODO: add backtrace capture on exception throw
    std::string trace_;
};

// ── Guard against Windows socket macro polluting our enum namespace ──
#ifdef NO_DATA
#undef NO_DATA
#endif

// ── Common error codes ──
enum class ErrorCode : uint32_t {
    UNKNOWN_ERROR       = 1000,
    UNKNOWN_TYPE        = 1001,
    UNKNOWN_DATABASE    = 1002,
    UNKNOWN_TABLE       = 1003,
    UNKNOWN_COLUMN      = 1004,
    UNKNOWN_FUNCTION    = 1005,

    SYNTAX_ERROR        = 1100,
    BAD_TYPE_NAME       = 1101,
    ILLEGAL_TYPE        = 1102,

    NO_DATA             = 1200,
    TOO_LARGE_STRING    = 1201,
    MEMORY_LIMIT_EXCEED = 1202,

    NET_ERROR           = 1300,
    CANNOT_CONNECT      = 1301,
    CANNOT_WRITE        = 1302,
    CANNOT_READ         = 1303,

    STORAGE_ERROR       = 1400,
    CANNOT_OPEN_FILE    = 1401,
    CANNOT_MKDIR        = 1402,
    PARTIAL_KILLED      = 1403,

    LOGICAL_ERROR       = 9900,
    NOT_IMPLEMENTED     = 9901,
    INCORRECT_DATA      = 9902,
};

// ── Specific exception types ──
class SyntaxError       : public Exception { public: using Exception::Exception; };
class UnknownTypeError  : public Exception { public: using Exception::Exception; };
class UnknownDatabaseError : public Exception { public: using Exception::Exception; };
class UnknownTableError : public Exception { public: using Exception::Exception; };
class UnknownColumnError : public Exception { public: using Exception::Exception; };
class UnknownFunctionError : public Exception { public: using Exception::Exception; };

class NoDataError       : public Exception { public: using Exception::Exception; };
class TooLargeStringError : public Exception { public: using Exception::Exception; };
class MemoryLimitError  : public Exception { public: using Exception::Exception; };

class NetError          : public Exception { public: using Exception::Exception; };
class CannotConnectError : public Exception { public: using Exception::Exception; };
class CannotWriteError  : public Exception { public: using Exception::Exception; };
class CannotReadError   : public Exception { public: using Exception::Exception; };

class StorageError      : public Exception { public: using Exception::Exception; };
class CannotOpenFileError : public Exception { public: using Exception::Exception; };
class CannotMkDirError  : public Exception { public: using Exception::Exception; };

class LogicalError      : public Exception { public: using Exception::Exception; };
class NotImplemented    : public Exception { public: using Exception::Exception; };
class IncorrectData     : public Exception { public: using Exception::Exception; };

class FatalError        : public Exception { public: using Exception::Exception; };

// ── Helper macro to throw typed exceptions ──
#define THROW_EXCEPTION(Type, code, msg) \
    throw Type{std::format("[{}] {}", code, msg), code}

#define THROW_MNEM(logical_msg) \
    THROW_EXCEPTION(LogicalError, 9900, logical_msg)

} // namespace mnesso::common
