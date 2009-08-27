#ifndef _XSCRIPT_STYLESHEET_CACHE_H_
#define _XSCRIPT_STYLESHEET_CACHE_H_

#include <string>

#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>

#include <xscript/component.h>
#include <xscript/stat_builder.h>

namespace xscript {

class Stylesheet;

class StylesheetCache : public Component<StylesheetCache>, public StatBuilderHolder {
public:
    StylesheetCache() : StatBuilderHolder("stylesheet-cache") {
    }

    virtual void clear();
    virtual void erase(const std::string &name);

    virtual boost::shared_ptr<Stylesheet> fetch(const std::string &name);
    virtual void store(const std::string &name, const boost::shared_ptr<Stylesheet> &stylesheet);
    virtual boost::mutex* getMutex(const std::string &name);
};

} // namespace xscript

#endif // _XSCRIPT_STYLESHEET_CACHE_H_
