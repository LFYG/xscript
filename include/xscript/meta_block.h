#ifndef _XSCRIPT_META_BLOCK_H_
#define _XSCRIPT_META_BLOCK_H_

#include <xscript/block.h>

namespace xscript {

class MetaBlock : public Block {
public:
    MetaBlock(const Block *block, xmlNodePtr node);
    virtual ~MetaBlock();

    virtual void call(boost::shared_ptr<Context> ctx, boost::shared_ptr<InvokeContext> invoke_ctx) const throw (std::exception);
    void callLua(boost::shared_ptr<Context> ctx, boost::shared_ptr<InvokeContext> invoke_ctx);
    void callCacheMissLua(boost::shared_ptr<Context> ctx, boost::shared_ptr<InvokeContext> invoke_ctx);

    const std::string& getTagKey() const;

protected:
    virtual void parseSubNode(xmlNodePtr node);
    virtual void property(const char *name, const char *value);
    virtual void postParse();
    bool luaNode(const xmlNodePtr node) const;
    bool cacheMissLuaNode(const xmlNodePtr node) const;

private:
    MetaBlock(const MetaBlock &);
    MetaBlock& operator = (const MetaBlock &);
    void parseLua(xmlNodePtr node, std::auto_ptr<Block> &block);
    Range getLuaCode(Block *lua) const;

private:
    const Block* parent_;
    std::auto_ptr<Block> cache_miss_lua_block_;
    std::auto_ptr<Block> lua_block_;
    std::string root_name_;
    xmlNs *root_ns_;
    std::string key_;
};

} // namespace xscript

#endif // _XSCRIPT_META_BLOCK_H_
