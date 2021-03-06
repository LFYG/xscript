#include "settings.h"

#include <sstream>

#include <boost/current_function.hpp>

#include <xscript/context.h>
#include <xscript/logger.h>
#include "xscript/message_interface.h"
#include <xscript/param.h>
#include <xscript/script.h>
#include <xscript/script_factory.h>
#include <xscript/typed_map.h>
#include <xscript/xml_util.h>

#include "local_arg_list.h"
#include "local_block.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace xscript {

const std::string LocalBlock::ON_CREATE_BLOCK_METHOD = "ON_CREATE_LOCAL_BLOCK";

LocalBlock::LocalBlock(const Extension *ext, Xml *owner, xmlNodePtr node) :
    Block(ext, owner, node), ThreadedBlock(ext, owner, node), TaggedBlock(ext, owner, node),
    node_id_(XmlUtils::getUniqueNodeId(node)), proxy_flags_(Context::PROXY_NONE) {
}

LocalBlock::~LocalBlock() {
}

void
LocalBlock::propertyInternal(const char *name, const char *value) {
    if (!TaggedBlock::propertyInternal(name, value)) {
        ThreadedBlock::property(name, value);
    }
}

void
LocalBlock::property(const char *name, const char *value) {
    if (strncasecmp(name, "proxy", sizeof("proxy")) == 0) {
        if (!strcasecmp(value, "yes")) {
            proxy_flags_ = Context::PROXY_ALL;
        }
        else if (!strcasecmp(value, "no")) {
            proxy_flags_ = Context::PROXY_NONE;
        }
        else if (!strcasecmp(value, "request")) {
            proxy_flags_ = Context::PROXY_REQUEST;
        }
        else {
            propertyInternal(name, value);
        }
    }
    else {
        propertyInternal(name, value);
    }
}

void
LocalBlock::call(boost::shared_ptr<Context> ctx,
    boost::shared_ptr<InvokeContext> invoke_ctx) const throw (std::exception) {

    if (invoke_ctx->haveCachedCopy()) {
        Tag local_tag = invoke_ctx->tag();
        local_tag.modified = false;
        invoke_ctx->tag(local_tag);
        invoke_ctx->resultDoc(XmlDocHelper());
        return;
    }
    
    const LocalArgList* args = dynamic_cast<const LocalArgList*>(invoke_ctx->getArgList());
    if (NULL == args) {
        throw CriticalInvokeError("Non local arg list");
    }
    boost::shared_ptr<TypedMap> local_params(new TypedMap());
    std::vector<Param*>::const_iterator it = params().begin();
    std::vector<Param*>::const_iterator end = params().end();
    for (unsigned int i = 0, size = args->size(); i < size; ++i) {
        if (end == it) {
            throw CriticalInvokeError("Incorrect param list");
        }
        local_params->insert((*it)->id(), args->typedValue(i));
        ++it;
    }

    boost::shared_ptr<Context> local_ctx =
        Context::createChildContext(script_, ctx, invoke_ctx, local_params, proxy_flags_);

    ContextStopper ctx_stopper(local_ctx);
    
    XmlDocSharedHelper doc = script_->invoke(local_ctx);
    XmlUtils::throwUnless(NULL != doc.get());
    
    if (local_ctx->noCache()) {
        invoke_ctx->resultType(InvokeContext::NO_CACHE);
    }
    
    ctx_stopper.reset();
    invoke_ctx->resultDoc(doc);
    if (tagged()) {
        const Xml::TimeMapType& modified_info = script_->modifiedInfo();
        time_t max_time = 0;
        for (Xml::TimeMapType::const_iterator it = modified_info.begin();
             it != modified_info.end();
             ++it) {
            max_time = std::max(max_time, it->second);
        }
        Tag local_tag(true, max_time, Tag::UNDEFINED_TIME);
        invoke_ctx->tag(local_tag);
    }
}

ArgList*
LocalBlock::createArgList(Context *ctx, InvokeContext *invoke_ctx) const {
    (void)ctx;
    (void)invoke_ctx;
    return new LocalArgList();
}

void
LocalBlock::parseSubNode(xmlNodePtr node) {
    if (NULL == node->name || 0 != xmlStrcasecmp(node->name, (const xmlChar*)"root")) {
        Block::parseSubNode(node);
        return;
    }

    const xmlChar* ref = node->ns ? node->ns->href : NULL;
    if (NULL != ref && 0 != xmlStrcasecmp(ref, (const xmlChar*)XmlUtils::XSCRIPT_NAMESPACE)) {
        Block::parseSubNode(node);
        return;
    }

    xmlAttrPtr name_attr = xmlHasProp(node, (const xmlChar*)"name");
    if (name_attr) {
        const char* value = XmlUtils::value(name_attr);
        if (NULL != value) {
            std::string node_name, prefix;
            const char* ch = strchr(value, ':');
            if (NULL == ch) {
                node_name.assign(value);
            }
            else {
                prefix.assign(value, ch - value);
                node_name.assign(ch + 1);
            }
            if (node_name.empty()) {
                std::stringstream str;
                str << "Empty root node name is not allowed in " << name() << " block";
                throw std::runtime_error(str.str());
            }
            xmlNodeSetName(node, (const xmlChar*)node_name.c_str());
            xmlNsPtr ns = NULL;
            if (!prefix.empty()) {
                const std::map<std::string, std::string> names = namespaces();
                std::map<std::string, std::string>::const_iterator it = names.find(prefix);
                if (names.end() == it) {
                    std::stringstream str;
                    str << "Unknown " << name() << " block namespace: " << prefix;
                    throw std::runtime_error(str.str());
                }
                ns = xmlSearchNsByHref(node->doc, node, (const xmlChar*)it->second.c_str());
                if (NULL == ns) {
                    std::stringstream str;
                    str << "Cannot find " << name() << " block namespace: " << prefix;
                    throw std::runtime_error(str.str());
                }
            }
            xmlSetNs(node, ns);
        }
        xmlRemoveProp(name_attr);
    }

    script_ = ScriptFactory::createScriptFromXmlNode(owner()->name(), node, dynamic_cast<Script*>(owner()));
    Xml::TimeMapType modified_info = owner()->modifiedInfo();
    script_->swapModifiedInfo(modified_info);
}

void
LocalBlock::postParseInternal() {
    if (NULL == script_.get()) {
        std::stringstream str;
        str << "Child script is not specified in " << name() << " block";
        throw std::runtime_error(str.str());
    }
    
    TaggedBlock::postParse();
}

void
LocalBlock::postParse() {
    postParseInternal();
    const std::vector<Param*>& params = this->params();
    for(std::vector<Param*>::const_iterator it = params.begin();
        it != params.end();
        ++it) {
        if ((*it)->id().empty()) {
            throw std::runtime_error("local block param without id");
        }
    }
    createCanonicalMethod("local.");
    notifyCreateBlock();
}

void
LocalBlock::proxy_flags(unsigned int flags) {
    proxy_flags_ = flags;
}

boost::shared_ptr<Script>
LocalBlock::script() const {
    return script_;
}

std::string
LocalBlock::createTagKey(const Context *ctx, const InvokeContext *invoke_ctx) const {
    std::string key(processMainKey(ctx, invoke_ctx));
    key.push_back('|');
    LocalArgList* args = dynamic_cast<LocalArgList*>(invoke_ctx->getArgList());
    if (NULL == args) {
        throw std::runtime_error("Incorrect arg list in local block");
    }
    for (unsigned int i = 0, size = args->size(); i < size; ++i) {
        const TypedValue& value = args->typedValue(i);
        if (i > 0) {
            key.push_back(':');
        }
        std::string res;
        value.serialize(res);
        key.append(res);
    }
    key.push_back('|');
    key.append(script_->name());
    key.push_back('|');
    key.append(modifiedKey(script_->modifiedInfo()));
    key.push_back('|');
    key.append(blocksModifiedKey(script_->blocks()));
    key.push_back('|');
    key.append(node_id_);
    return key;
}

void
LocalBlock::notifyCreateBlock() {

    MessageParam<Script> script_param(script_.get());
    MessageParam<unsigned int> flags_param(&proxy_flags_);

    MessageParamBase* param_list[2];
    param_list[0] = &script_param;
    param_list[1] = &flags_param;

    MessageParams params(2, param_list);
    MessageResultBase result;

    MessageProcessor::instance()->process(ON_CREATE_BLOCK_METHOD, params, result);
}

class OnCreateLocalBlockHandler : public MessageHandler {
    Result process(const MessageParams &params, MessageResultBase &result) {
        (void)params;
	(void)result;
	return CONTINUE;
    }
};
				
class LocalBlockHandlerRegisterer {
public:
    LocalBlockHandlerRegisterer() {
        MessageProcessor::instance()->registerBack(LocalBlock::ON_CREATE_BLOCK_METHOD,
        boost::shared_ptr<MessageHandler>(new OnCreateLocalBlockHandler()));
    }
};
							    
static LocalBlockHandlerRegisterer reg_http_utils_handlers;

} // namespace xscript
