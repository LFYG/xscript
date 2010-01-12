#include "settings.h"

#include <sstream>

#include "xscript/context.h"
#include "xscript/logger.h"
#include "xscript/message_interface.h"
#include "xscript/operation_mode.h"
#include "xscript/remote_tagged_block.h"
#include "xscript/request.h"
#include "xscript/response.h"
#include "xscript/script.h"
#include "xscript/stylesheet.h"
#include "xscript/util.h"
#include "xscript/vhost_data.h"
#include "xscript/xml_util.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace xscript {
namespace DevelopmentModeHandlers {

class ProcessErrorHandler : public MessageHandler {
    Result process(const MessageParams &params, MessageResultBase &result) {
        (void)result;
        const std::string& message = params.get<const std::string>(0);
        log()->error("%s", message.c_str());
        throw UnboundRuntimeError(message);
    }
};

class ProcessCriticalInvokeErrorHandler : public MessageHandler {
    Result process(const MessageParams &params, MessageResultBase &result) {
        (void)result;
        const std::string& message = params.get<const std::string>(0);
        throw CriticalInvokeError(message);
    }
};

class SendErrorHandler : public MessageHandler {
    Result process(const MessageParams &params, MessageResultBase &result) {
        (void)result;
        Response* response = params.getPtr<Response>(0);
        unsigned short status = params.get<unsigned short>(1);
        const std::string& message = params.get<const std::string>(2);
        response->sendError(status, message);
        return BREAK;
    }
};

class IsProductionHandler : public MessageHandler {
    Result process(const MessageParams &params, MessageResultBase &result) {
        (void)params;
        result.set(false);
        return BREAK;
    }
};

class AssignBlockErrorHandler : public MessageHandler {
    Result process(const MessageParams &params, MessageResultBase &result) {
        (void)result;
        Context* ctx = params.getPtr<Context>(0);
        const Block* block = params.getPtr<const Block>(1);
        const std::string& error = params.get<const std::string>(2);
        ctx->assignRuntimeError(block, error);
        return BREAK;
    }
};

class ProcessPerblockXsltErrorHandler : public MessageHandler {
    Result process(const MessageParams &params, MessageResultBase &result) {
        (void)result;
        const Context* ctx = params.getPtr<const Context>(0);
        const Block* block = params.getPtr<const Block>(1);
        std::string res = ctx->getRuntimeError(block);
        if (!res.empty()) {
            throw CriticalInvokeError(res, "xslt", block->xsltName());
        }
        if (XmlUtils::hasXMLError()) {
            std::string error = XmlUtils::getXMLError();
            if (!error.empty()) {
                throw InvokeError(error, "xslt", block->xsltName());
            }
        }
        return BREAK;
    }
};

class ProcessScriptErrorHandler : public MessageHandler {
    Result process(const MessageParams &params, MessageResultBase &result) {
        (void)result;
        
        const Context* ctx = params.getPtr<const Context>(0);
        const Script* script = params.getPtr<const Script>(1);
        
        std::string res;
        unsigned int size = script->blocksNumber();
        for (unsigned int i = 0; i < size; ++i) {
            std::string error = ctx->getRuntimeError(script->block(i));
            if (!error.empty()) {
                res.append(error);
                res.push_back(' ');
            }
        }
        
        if (!res.empty()) {
            throw InvokeError(res.c_str());
        }
        
        return BREAK;
    }
};

class ProcessMainXsltErrorHandler : public MessageHandler {
    Result process(const MessageParams &params, MessageResultBase &result) {
        (void)result;
        
        const Context* ctx = params.getPtr<const Context>(0);
        const Script* script = params.getPtr<const Script>(1);
        const Stylesheet* style = params.getPtr<const Stylesheet>(2);
        
        std::string res = ctx->getRuntimeError(NULL);
        if (!res.empty()) {            
            std::stringstream stream;
            stream << res << ". Script: " << script->name() << ". Main stylesheet: " << style->name();
            throw InvokeError(stream.str());
        }
        if (XmlUtils::hasXMLError()) {
            std::string error = XmlUtils::getXMLError();
            if (!error.empty()) {
                std::stringstream stream;
                stream << error << ". Script: " << script->name() << ". Main stylesheet: " << style->name();
                throw InvokeError(stream.str());
            }
        }
        
        return BREAK;
    }
};

class ProcessXmlErrorHandler : public MessageHandler {
    Result process(const MessageParams &params, MessageResultBase &result) {
        (void)result;
        if (XmlUtils::hasXMLError()) {
            std::string error = XmlUtils::getXMLError();
            if (!error.empty()) {
                const std::string& filename = params.get<const std::string>(0);
                std::stringstream stream;
                stream << error << ". File: " << filename;
                throw UnboundRuntimeError(error);
            }
        }
        
        return BREAK;
    }
};

class CollectErrorHandler : public MessageHandler {
    Result process(const MessageParams &params, MessageResultBase &result) {
        (void)result;
        const InvokeError& error = params.get<const InvokeError>(0);
        InvokeError& full_error = params.get<InvokeError>(1);
        
        const InvokeError::InfoMapType& info = error.info();
        for(InvokeError::InfoMapType::const_iterator it = info.begin();
            it != info.end();
            ++it) {
            full_error.add(it->first, it->second);        
        } 
 
        return BREAK;
    }
};

class CheckDevelopmentVariableHandler : public MessageHandler {
    Result process(const MessageParams &params, MessageResultBase &result) {
        const Request* request = params.getPtr<const Request>(0);
        const std::string& var = params.get<const std::string>(1);
        result.set(VirtualHostData::instance()->checkVariable(request, var));
        return BREAK;
    }
};

class CheckRemoteTimeoutHandler : public MessageHandler {
    Result process(const MessageParams &params, MessageResultBase &result) {
        (void)result;
        RemoteTaggedBlock* block = params.getPtr<RemoteTaggedBlock>(0);
        if (block->retryCount() == 0 &&
            !block->tagged() &&
            !block->isDefaultRemoteTimeout()) {
            
            throw std::runtime_error("remote timeout setup is prohibited for non-tagged blocks or when tag cache time is nil");
        }
        return BREAK;
    }
};

struct HandlerRegisterer {
    HandlerRegisterer() {
        MessageProcessor::instance()->registerFront(OperationMode::PROCESS_ERROR_METHOD,
                boost::shared_ptr<MessageHandler>(new ProcessErrorHandler()));
        MessageProcessor::instance()->registerFront(OperationMode::PROCESS_CRITICAL_INVOKE_ERROR_METHOD,
                boost::shared_ptr<MessageHandler>(new ProcessCriticalInvokeErrorHandler()));
        MessageProcessor::instance()->registerFront(OperationMode::SEND_ERROR_METHOD,
                boost::shared_ptr<MessageHandler>(new SendErrorHandler()));
        MessageProcessor::instance()->registerFront(OperationMode::IS_PRODUCTION_METHOD,
                boost::shared_ptr<MessageHandler>(new IsProductionHandler()));
        MessageProcessor::instance()->registerFront(OperationMode::ASSIGN_BLOCK_ERROR_METHOD,
                boost::shared_ptr<MessageHandler>(new AssignBlockErrorHandler()));
        MessageProcessor::instance()->registerFront(OperationMode::PROCESS_PERBLOCK_XSLT_ERROR_METHOD,
                boost::shared_ptr<MessageHandler>(new ProcessPerblockXsltErrorHandler()));
        MessageProcessor::instance()->registerFront(OperationMode::PROCESS_SCRIPT_ERROR_METHOD,
                boost::shared_ptr<MessageHandler>(new ProcessScriptErrorHandler()));
        MessageProcessor::instance()->registerFront(OperationMode::PROCESS_MAIN_XSLT_ERROR_METHOD,
                boost::shared_ptr<MessageHandler>(new ProcessMainXsltErrorHandler()));
        MessageProcessor::instance()->registerFront(OperationMode::PROCESS_XML_ERROR_METHOD,
                boost::shared_ptr<MessageHandler>(new ProcessXmlErrorHandler()));
        MessageProcessor::instance()->registerFront(OperationMode::COLLECT_ERROR_METHOD,
                boost::shared_ptr<MessageHandler>(new CollectErrorHandler()));
        MessageProcessor::instance()->registerFront(OperationMode::CHECK_DEVELOPMENT_VARIABLE_METHOD,
                boost::shared_ptr<MessageHandler>(new CheckDevelopmentVariableHandler()));
        MessageProcessor::instance()->registerFront(OperationMode::CHECK_REMOTE_TIMEOUT_METHOD,
                boost::shared_ptr<MessageHandler>(new CheckRemoteTimeoutHandler()));
    }
};

static HandlerRegisterer reg_handlers;

} // namespace DevelopmentModeHandlers
} // namespace xscript
