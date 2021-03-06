#include "settings.h"

#include <cassert>

#include <boost/lexical_cast.hpp>

#include <xscript/context.h>
#include <xscript/protocol.h>
#include <xscript/util.h>

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

static std::string getOriginalURI(const xscript::Context *ctx) {
    return ctx->request()->getOriginalURI();
}

static std::string getOriginalUrl(const xscript::Context *ctx) {
    return ctx->request()->getOriginalUrl();
}

static std::string getURI(const xscript::Context *ctx) {
    return ctx->request()->getURI();
}

static std::string getHost(const xscript::Context *ctx) {
    return ctx->request()->getHost();
}

static std::string getOriginalHost(const xscript::Context *ctx) {
    return ctx->request()->getOriginalHost();
}

static std::string getSecure(const xscript::Context *ctx) {
    return ctx->request()->isSecure() ? "yes" : "no";
}

static std::string getContentLength(const xscript::Context *ctx) {
    return boost::lexical_cast<std::string>(ctx->request()->getContentLength());
}

static std::string getBot(const xscript::Context *ctx) {
	return ctx->request()->isBot() ? "yes" : "no";
}

static std::string getPath(const xscript::Context *ctx) {
    return ctx->request()->getScriptName();
}

static std::string getPathInfo(const xscript::Context *ctx) {
    return ctx->request()->getPathInfo();
}

static std::string getRealPath(const xscript::Context *ctx) {
    return ctx->request()->getScriptFilename();
}

static std::string getQuery(const xscript::Context *ctx) {
    return ctx->request()->getQueryString();
}

static std::string getRemoteIP(const xscript::Context *ctx) {
    return ctx->request()->getRealIP();
}

static std::string getMethod(const xscript::Context *ctx) {
    return ctx->request()->getRequestMethod();
}

static std::string getHttpUser(const xscript::Context *ctx) {
    return ctx->request()->getRemoteUser();
}

static std::string getContentEncoding(const xscript::Context *ctx) {
    return ctx->request()->getContentEncoding();
}

static std::string getContentType(const xscript::Context *ctx) {
    return ctx->request()->getContentType();
}

static std::string getServerPort(const xscript::Context *ctx) {
    return boost::lexical_cast<std::string>(ctx->request()->getServerPort());
}

namespace xscript {

typedef boost::function<std::string (const Context*)> ProtocolMethod;
typedef std::map<std::string, ProtocolMethod, StringCILess> MethodMap;
    
static MethodMap methods;

const std::string Protocol::PATH = "path";
const std::string Protocol::PATH_INFO = "pathinfo";
const std::string Protocol::REAL_PATH = "realpath";
const std::string Protocol::ORIGINAL_URI = "originaluri";
const std::string Protocol::ORIGINAL_URL = "originalurl";
const std::string Protocol::QUERY = "query";
const std::string Protocol::REMOTE_IP = "remote_ip";
const std::string Protocol::URI = "uri";
const std::string Protocol::HOST = "host";
const std::string Protocol::ORIGINAL_HOST = "originalhost";
const std::string Protocol::METHOD = "method";
const std::string Protocol::SECURE = "secure";
const std::string Protocol::HTTP_USER = "http_user";
const std::string Protocol::CONTENT_LENGTH = "content-length";
const std::string Protocol::CONTENT_ENCODING = "content-encoding";
const std::string Protocol::CONTENT_TYPE = "content-type";
const std::string Protocol::BOT = "bot";
const std::string Protocol::PORT = "port";

class ProtocolRegistrator {
public:
    ProtocolRegistrator();
};

Protocol::Protocol() {
}

Protocol::~Protocol() {
}

std::string
Protocol::get(const Context *ctx, const char *name) {
    assert(ctx);
    MethodMap::iterator it = methods.find(name);
    if (methods.end() == it) {
        throw std::runtime_error(std::string("Unknown protocol arg: ") + name);
    }

    return it->second(ctx);
}

ProtocolRegistrator::ProtocolRegistrator() {
    methods[Protocol::PATH] = &getPath;
    methods[Protocol::PATH_INFO] = &getPathInfo;
    methods[Protocol::REAL_PATH] = &getRealPath;
    methods[Protocol::ORIGINAL_URI] = &getOriginalURI;
    methods[Protocol::ORIGINAL_URL] = &getOriginalUrl;
    methods[Protocol::QUERY] = &getQuery;
    methods[Protocol::REMOTE_IP] = &getRemoteIP;
    methods[Protocol::URI] = &getURI;
    methods[Protocol::HOST] = &getHost;
    methods[Protocol::ORIGINAL_HOST] = &getOriginalHost;
    methods[Protocol::METHOD] = &getMethod;
    methods[Protocol::SECURE] = &getSecure;
    methods[Protocol::HTTP_USER] = &getHttpUser;
    methods[Protocol::CONTENT_LENGTH] = &getContentLength;
    methods[Protocol::CONTENT_ENCODING] = &getContentEncoding;
    methods[Protocol::CONTENT_TYPE] = &getContentType;
    methods[Protocol::BOT] = &getBot;
    methods[Protocol::PORT] = &getServerPort;
}

static ProtocolRegistrator reg_;

} // namespace xscript
