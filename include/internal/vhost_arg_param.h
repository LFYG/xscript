#ifndef _XSCRIPT_INTERNAL_VHOST_ARG_PARAM_H
#define _XSCRIPT_INTERNAL_VHOST_ARG_PARAM_H

#include "xscript/param.h"

namespace xscript {

class VHostArgParam : public TypedParam {
public:
    VHostArgParam(Object *owner, xmlNodePtr node);
    virtual ~VHostArgParam();

    virtual const char* type() const;

    static std::string variable(const Context *ctx, const std::string &name);
    
    static std::auto_ptr<Param> create(Object *owner, xmlNodePtr node);
    static bool is(const Context *ctx, const std::string &name, const std::string &value);

protected:
    virtual ValueResult getValue(const Context *ctx) const;
};

} // namespace xscript

#endif // _XSCRIPT_INTERNAL_VHOST_ARG_PARAM_H
