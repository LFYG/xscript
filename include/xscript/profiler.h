#ifndef _XSCRIPT_PROFILER_H_
#define _XSCRIPT_PROFILER_H_

#include <sys/time.h>
#include <boost/cstdint.hpp>
#include <boost/function.hpp>

namespace xscript {

class Logger;

/**
 * Calculate timeval delta in milliseconds
 */
boost::uint64_t
operator - (const timeval &endTime, const timeval &startTime);

/**
 * Profile some function.
 * Wraps function end returns pair of function res and passed time.
 */
template<typename Ret> std::pair<Ret, boost::uint64_t>
profile(const boost::function<Ret ()>& f) {
    struct timeval startTime, endTime;
    gettimeofday(&startTime, 0);

    Ret res = f();

    gettimeofday(&endTime, 0);
    return std::make_pair(res, endTime - startTime);
}

/**
 * Specialisation of profile for functions returning void.
 * Return just passed time.
 */
boost::uint64_t
profile(const boost::function<void ()>& f);

/**
 * Scoped profiler.
 */
class Profiler {
public:
    /**
     * Create profiler.
     * \param log   Logger to output to.
     * \param info  Info string appened to output.
     */
    Profiler(Logger* log, const std::string& info);

    /**
     * Destructor which output profiling info into log.
     */
    ~Profiler();
private:
    Logger *log_;
    std::string info_;
    timeval startTime_;
};

} // namespace xscript

/**
 * Macro for lazy evaluating Profiler info.
 */
#define PROFILER(log,info) \
    std::auto_ptr<xscript::Profiler> __p; \
    if ((log)->level() >= xscript::Logger::LEVEL_INFO) \
        __p.reset(new xscript::Profiler((log), (info)));

#endif // _XSCRIPT_PROFILER_H_
