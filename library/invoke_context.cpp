#include "settings.h"

#include <memory>
#include <string>
#include <stdexcept>

#include <boost/thread/mutex.hpp>

#include "xscript/context.h"
#include "xscript/doc_cache_strategy.h"
#include "xscript/invoke_context.h"
#include "xscript/meta.h"
#include "xscript/tag.h"
#include "xscript/typed_map.h"
#include "xscript/xml_util.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace xscript {

struct InvokeContext::ContextData {
    ContextData() : doc_(new XmlDocHelper()),
        tagged_(false), result_type_(ERROR), have_cached_copy_(false), parent_(NULL),
        meta_(new Meta), is_meta_(false) {}
    ContextData(InvokeContext *parent) : doc_(new XmlDocHelper()),
        tagged_(false), result_type_(ERROR), have_cached_copy_(false), parent_(parent),
        meta_(new Meta), is_meta_(false) {}
    XmlDocSharedHelper doc_;
    XmlDocSharedHelper meta_doc_;
    bool tagged_;
    Tag tag_;
    ResultType result_type_;
    bool have_cached_copy_;
    boost::shared_ptr<Context> local_context_;
    boost::shared_ptr<TagKey> key_;

    InvokeContext* parent_;
    boost::shared_ptr<Meta> meta_;
    bool is_meta_;
};

InvokeContext::InvokeContext() : ctx_data_(new ContextData())
{}

InvokeContext::InvokeContext(InvokeContext *parent) :
        ctx_data_(new ContextData(parent))
{}

InvokeContext::~InvokeContext() {
    delete ctx_data_;
}

InvokeContext*
InvokeContext::parent(Context *ctx) const {
    return ctx_data_->parent_ ? ctx_data_->parent_ : ctx->invokeContext();
}

XmlDocSharedHelper
InvokeContext::resultDoc() const {
    return ctx_data_->doc_;
}

XmlDocSharedHelper
InvokeContext::metaDoc() const {
    return ctx_data_->meta_doc_;
}

xmlDocPtr
InvokeContext::resultDocPtr() const {
    return ctx_data_->doc_->get();
}

xmlDocPtr
InvokeContext::metaDocPtr() const {
    return ctx_data_->meta_doc_.get() ? ctx_data_->meta_doc_->get() : NULL;
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
    if (NULL == doc.get()) {
        throw std::logic_error("Cannot add NULL doc to invoke context");
    }
    ctx_data_->doc_.reset();
    ctx_data_->doc_ = doc;
}

void
InvokeContext::resultDoc(XmlDocHelper doc) {
    ctx_data_->doc_ = XmlDocSharedHelper(new XmlDocHelper(doc));
}

void
InvokeContext::metaDoc(const XmlDocSharedHelper &doc) {
    if (NULL == doc.get()) {
        throw std::logic_error("Cannot add NULL meta doc to invoke context");
    }
    ctx_data_->meta_doc_ = doc;
}

void
InvokeContext::metaDoc(XmlDocHelper doc) {
    ctx_data_->meta_doc_ = XmlDocSharedHelper(new XmlDocHelper(doc));
}

void
InvokeContext::resultType(ResultType type) {
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
InvokeContext::getLocalContext() {
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

void
InvokeContext::setMetaParam(const std::string &name, const std::string &value) {
    if (ctx_data_->is_meta_) {
        ctx_data_->meta_->set(name, value);
    }
    else {
        ctx_data_->meta_->set2Core(name, value);
    }
}

void
InvokeContext::setMetaFlag() {
    ctx_data_->is_meta_ = true;
}

} // namespace xscript
