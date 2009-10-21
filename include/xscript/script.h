#ifndef _XSCRIPT_SCRIPT_H_
#define _XSCRIPT_SCRIPT_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include <boost/cstdint.hpp>
#include <boost/thread/mutex.hpp>

#include <xscript/functors.h>
#include <xscript/object.h>
#include <xscript/xml.h>
#include <xscript/xml_helpers.h>

namespace xscript {

class Block;
class Context;
class ScriptFactory;
class ScriptHandlerRegisterer;

/**
 * Parsed script.
 *
 * Stores parse blocks, associated stylesheet and various processing flags.
 * Created by ScriptFactory (for caching purposes).
 */

class Script : public virtual Object, public Xml {
public:
    virtual ~Script();

    bool forceStylesheet() const;
    bool binaryPage() const;
    boost::uint32_t expireTimeDelta() const;
    bool expireTimeDeltaUndefined() const;
    time_t cacheTime() const;
    boost::int32_t pageRandomMax() const;
    bool allowMethod(const std::string& value) const;
    const Block* block(unsigned int n) const;
    const Block* block(const std::string &id, bool throw_error = true) const;
    unsigned int blocksNumber() const;
    const std::map<std::string, std::string>& headers() const;
    const std::string& extensionProperty(const std::string &name) const;
    
    std::string info(const Context *ctx) const;
    bool cachable(const Context *ctx, bool for_save) const;
    
    virtual XmlDocHelper invoke(boost::shared_ptr<Context> ctx);
    virtual void applyStylesheet(boost::shared_ptr<Context> ctx, XmlDocHelper &doc);
    
    virtual std::string fullName(const std::string &name) const;
    
    virtual std::string createTagKey(const Context *ctx) const;
    std::string createTagKey(const Context *ctx, bool page_cache) const;
    
    void addExpiresHeader(const Context *ctx) const;

    class HandlerRegisterer;
protected:
    static const std::string PARSE_XSCRIPT_NODE_METHOD;
    static const std::string REPLACE_XSCRIPT_NODE_METHOD;
    static const std::string PROPERTY_METHOD;
    static const std::string CACHABLE_METHOD;
    
    Script(const std::string &name);

    void parse(const std::string &xml);
    
    virtual void postParse();
    
private:
    friend class ScriptFactory;
    
    // workaround for woody
    class ParseXScriptNodeHandler;
    friend class ParseXScriptNodeHandler;
    class ReplaceXScriptNodeHandler;
    friend class ReplaceXScriptNodeHandler;
    class PropertyHandler;
    friend class PropertyHandler;
    class CachableHandler;
    friend class CachableHandler;
    
    class ScriptData;
    // workaround for woody
    friend class ScriptData;
    ScriptData *data_;
};

} // namespace xscript

#endif // _XSCRIPT_SCRIPT_H_
