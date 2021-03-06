#ifndef _XSCRIPT_SERVER_H_
#define _XSCRIPT_SERVER_H_

#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>
#include <string>

#include "xscript/request.h"
#include "xscript/response.h"
#include "xscript/state.h"
#include "xscript/xml_helpers.h"

namespace xscript {

class Config;
class Context;
class Script;

class Server : private boost::noncopyable {
public:
    Server(Config *config);
    virtual ~Server();

    virtual bool needApplyMainStylesheet(Request *request) const;
    virtual bool needApplyPerblockStylesheet(Request *request) const;
    virtual bool useXsltProfiler() const = 0;
    const std::string& hostname() const;
    Config* config() const;

protected:
    void handleRequest(const boost::shared_ptr<Request> &request,
                       const boost::shared_ptr<Response> &response,
                       boost::shared_ptr<Context> &ctx);    
    bool processCachedDoc(Context *ctx, Script *script);
    void sendResponse(Context *ctx, XmlDocSharedHelper doc);
                               
    virtual boost::shared_ptr<Script> getScript(Request *request);
    static std::pair<std::string, bool> findScript(const std::string &name);
    void addHeaders(Context *ctx);
    void sendHeaders(Context *ctx);

    bool isAlternatePort(unsigned short port) const;
    bool isNoXsltPort(unsigned short port) const;

    virtual Context* createContext(const boost::shared_ptr<Script> &script,
                                   const boost::shared_ptr<State> &state,
                                   const boost::shared_ptr<Request> &request,
                                   const boost::shared_ptr<Response> &response);
private:
    class ServerData;
    std::auto_ptr<ServerData> data_;
};

} // namespace xscript

#endif // _XSCRIPT_SERVER_H_
