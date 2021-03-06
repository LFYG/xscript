#include "settings.h"

#include <cerrno>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <algorithm>

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/checked_delete.hpp>
#include <boost/tokenizer.hpp>

#include <boost/thread/xtime.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

#include <libxml/uri.h>
#include <libxml/xinclude.h>

#include "xscript/block.h"
#include "xscript/cache_strategy.h"
#include "xscript/context.h"
#include "xscript/doc_cache.h"
#include "xscript/exception.h"
#include "xscript/extension.h"
#include "xscript/http_utils.h"
#include "xscript/logger.h"
#include "xscript/message_interface.h"
#include "xscript/operation_mode.h"
#include "xscript/policy.h"
#include "xscript/profiler.h"
#include "xscript/response.h"
#include "xscript/script.h"
#include "xscript/stylesheet.h"
#include "xscript/stylesheet_factory.h"
#include "xscript/threaded_block.h"
#include "xscript/xml_util.h"

#include "internal/param_factory.h"
#include "internal/extension_list.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace xscript {

static const boost::uint32_t EXPIRE_TIME_DELTA_UNDEFINED = std::numeric_limits<boost::uint32_t>::max();
static const std::string GET_METHOD = "GET";

const std::string Script::PARSE_XSCRIPT_NODE_METHOD = "SCRIPT_PARSE_XSCRIPT_NODE";
const std::string Script::REPLACE_XSCRIPT_NODE_METHOD = "SCRIPT_REPLACE_XSCRIPT_NODE";
const std::string Script::PROPERTY_METHOD = "SCRIPT_PROPERTY";
const std::string Script::CACHABLE_METHOD = "SCRIPT_CACHABLE";

class Script::ScriptData {
public:  
    ScriptData(Script *owner);
    ~ScriptData();
    
    bool threaded() const;
    bool forceStylesheet() const;
    bool binaryPage() const;
    boost::uint32_t expireTimeDelta() const;
    bool expireTimeDeltaUndefined() const;
    bool allowMethod(const std::string &value) const;
    
    const std::map<std::string, std::string>& headers() const;
    std::set<xmlNodePtr>& xscriptNodes() const;
    
    unsigned int blocksNumber() const;
    Block* block(unsigned int n) const;
    Block* block(const std::string &id, bool throw_error) const;
    std::vector<Block*>& blocks();
    
    void addBlock(Block *block);
    void addXscriptNode(xmlNodePtr node);
    void addHeader(const std::string &name, const std::string &value);
    void setHeaders(Response *response) const;

    void threaded(bool value);
    void forceStylesheet(bool value);
    void expireTimeDelta(boost::uint32_t value);
    void binaryPage(bool value);
    void allowMethods(const char *value);
    void flag(unsigned int type, bool value);
    
    void parseNode(xmlNodePtr node, std::vector<xmlNodePtr> &xscript_nodes);
    void parseStylesheetNode(const xmlNodePtr node);
    void parseHeadersNode(xmlNodePtr node);
    void parseXScriptNodes(std::vector<xmlNodePtr> &xscript_nodes);
    void parseBlocks();
    void buildXScriptNodeSet(std::vector<xmlNodePtr> &xscript_nodes);
    void addHeaders(Context *ctx) const;
    XmlDocSharedHelper fetchResults(Context *ctx) const;
    void fetchRecursive(Context *ctx, xmlNodePtr node, xmlNodePtr newnode,
                        unsigned int &count, unsigned int &xscript_count) const;
    void parseXScriptNode(const xmlNodePtr node);
    void replaceXScriptNode(xmlNodePtr node, xmlNodePtr newnode, Context *ctx) const;
    void property(const char *name, const char *value);
    bool cachable(const Context *ctx, bool for_save);

    const Script *parent_;
    Script *owner_;
    XmlDocHelper doc_;
    std::vector<Block*> blocks_;
    unsigned int flags_;
    boost::uint32_t expire_time_delta_;
    std::set<xmlNodePtr> xscript_node_set_;
    std::map<std::string, std::string> headers_;
    std::vector<std::string> allow_methods_;
    
    static const unsigned int FLAG_THREADED = 1;
    static const unsigned int FLAG_FORCE_STYLESHEET = 1 << 1;
    static const unsigned int FLAG_BINARY_PAGE = 1 << 2;
};

class Script::ParseXScriptNodeHandler : public MessageHandler {
    Result process(const MessageParams &params, MessageResultBase &result);
};
class Script::ReplaceXScriptNodeHandler : public MessageHandler {
    Result process(const MessageParams &params, MessageResultBase &result);
};
class Script::PropertyHandler : public MessageHandler {
    Result process(const MessageParams &params, MessageResultBase &result);
};
class Script::CachableHandler : public MessageHandler {
    Result process(const MessageParams &params, MessageResultBase &result);
};

Script::ScriptData::ScriptData(Script *owner) :
    parent_(NULL), owner_(owner), flags_(FLAG_FORCE_STYLESHEET),
    expire_time_delta_(EXPIRE_TIME_DELTA_UNDEFINED)
{}

Script::ScriptData::~ScriptData() {
    std::for_each(blocks_.begin(), blocks_.end(), boost::checked_deleter<Block>());
}

bool
Script::ScriptData::threaded() const {
    return flags_ & FLAG_THREADED;
}

bool
Script::ScriptData::forceStylesheet() const {
    return flags_ & FLAG_FORCE_STYLESHEET;
}

bool
Script::ScriptData::binaryPage() const {
    return flags_ & FLAG_BINARY_PAGE;
}

boost::uint32_t
Script::ScriptData::expireTimeDelta() const {
    return expire_time_delta_;
}

bool
Script::ScriptData::allowMethod(const std::string &value) const {
    if (!allow_methods_.empty() &&
        std::find(allow_methods_.begin(), allow_methods_.end(), value) == allow_methods_.end()) {
        return false;
    }
    return true;
}

unsigned int
Script::ScriptData::blocksNumber() const {
    return blocks_.size();
}

Block*
Script::ScriptData::block(unsigned int n) const {
    return blocks_.at(n);
}

Block*
Script::ScriptData::block(const std::string &id, bool throw_error) const {
    try {
        for (std::vector<Block*>::const_iterator i = blocks_.begin(), end = blocks_.end(); i != end; ++i) {
            if ((*i)->id() == id) {
                return (*i);
            }
        }
    }
    catch (const std::exception &e) {
        log()->error("Can not get block by id: %s, owner: %s, error: %s", id.c_str(), owner_->name().c_str(), e.what());
        throw;
    }

    if (throw_error) {
        throw std::invalid_argument("requested block with nonexistent id: " + id + " owner: " + owner_->name());
    }
    return NULL;
}

std::vector<Block*>&
Script::ScriptData::blocks() {
    return blocks_;
}

const std::map<std::string, std::string>&
Script::ScriptData::headers() const {
    return headers_;
}

void
Script::ScriptData::threaded(bool value) {
    flag(FLAG_THREADED, value);
}

void
Script::ScriptData::forceStylesheet(bool value) {
    flag(FLAG_FORCE_STYLESHEET, value);
}

void
Script::ScriptData::expireTimeDelta(boost::uint32_t value) {
    expire_time_delta_ = value;
}

void
Script::ScriptData::binaryPage(bool value) {
    flag(FLAG_BINARY_PAGE, value);
}

void
Script::ScriptData::flag(unsigned int type, bool value) {
    flags_ = value ? (flags_ | type) : (flags_ &= ~type);
}

void
Script::ScriptData::addBlock(Block *block) {
    blocks_.push_back(block);
}

void
Script::ScriptData::addXscriptNode(xmlNodePtr node) {
    xscript_node_set_.insert(node);
}

void
Script::ScriptData::addHeader(const std::string &name, const std::string &value) {
    headers_.insert(std::make_pair<std::string, std::string>(name, value));
}

void
Script::ScriptData::setHeaders(Response *response) const {
    for (std::map<std::string, std::string>::const_iterator i = headers_.begin(), end = headers_.end(); i != end; ++i) {
        response->setHeader(i->first, i->second);
    }
}

bool
Script::ScriptData::expireTimeDeltaUndefined() const {
    return expire_time_delta_ == EXPIRE_TIME_DELTA_UNDEFINED;
}

std::set<xmlNodePtr>&
Script::ScriptData::xscriptNodes() const {
    return const_cast<std::set<xmlNodePtr>&>(xscript_node_set_);
}

void
Script::ScriptData::allowMethods(const char *value) {
    allow_methods_.clear();
    typedef boost::char_separator<char> Separator;
    typedef boost::tokenizer<Separator> Tokenizer;
    std::string method_list(value);
    Tokenizer tok(method_list, Separator(", "));
    for (Tokenizer::iterator it = tok.begin(), it_end = tok.end(); it != it_end; ++it) {
        allow_methods_.push_back(*it);
        std::vector<std::string>::reverse_iterator method = allow_methods_.rbegin();
        std::transform(method->begin(), method->end(), method->begin(),
                       boost::bind(&toupper, _1));
    }
}

void
Script::ScriptData::parseNode(xmlNodePtr node, std::vector<xmlNodePtr> &xscript_nodes) {
    ExtensionList* elist = ExtensionList::instance();
       
    while (NULL != node) {
        if (XML_PI_NODE == node->type) {
            if (xmlStrncasecmp(node->name, (const xmlChar*) "xml-stylesheet", sizeof("xml-stylesheet")) == 0) {
                if (owner_->xsltName().empty()) {
                    log()->debug("%s, parse stylesheet", owner_->name().c_str());
                    parseStylesheetNode(node);
                }
                else {
                    log()->debug("%s, skip stylesheet", owner_->name().c_str());
                }
                xmlNodePtr snode = node;
                node = node->next;
                xmlUnlinkNode(snode);
                xmlFreeNode(snode);
            }
            else {
                node = node->next;
            }
            continue;
        }
        if (XML_ELEMENT_NODE == node->type) {
            if (xmlStrncasecmp(node->name, (const xmlChar*) "xscript", sizeof("xscript")) == 0) {
                xscript_nodes.push_back(node);
                node = node->next;
                continue;
            }
            
            if (xmlStrEqual(node->name, XINCLUDE_FALLBACK) &&
                node->ns && node->ns->href &&
                (xmlStrEqual(node->ns->href, XINCLUDE_NS) ||
                 xmlStrEqual(node->ns->href, XINCLUDE_OLD_NS))) {
                
                node = node->next;
                continue;
            }
    
            Extension *ext = elist->extension(node, true);
            if (NULL != ext) {
                log()->debug("%s, creating block %s", owner_->name().c_str(), ext->name());
                std::auto_ptr<Block> b = ext->createBlock(owner_, node);
                assert(b.get());

                addBlock(b.get());
                b.release();
                node = node->next;
                continue;
            }
        }
        if (node->children) {
            parseNode(node->children, xscript_nodes);
        }
        node = node->next;
    }
}

void
Script::ScriptData::parseStylesheetNode(const xmlNodePtr node) {
    if (node->content) {
        const xmlChar* href = xmlStrstr(node->content, (const xmlChar*) "href=");
        if (NULL != href) {
            href += sizeof("href=") - 1;
            const xmlChar* begin = xmlStrchr(href, '"');
            if (NULL != begin) {
                begin += 1;
                const xmlChar* end = xmlStrchr(begin, '"');
                if (NULL != end) {
                    if (begin == end) {
                        throw std::runtime_error("empty href in stylesheet node");
                    }
                    std::string xslt((const char*)begin, (const char*)end);
                    owner_->xsltName(xslt.c_str());
                    return;
                }
            }
        }
    }
    throw std::runtime_error("can not parse stylesheet node");
}

void
Script::ScriptData::parseHeadersNode(xmlNodePtr node) {
    while (node) {
        if (node->name &&
            xmlStrncasecmp(node->name, (const xmlChar*) "header", sizeof("header")) == 0) {
            const char *name = XmlUtils::attrValue(node, "name"), *value = XmlUtils::attrValue(node, "value");
            if (name && value) {
                addHeader(name, value);
            }
        }
        node = node->next;
    }
}

void
Script::ScriptData::parseXScriptNodes(std::vector<xmlNodePtr> &xscript_nodes) {

    log()->debug("parsing xscript nodes");

    for(std::vector<xmlNodePtr>::reverse_iterator i = xscript_nodes.rbegin(), end = xscript_nodes.rend();
        i != end;
        ++i) {
        parseXScriptNode(*i);
    }
}

void
Script::ScriptData::parseBlocks() {
    
    log()->debug("parsing blocks");

    bool is_threaded = threaded();
    std::vector<Block*> &block_vec = blocks();
    for(std::vector<Block*>::iterator it = block_vec.begin();
        it != block_vec.end();
        ++it) {
        Block *block = *it;
        assert(block);
        block->threaded(is_threaded);
        block->parse();
    }
}

void
Script::ScriptData::buildXScriptNodeSet(std::vector<xmlNodePtr>& xscript_nodes) {

    log()->debug("build xscript node set");

    for(std::vector<xmlNodePtr>::iterator i = xscript_nodes.begin(), end = xscript_nodes.end();
        i != end;
        ++i) {
        addXscriptNode(*i);
    }
    xscript_nodes.clear();
}


void
Script::ScriptData::addHeaders(Context *ctx) const {
    Response *response = ctx->response();
    if (NULL == response) { // request can be null only when running tests
        return;
    }
    response->setExpiresHeader();
    setHeaders(response);
}

XmlDocSharedHelper
Script::ScriptData::fetchResults(Context *ctx) const {

    XmlDocSharedHelper newdoc(xmlCopyDoc(doc_.get(), 1));
    XmlUtils::throwUnless(NULL != newdoc.get());
     
    unsigned int count = 0, xscript_count = 0;
     
    xmlNodePtr node = xmlDocGetRootElement(doc_.get());
    assert(node);
        
    xmlNodePtr newnode = xmlDocGetRootElement(newdoc.get());
    assert(newnode);
        
    fetchRecursive(ctx, node, newnode, count, xscript_count);
    return newdoc;
}

void
Script::ScriptData::fetchRecursive(Context *ctx, xmlNodePtr node, xmlNodePtr newnode,
                                   unsigned int &count, unsigned int &xscript_count) const {
    const std::set<xmlNodePtr>& xscript_node_set = xscriptNodes();
    unsigned int blocks_num = blocksNumber();
    while (node && count + xscript_count != blocks_num + xscript_node_set.size()) {
        if (newnode == NULL) {
            throw std::runtime_error(std::string("internal error in node ") + (char*)node->name);
        }
        xmlNodePtr next = newnode->next;
        if (count < blocks_num && block(count)->node() == node) {
            boost::shared_ptr<InvokeContext> result = ctx->result(count);
            xmlDocPtr doc = result->resultDoc().get();
            assert(doc);
            xmlNodePtr result_doc_root_node = xmlDocGetRootElement(doc);
            if (result_doc_root_node) {
                if (result->error()) {
                    xmlReplaceNode(newnode, xmlCopyNode(result_doc_root_node, 1));
                }
                else {
                    xmlDocPtr meta_doc = result->meta_error() ? NULL : result->metaDoc().get();
                    xmlNodePtr last_node = block(count)->processXPointer(
                        result.get(), doc, meta_doc, newnode, &xmlReplaceNode);
                    meta_doc = result->metaDoc().get();
                    xmlNodePtr root = meta_doc ? xmlDocGetRootElement(meta_doc) : NULL;
                    if (result->meta_error() && root) {  //add error meta
                        if (last_node) {
                            xmlAddNextSibling(last_node, xmlCopyNode(root, 1));
                        }
                        else {
                            xmlReplaceNode(newnode, xmlCopyNode(root, 1));
                        }
                    }
                    else if (NULL == last_node) { // unlink if block and meta responses are empty
                        xmlUnlinkNode(newnode);
                    }
                }
            }
            else {
                xmlUnlinkNode(newnode);
            }
            xmlFreeNode(newnode);
            count++;
        }
        else if (xscript_node_set.find(node) != xscript_node_set.end() ) {
            replaceXScriptNode(node, newnode, ctx);
            xscript_count++;
        }
        else if (node->children) {
            fetchRecursive(ctx, node->children, newnode->children, count, xscript_count);
        }
        node = node->next;
        newnode = next;
    }
}

void
Script::ScriptData::parseXScriptNode(const xmlNodePtr node) {
    MessageParam<Script> script_param(owner_);
    MessageParam<const xmlNodePtr> node_param(&node);
    
    MessageParamBase* param_list[2];
    param_list[0] = &script_param;
    param_list[1] = &node_param;
    
    MessageParams params(2, param_list);
    MessageResultBase result;
  
    MessageProcessor::instance()->process(PARSE_XSCRIPT_NODE_METHOD, params, result);
}

MessageHandler::Result
Script::ParseXScriptNodeHandler::process(const MessageParams &params,
                                         MessageResultBase &result) {
    (void)result;
    
    Script* script = params.getPtr<Script>(0);
    const xmlNodePtr node = params.get<const xmlNodePtr>(1);
    
    log()->debug("parsing xscript node");

    XmlUtils::visitAttributes(node->properties, boost::bind(
        &Script::ScriptData::property, script->data_.get(), _1, _2));

    xmlNodePtr child = node->children;
    for( ; child; child = child->next) {
        if (child->name &&
            xmlStrncasecmp(child->name, (const xmlChar*) "add-headers", sizeof("add-headers")) == 0) {
            script->data_->parseHeadersNode(child->children);
            continue;
        }
        else if (script->xsltParamNode(child)) {
            log()->debug("parsing xslt-param node from script");
            script->parseXsltParamNode(child);
            continue;
        }
        else if (XML_ELEMENT_NODE == child->type) {
            const char *name = (const char *) child->name, *value = XmlUtils::value(child);
            if (name && value) {
                script->data_->property(name, value);
            }
            continue;
        }
    }
    return CONTINUE;
}

void
Script::ScriptData::replaceXScriptNode(xmlNodePtr node, xmlNodePtr newnode, Context *ctx) const {
    MessageParam<Script> script_param(const_cast<Script*>(owner_)); //TODO: remove const_cast
    MessageParam<xmlNodePtr> node_param(&node);
    MessageParam<xmlNodePtr> newnode_param(&newnode);
    MessageParam<Context> context_param(ctx);
    
    MessageParamBase* param_list[4];
    param_list[0] = &script_param;
    param_list[1] = &node_param;
    param_list[2] = &newnode_param;
    param_list[3] = &context_param;
    
    MessageParams params(4, param_list);
    MessageResultBase result;
  
    MessageProcessor::instance()->process(REPLACE_XSCRIPT_NODE_METHOD, params, result);
}

MessageHandler::Result
Script::ReplaceXScriptNodeHandler::process(const MessageParams &params,
                                           MessageResultBase &result) {
    (void)result;
    xmlNodePtr newnode = params.get<xmlNodePtr>(2);
    xmlUnlinkNode(newnode);
    xmlFreeNode(newnode);
    return CONTINUE;
}

void
Script::ScriptData::property(const char *prop, const char *value) {
    MessageParam<Script> script_param(owner_);
    MessageParam<const char> prop_param(prop);
    MessageParam<const char> value_param(value);
    
    MessageParamBase* param_list[3];
    param_list[0] = &script_param;
    param_list[1] = &prop_param;
    param_list[2] = &value_param;
    
    MessageParams params(3, param_list);
    MessageResultBase result;
  
    MessageProcessor::instance()->process(PROPERTY_METHOD, params, result);
}

MessageHandler::Result
Script::PropertyHandler::process(const MessageParams &params,
                                 MessageResultBase &result) {
    (void)result;
    
    Script* script = params.getPtr<Script>(0);
    const char* prop = params.getPtr<const char>(1);
    const char* value = params.getPtr<const char>(2);
    
    log()->debug("%s, setting property: %s=%s", script->name().c_str(), prop, value);

    if (strncasecmp(prop, "all-threaded", sizeof("all-threaded")) == 0) {
        script->data_->threaded(strncasecmp(value, "yes", sizeof("yes")) == 0);
    }
    else if (strncasecmp(prop, "allow-methods", sizeof("allow-methods")) == 0) {
        script->data_->allowMethods(value);
    }
    else if (strncasecmp(prop, "xslt-dont-apply", sizeof("xslt-dont-apply")) == 0) {
        script->data_->forceStylesheet(strncasecmp(value, "yes", sizeof("yes")) != 0);
    }
    else if (strncasecmp(prop, "http-expire-time-delta", sizeof("http-expire-time-delta")) == 0) {
        try {
            script->data_->expireTimeDelta(boost::lexical_cast<boost::uint32_t>(value));
        }
        catch(const boost::bad_lexical_cast &e) {
            throw std::runtime_error(
                std::string("cannot parse http-expire-time-delta value: ") + value);
        }
    }
    else if (strncasecmp(prop, "binary-page", sizeof("binary-page")) == 0) {
        script->data_->binaryPage(strncasecmp(value, "yes", sizeof("yes")) == 0);
    }
    else if (script->checkProperty(prop, value)) {
    }
    else {
        throw std::runtime_error(std::string("invalid script property: ").append(prop));
    }
    return CONTINUE;
}

bool
Script::ScriptData::cachable(const Context *ctx, bool for_save) {
    MessageParam<Script> script_param(owner_);
    MessageParam<const Context> context_param(ctx);
    MessageParam<bool> for_save_param(&for_save);
    
    MessageParamBase* param_list[3];
    param_list[0] = &script_param;
    param_list[1] = &context_param;
    param_list[2] = &for_save_param;
    
    MessageParams params(3, param_list);
    MessageResult<bool> result;
  
    MessageProcessor::instance()->process(CACHABLE_METHOD, params, result);
    return result.get();
}

MessageHandler::Result
Script::CachableHandler::process(const MessageParams &params,
                                 MessageResultBase &result) {
    Script* script = params.getPtr<Script>(0);
    const Context* ctx = params.getPtr<const Context>(1);
    bool for_save = params.get<bool>(2);
    
    if (NULL == script->cacheStrategy() || script->cacheTimeUndefined() ||
            script->cacheTime() < DocCache::instance()->minimalCacheTime()) {
        result.set(false);
        return CONTINUE;
    }
    
    if (ctx->noCache() || ctx->response()->suppressBody(ctx->request())) {
        log()->warn("Cannot cache script. Owner: %s Context is not cachable", script->name().c_str());
        result.set(false);
        return CONTINUE;
    }
        
    if (script->binaryPage()) {
        log()->warn("Cannot cache script. Owner: %s Content is binary", script->name().c_str());
        result.set(false);
        return CONTINUE;
    }
    
    if (ctx->noMainXsltPort()) { // no warning
        log()->info("Cannot cache script. Owner: %s Alternate or noxslt port", script->name().c_str());
        result.set(false);
        return CONTINUE;
    }

    const Request *req = ctx->request();
    const std::string &method = req->getRequestMethod();
    bool cached_method = (method == GET_METHOD);
    if (!cached_method && req->hasPostData()) {
        cached_method = (0 == req->requestBody().second); // allowed for POST/PUT with empty body
    }
    if (!cached_method) {
        log()->warn("Cannot cache script. Owner: %s Method %s is not GET method", method.c_str(), script->name().c_str());
        result.set(false);
        return CONTINUE;
    }
    
    if (for_save) {
        if (ctx->xsltChanged(script)) {
            log()->warn("Cannot cache script. Owner: %s Main stylesheet changed", script->name().c_str());
            result.set(false);
            return CONTINUE;
        }

        int status = ctx->response()->status();
        if (200 != status) {
            log()->warn("Cannot cache script. Owner: %s Status %d is not 200 (OK)", script->name().c_str(), status);
            result.set(false);
            return CONTINUE;
        }
        
        const CookieSet &cookies = ctx->response()->outCookies();       
        for(CookieSet::const_iterator it = cookies.begin(); it != cookies.end(); ++it) {
            if (!Policy::instance()->allowCachingOutputCookie(it->name().c_str())) {
                log()->warn("Cannot cache script. Owner: %s Output cookie %s is not allowed", script->name().c_str(), it->name().c_str());
                result.set(false);
                return CONTINUE;
            }
        }
    }
    
    log()->info("Script %s is cacheable", script->name().c_str());
    result.set(true);
    return CONTINUE;
}

Script::Script(const std::string &name) :
    Xml(name), data_(new ScriptData(this))
{}

Script::~Script() {
}

bool
Script::forceStylesheet() const {
    return data_->forceStylesheet();
}

bool
Script::binaryPage() const {
    return data_->binaryPage();
}

boost::uint32_t
Script::expireTimeDelta() const {
    return data_->expireTimeDelta();
}

bool
Script::allowMethod(const std::string &value) const {
    return data_->allowMethod(value);
}

unsigned int
Script::blocksNumber() const {
    return data_->blocksNumber();
}

const Block*
Script::block(unsigned int n) const {
    return data_->block(n);
}

const Block*
Script::block(const std::string &id, bool throw_error) const {
    return data_->block(id, throw_error);
}

const std::vector<Block*>&
Script::blocks() const {
    return data_->blocks();
}

const std::map<std::string, std::string>&
Script::headers() const {
    return data_->headers();
}

static const std::string STR_ERROR_EMPTY_XML_DOC = "got empty xml doc";
static const std::string STR_ERROR_XML_DOC_WITHOUT_ROOT = "got xml doc without root";

void
Script::parseInternal(const boost::function<xmlDocPtr()> &parserFunc) {
    XmlDocHelper doc(NULL);
    {
        XmlInfoCollector::Starter starter;
        doc = XmlDocHelper(parserFunc());

        XmlUtils::throwUnless(NULL != doc.get());
        if (NULL == doc->children) {
            throw std::runtime_error(STR_ERROR_EMPTY_XML_DOC);
        }

        XmlUtils::throwUnless(xmlXIncludeProcessFlags(doc.get(), XML_PARSE_NOENT) >= 0);

        if (NULL == xmlDocGetRootElement(doc.get())) {
            throw std::runtime_error(STR_ERROR_XML_DOC_WITHOUT_ROOT);
        }
        
        TimeMapType* modified_info = XmlInfoCollector::getModifiedInfo();
        TimeMapType fake;
        modified_info ? swapModifiedInfo(*modified_info) : swapModifiedInfo(fake);
        
        std::string error = XmlInfoCollector::getError();
        if (!error.empty()) {
            throw UnboundRuntimeError(error);
        }
        
        OperationMode::instance()->processXmlError(name());
    }

    data_->doc_ = doc;
    parseXScript();
}

void
Script::parseXScript() {
    std::vector<xmlNodePtr> xscript_nodes;
    data_->parseNode(data_->doc_->children, xscript_nodes);
    data_->parseXScriptNodes(xscript_nodes);
    data_->parseBlocks();
    data_->buildXScriptNodeSet(xscript_nodes);
    postParse();
}

void
Script::parse() {
    namespace fs = boost::filesystem;
    fs::path path(name());
    std::string file_str;

#ifdef BOOST_FILESYSTEM_VERSION
    #if BOOST_FILESYSTEM_VERSION == 3
        file_str = path.native();
    #else
        file_str = path.native_file_string();
    #endif
#else
    file_str = path.native_file_string();
#endif

    if (!fs::exists(path) || fs::is_directory(path)) {
        throw CanNotOpenError(file_str);
    }

    PROFILER(log(), "Script.parse " + file_str);
    XmlCharHelper canonic_path(xmlCanonicPath((const xmlChar*)file_str.c_str()));

    boost::function<xmlDocPtr()> parserFunc =
        boost::bind(&xmlReadFile, (const char*)canonic_path.get(),
            (const char*)NULL, XML_PARSE_DTDATTR | XML_PARSE_NOENT);
    
    parseInternal(parserFunc);
}

void
Script::parseFromXml(const std::string &xml) {
    boost::function<xmlDocPtr()> parserFunc =
        boost::bind(&xmlReadMemory, xml.c_str(), xml.size(), StringUtils::EMPTY_STRING.c_str(),
            (const char*)NULL, XML_PARSE_DTDATTR | XML_PARSE_NOENT);
    
    parseInternal(parserFunc);
}

void
Script::parseFromXml(xmlNodePtr node, const Script *parent) {
    data_->parent_ = parent;
    data_->doc_ = XmlDocHelper(xmlNewDoc((const xmlChar*) "1.0"));
    XmlNodeHelper root_node(xmlCopyNode(node, 1));
    xmlDocSetRootElement(data_->doc_.get(), root_node.get());
    xmlNodeSetBase(root_node.release(), xmlNodeGetBase(node->doc, node));
    parseXScript();
}

std::string
Script::fullName(const std::string &name) const {
    return Xml::fullName(name);
}

const std::string&
Script::xsltName() const {
    return Object::xsltNameRaw();
}

XmlDocSharedHelper
Script::invoke(boost::shared_ptr<Context> ctx) {
    const std::string &script_name = name();
    log()->info("Script::invoke, invoking %s", script_name.c_str());
    PROFILER(log(), "invoke script " + script_name);
    ctx->wait(invokeBlocks(ctx));
    return processResults(ctx);
}

boost::xtime
Script::invokeBlocks(boost::shared_ptr<Context> ctx) {
    unsigned int count = 0;
    boost::xtime end_time = Context::delay(0);
    std::vector<Block*> &blocks = data_->blocks();
    ctx->expect(blocks.size());
    bool stop = false;
    for (std::vector<Block*>::iterator it = blocks.begin();
         it != blocks.end();
         ++it, ++count) {
	const Block *block = *it;
        if (!stop && (ctx->skipNextBlocks() || ctx->stopBlocks())) {
            stop = true;
        }
        if (stop) {
            ctx->result(count, block->fakeResult(true));
            continue;
        }
        try {
            if (!block->checkGuard(ctx.get())) {
                log()->info("Guard skipped block processing. Owner: %s Block: %s. Method: %s",
                    name().c_str(), block->name(), block->method().c_str());
                ctx->result(count, block->fakeResult(false));
                continue;
            }
        }
        catch (const std::exception &e) {
            log()->error("Error while guard processing: %s. Owner: %s Block: %s. Method: %s",
                    e.what(), name().c_str(), block->name(), block->method().c_str());
            ctx->result(count, block->errorResult(e.what(), false));
            continue;
        }

        try {
            if (block->invokeCheckThreadedEx(ctx, count)) {
                const ThreadedBlock *tb = dynamic_cast<const ThreadedBlock*>(block);
                assert(tb);
                boost::xtime tmp = Context::delay(tb->timeout());
                if (boost::xtime_cmp(tmp, end_time) > 0) {
                    end_time = tmp;
                }
            }
            continue;
        }
        catch (const CriticalInvokeError &e) {
            std::string full_error;
            ctx->result(count, block->errorResult(e, full_error));
            OperationMode::instance()->assignBlockError(ctx.get(), block, full_error);
            throw;
        }
        catch (const InvokeError &e) {
            ctx->result(count, block->errorResult(e, false));
        }
        catch (const std::exception &e) {
            ctx->result(count, block->errorResult(e.what(), false));
        }
        ctx->setNoCache();
    }
    return end_time;
}

XmlDocSharedHelper
Script::processResults(boost::shared_ptr<Context> ctx) {
    OperationMode::instance()->processScriptError(ctx.get(), this);
    data_->addHeaders(ctx.get());
    if (ctx->isRoot() && (binaryPage() || ctx->response()->isBinary())) {
        log()->info("suppress fetching block results for binary page");
        return XmlDocSharedHelper();
    }
    return data_->fetchResults(ctx.get());
}

bool
Script::applyStylesheet(boost::shared_ptr<Context> ctx, XmlDocSharedHelper &doc) {    

    const std::string xslt_name = ctx->xsltName();
    boost::shared_ptr<Stylesheet> stylesheet(StylesheetFactory::createStylesheet(xslt_name));

    const std::string &script_name = name();

    PROFILER(log(), "apply stylesheet " + xslt_name + " for " + script_name);
    log()->info("applying stylesheet %s for %s", xslt_name.c_str(), script_name.c_str());
    
    ctx->createDocumentWriter(stylesheet);
    Object::applyStylesheet(stylesheet, ctx, boost::shared_ptr<InvokeContext>(), doc, false);
    
    bool result = !XmlUtils::hasXMLError();	    
    OperationMode::instance()->processMainXsltError(ctx.get(), this, stylesheet.get());
    return result;
}

void
Script::addHeaders(Context *ctx) const {
    data_->addHeaders(ctx);
}

void
Script::postParse() {
}

bool
Script::expireTimeDeltaUndefined() const {
    return data_->expireTimeDeltaUndefined();
}

static const std::string STR_SCHEME_DELIMITER = "://";

std::string
Script::createTagKey(const Context *ctx, const InvokeContext *invoke_ctx) const {
    (void)invoke_ctx;
    CacheStrategy* strategy = cacheStrategy();
    if (NULL == strategy) {
        throw std::logic_error("Cannot cache page without strategy");
    }

    std::string key = ctx->request()->getOriginalUrl();
    std::string::size_type pos = key.find('?');
    if (std::string::npos != pos) {
        key.erase(pos);
    }

    pos = key.find(STR_SCHEME_DELIMITER);
    if (std::string::npos != pos) {
        pos += 2;
    }

    while(std::string::npos != pos) {
        std::string::size_type pos_prev = pos;
        pos = key.find('/', pos + 1);
        if (std::string::npos != pos && pos - pos_prev == 1) {
            key.erase(pos, 1);
            --pos;
        }
    }

    std::string key_strategy = strategy->createKey(ctx);
    std::string key_xslt = fileModifiedKey(xsltName());
    std::string key_common = commonTagKey(ctx);

    key.reserve(key.size() + key_strategy.size() + key_xslt.size() + key_common.size() + 3);

    key.push_back('|');
    key.append(key_strategy);
    key.push_back('|');
    key.append(key_xslt);
    key.push_back('|');
    key.append(key_common);

    return key;
}

std::string
Script::createBlockTagKey(const Context *ctx) const {
    std::string key = name();
    std::string key_common = commonTagKey(ctx);

    key.reserve(key.size() + key_common.size() + 1);
    key.push_back('|');
    key.append(key_common);
    return key;
}

std::string
Script::commonTagKey(const Context *ctx) const {
    (void)ctx;
    std::string key = modifiedKey(modifiedInfo());
    std::string key_blocks = blocksModifiedKey(data_->blocks());

    key.reserve(key.size() + key_blocks.size() + 1);
    key.push_back('|');
    key.append(key_blocks);
    return key;
}

void
Script::xsltName(const char *name) {
    Object::xsltName(name, NULL);
}

std::string
Script::info(const Context *ctx) const {
    std::string info("Url: ");

    std::string url(ctx->request()->getOriginalUrl());
    std::string::size_type pos = url.rfind('?');
    if (std::string::npos != pos) {
        url.erase(pos);
    }
    
    info.append(url);
    info.append(" | Filename: ");
    info.append(name());
    if (!cacheTimeUndefined()) {
        info.append(" | Cache-time: ");
        info.append(boost::lexical_cast<std::string>(cacheTime()));
    }
    
    return info;
}

bool
Script::cachable(const Context *ctx, bool for_save) const {
    return data_->cachable(ctx, for_save);
}

const Script*
Script::parent() const {
    return data_->parent_;
}

bool
Script::valid() const {
    CacheStrategy* strategy = cacheStrategy();
    return strategy ? strategy->valid() : true;
}

class Script::HandlerRegisterer {
public:
    HandlerRegisterer() {
        MessageProcessor::instance()->registerBack(PARSE_XSCRIPT_NODE_METHOD,
            boost::shared_ptr<MessageHandler>(new ParseXScriptNodeHandler()));
        MessageProcessor::instance()->registerBack(REPLACE_XSCRIPT_NODE_METHOD,
            boost::shared_ptr<MessageHandler>(new ReplaceXScriptNodeHandler()));
        MessageProcessor::instance()->registerBack(PROPERTY_METHOD,
            boost::shared_ptr<MessageHandler>(new PropertyHandler()));
        MessageProcessor::instance()->registerBack(CACHABLE_METHOD,
            boost::shared_ptr<MessageHandler>(new CachableHandler()));
    }
};

static Script::HandlerRegisterer reg_script_handlers;

} // namespace xscript
