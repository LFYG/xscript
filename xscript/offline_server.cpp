#include "settings.h"

#include "offline_request.h"
#include "offline_server.h"
#include "xslt_profiler.h"

#include "xscript/config.h"
#include "xscript/context.h"
#include "xscript/exception.h"
#include "xscript/operation_mode.h"
#include "xscript/request_data.h"
#include "xscript/script.h"
#include "xscript/script_factory.h"
#include "xscript/state.h"
#include "xscript/xml_util.h"

#include <boost/shared_ptr.hpp>
#include <boost/tokenizer.hpp>

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace xscript {

OfflineServer::OfflineServer(Config *config) : Server(config) {
    root_ = config->as<std::string>("/xscript/offline/root-dir", "/usr/local/www");
    config->stopCollectCache();
}

OfflineServer::~OfflineServer() {
}

bool
OfflineServer::useXsltProfiler() const {
    return false;
}

std::string
OfflineServer::renderBuffer(const std::string &url,
                            const std::string &xml,
                            const std::string &body,
                            const std::string &headers,
                            const std::string &vars) {
    XmlUtils::registerReporters();

    std::vector<std::string> header_list, var_list;
    
    typedef boost::char_separator<char> Separator;
    typedef boost::tokenizer<Separator> Tokenizer;
    
    if (!headers.empty()) {
        Tokenizer tok(headers, Separator("\n\r"));
        for (Tokenizer::iterator it = tok.begin(), it_end = tok.end(); it != it_end; ++it) {
            header_list.push_back(*it);
        }
    }
    
    if (!vars.empty()) {
        Tokenizer tok(vars, Separator("\n\r"));
        for (Tokenizer::iterator it = tok.begin(), it_end = tok.end(); it != it_end; ++it) {
            var_list.push_back(*it);
        }
    }
    
    std::stringstream buffer;

    boost::shared_ptr<Context> ctx;
    boost::shared_ptr<Request> request(new OfflineRequest(root_));
    boost::shared_ptr<Response> response(new OfflineResponse(&buffer, &buffer, true));
    ResponseDetacher response_detacher(response.get(), ctx);
    
    OfflineRequest* offline_request = dynamic_cast<OfflineRequest*>(request.get());

    try {
        offline_request->attach(url, xml, body, header_list, var_list);        

        handleRequest(request, response, ctx);
    }
    catch (const BadRequestError &e) {
        OperationMode::instance()->sendError(response.get(), 400, e.what());
    }
    catch (const std::exception &e) {
        OperationMode::instance()->sendError(response.get(), 500, e.what());
    }
    
    return buffer.str();
}

std::string
OfflineServer::renderFile(const std::string &file,
                          const std::string &body,
                          const std::string &headers,
                          const std::string &vars) {
    return renderBuffer(file, StringUtils::EMPTY_STRING, body, headers, vars);
}

boost::shared_ptr<Script>
OfflineServer::getScript(Request *request) {
    
    OfflineRequest *offline_request = dynamic_cast<OfflineRequest*>(request);
    if (NULL == offline_request) {
        throw std::logic_error("Conflict: NULL or not a OfflineRequest in OfflineServer");
    }
    
    const std::string& xml = offline_request->xml();
    if (xml.empty()) {
        return Server::getScript(request);
    }
    
    return ScriptFactory::createScriptFromXml(request->getScriptFilename(), xml);
}

}
