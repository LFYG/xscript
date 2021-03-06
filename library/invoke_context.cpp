#include "settings.h"

#include <memory>
#include <string>
#include <stdexcept>
#include <map>

#include "xscript/context.h"
#include "xscript/doc_cache_strategy.h"
#include "xscript/invoke_context.h"
#include "xscript/meta.h"
#include "xscript/string_utils.h"
#include "xscript/tag.h"
#include "xscript/typed_map.h"
#include "xscript/xml_util.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace xscript {

struct InvokeContext::ContextData {
    ContextData() : tagged_(false), result_type_(ERROR),
        have_cached_copy_(false), base_(NULL), meta_(new Meta) {}
    ContextData(InvokeContext *base) :  tagged_(false), result_type_(ERROR),
        have_cached_copy_(false), base_(base), meta_(new Meta) {}

    XmlDocSharedHelper doc_;
    XmlDocSharedHelper meta_doc_;
    bool tagged_;
    Tag tag_;
    ResultType result_type_;
    bool have_cached_copy_;
    boost::shared_ptr<Context> local_context_;
    boost::shared_ptr<TagKey> key_;
    std::string xslt_;

    InvokeContext* base_;
    boost::shared_ptr<Meta> meta_;
    boost::shared_ptr<ArgList> args_;
    std::map<std::string, boost::shared_ptr<ArgList> > extra_args_;
    std::map<std::string, std::string> extra_keys_;

    boost::shared_ptr<XPathExpr> xpointer_;
    boost::shared_ptr<XPathExpr> meta_xpointer_;

    std::vector<std::string> xslt_params_;

    void setExtraArgList(const std::string &name, const boost::shared_ptr<ArgList> &args) {
        std::map<std::string, boost::shared_ptr<ArgList> >::iterator it = extra_args_.find(name);
        if (extra_args_.end() == it) {
            extra_args_.insert(std::make_pair(name, args));
        }
        else {
            it->second = args;
        }
    }

    const ArgList* getExtraArgList(const std::string &name) const {
        std::map<std::string, boost::shared_ptr<ArgList> >::const_iterator it = extra_args_.find(name);
        if (extra_args_.end() == it) {
            return NULL;
        }
        return it->second.get();
    }

    void appendXsltParam(const std::string &value) {
        xslt_params_.push_back(value);
    }

    const std::vector<std::string>& xsltParams() const{
        return xslt_params_;
    }

private:
    ContextData(const ContextData &);
    ContextData& operator = (const ContextData &);
};


InvokeContext::InvokeContext() : ctx_data_(new ContextData())
{}

InvokeContext::InvokeContext(InvokeContext *parent) :
        ctx_data_(new ContextData(parent))
{}

InvokeContext::~InvokeContext() {
}

InvokeContext*
InvokeContext::parent(Context *ctx) const {
    return ctx_data_->base_ ? ctx_data_->base_ : ctx->invokeContext();
}

XmlDocSharedHelper
InvokeContext::resultDoc() const {
    return ctx_data_->doc_;
}

XmlDocSharedHelper
InvokeContext::metaDoc() const {
    return ctx_data_->meta_doc_;
}

InvokeContext::ResultType
InvokeContext::resultType() const {
    return ctx_data_->result_type_;
}

const Tag&
InvokeContext::tag() const {
    return ctx_data_->tag_;
}

bool
InvokeContext::tagged() const {
    return ctx_data_->tagged_;
}

bool
InvokeContext::haveCachedCopy() const {
    return ctx_data_->have_cached_copy_;
}

void
InvokeContext::haveCachedCopy(bool flag) {
    ctx_data_->have_cached_copy_ = flag;
}

void
InvokeContext::resultDoc(const XmlDocSharedHelper &doc) {
    ctx_data_->doc_.reset();
    ctx_data_->doc_ = doc;
}

void
InvokeContext::resultDoc(XmlDocHelper doc) {
    ctx_data_->doc_ = XmlDocSharedHelper(doc.release());
}

void
InvokeContext::metaDoc(const XmlDocSharedHelper &doc) {
    ctx_data_->meta_doc_.reset();
    ctx_data_->meta_doc_ = doc;
}

void
InvokeContext::metaDoc(XmlDocHelper doc) {
    ctx_data_->meta_doc_ = XmlDocSharedHelper(doc.release());
}

void
InvokeContext::resultType(ResultType type) {
    if ((SUCCESS == type) && (NO_CACHE == ctx_data_->result_type_)) {
        return;
    }
    ctx_data_->result_type_ = type;
}

void
InvokeContext::tag(const Tag &tag) {
    ctx_data_->tag_ = tag;
    ctx_data_->tagged_ = true;
}

void
InvokeContext::resetTag() {
    ctx_data_->tag_ = Tag();
    ctx_data_->tagged_ = false;
}

void
InvokeContext::tagKey(const boost::shared_ptr<TagKey> &key) {
    ctx_data_->key_ = key;
}

bool
InvokeContext::error() const {
    return ERROR == ctx_data_->result_type_;
}

bool
InvokeContext::meta_error() const {
    return META_ERROR == ctx_data_->result_type_;
}

bool
InvokeContext::success() const {
    return SUCCESS == ctx_data_->result_type_;
}

bool
InvokeContext::noCache() const {
    return NO_CACHE == ctx_data_->result_type_;
}

TagKey*
InvokeContext::tagKey() const {
    return ctx_data_->key_.get();
}

void
InvokeContext::setLocalContext(const boost::shared_ptr<Context> &ctx) {
    ctx_data_->local_context_ = ctx;
}

const boost::shared_ptr<Context>&
InvokeContext::getLocalContext() const {
    return ctx_data_->local_context_;
}

boost::shared_ptr<Meta>
InvokeContext::meta() const {
    return ctx_data_->meta_;
}

void
InvokeContext::setMeta(const boost::shared_ptr<Meta> &meta) {
    ctx_data_->meta_ = meta;
}

bool
InvokeContext::isMeta() const {
    return NULL != ctx_data_->base_;
}

const std::string&
InvokeContext::xsltName() const {
    return ctx_data_->xslt_;
}

void
InvokeContext::xsltName(const std::string &name) {
    ctx_data_->xslt_ = name;
}

void
InvokeContext::setArgList(const boost::shared_ptr<ArgList> &args) {
    ctx_data_->args_ = args;
}

ArgList*
InvokeContext::getArgList() const {
    return ctx_data_->args_.get();
}

void
InvokeContext::setExtraArgList(const std::string &name, const boost::shared_ptr<ArgList> &args) {
    ctx_data_->setExtraArgList(name, args);
}

const ArgList*
InvokeContext::getExtraArgList(const std::string &name) const {
    return ctx_data_->getExtraArgList(name);
}


void
InvokeContext::setXPointer(const boost::shared_ptr<XPathExpr> &xpointer) {
    ctx_data_->xpointer_ = xpointer;
}

const boost::shared_ptr<XPathExpr>&
InvokeContext::xpointer() const {
    return ctx_data_->xpointer_;
}

void
InvokeContext::setMetaXPointer(const boost::shared_ptr<XPathExpr> &xpointer) {
    ctx_data_->meta_xpointer_ = xpointer;
}

const boost::shared_ptr<XPathExpr>&
InvokeContext::metaXPointer() const {
    return ctx_data_->meta_xpointer_;
}

void
InvokeContext::appendXsltParam(const std::string &value) {
    ctx_data_->appendXsltParam(value);
}

const std::vector<std::string>&
InvokeContext::xsltParams() const {
    return ctx_data_->xsltParams();
}

const std::string&
InvokeContext::extraKey(const std::string &key) const {
    std::map<std::string, std::string>::const_iterator it = ctx_data_->extra_keys_.find(key);
    if (it != ctx_data_->extra_keys_.end()) {
        return it->second;
    }
    return StringUtils::EMPTY_STRING;
}

void
InvokeContext::extraKey(const std::string &key, std::string &value) {
    ctx_data_->extra_keys_[key].assign(value);
}

} // namespace xscript
