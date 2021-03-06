#ifndef _XSCRIPT_LOGGER_H_
#define _XSCRIPT_LOGGER_H_

#include <cstdarg>

namespace xscript {


class Logger {
public:
    enum LogLevel {
        LEVEL_CRIT  = 1,
        LEVEL_ERROR = 2,
        LEVEL_WARN  = 3,
        LEVEL_INFO  = 4,
        LEVEL_DEBUG = 5,
    };

    explicit Logger(LogLevel level);
    virtual ~Logger();

    void exiting(const char *function);
    void entering(const char *function);

    void crit(const char *format, ...)
#ifdef __GNUC__
    __attribute__ ((format (printf, 2, 3)))
#endif
    ;

    void error(const char *format, ...)
#ifdef __GNUC__
    __attribute__ ((format (printf, 2, 3)))
#endif
    ;

    void warn(const char *format, ...)
#ifdef __GNUC__
    __attribute__ ((format (printf, 2, 3)))
#endif
    ;

    void info(const char *format, ...)
#ifdef __GNUC__
    __attribute__ ((format (printf, 2, 3)))
#endif
    ;

    void debug(const char *format, ...)
#ifdef __GNUC__
    __attribute__ ((format (printf, 2, 3)))
#endif
    ;

    static void xmllog(const char *format, va_list args);
    static void xmllog(void *ctx, const char *format, ...);

    bool enabledCrit() const {
        return level_ >= LEVEL_CRIT;
    }

    bool enabledError() const {
        return level_ >= LEVEL_ERROR;
    }

    bool enabledWarn() const {
        return level_ >= LEVEL_WARN;
    }

    bool enabledInfo() const {
        return level_ >= LEVEL_INFO;
    }

    bool enabledDebug() const {
        return level_ >= LEVEL_DEBUG;
    }

    LogLevel level() const {
        return level_;
    }

    void level(LogLevel value) {
        level_ = value;
    }

    unsigned char flags() const {
        return flags_;
    }

    void setFlag(unsigned char mask) {
        flags_ |= mask;
    }

    virtual void logRotate() = 0;

protected:
    LogLevel level_;
    unsigned char flags_;

    virtual void critInternal(const char *format, va_list args) = 0;
    virtual void errorInternal(const char *format, va_list args) = 0;
    virtual void warnInternal(const char *format, va_list args) = 0;
    virtual void infoInternal(const char *format, va_list args) = 0;
    virtual void debugInternal(const char *format, va_list args) = 0;
};

Logger* log();


} // namespace xscript

#endif // _XSCRIPT_LOGGER_H_
