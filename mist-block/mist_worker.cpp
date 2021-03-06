#include "settings.h"

#include <sys/time.h>
#include <strings.h>

#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>

#include <libxml/xpathInternals.h>

#include "mist_worker.h"
#include "state_node.h"
#include "state_prefix_node.h"

#include <xscript/context.h>
#include <xscript/encoder.h>
#include <xscript/state.h>
#include <xscript/string_utils.h>
#include <xscript/typed_map.h>
#include <xscript/util.h>
#include <xscript/xml_util.h>
#include <xscript/xslt_extension.h>

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace xscript {

MistWorker::MethodMap MistWorker::methods_;

MistWorker::MistWorker(Method method) :
    method_(method)
{}

MistWorker::~MistWorker()
{}

std::auto_ptr<MistWorker>
MistWorker::create(const std::string &method) {
    MethodMap::iterator it = methods_.find(method);
    if (methods_.end() == it) {
        throw std::runtime_error("Unknown mist worker method: " + method);
    }
    return std::auto_ptr<MistWorker>(new MistWorker(it->second));
}

std::auto_ptr<MistWorker>
MistWorker::clone() const {
    return std::auto_ptr<MistWorker>(new MistWorker(method_));
}

bool
MistWorker::isAttachStylesheet() const {
    return method_ == &MistWorker::attachStylesheet;
}

void
MistWorker::attachData(const std::string &data) {
    data_ = data;
}

XmlNodeHelper
MistWorker::run(Context *ctx, const std::vector<std::string> &params) const {
    return (this->*method_)(ctx, params);
}

XmlNodeHelper
MistWorker::run(Context *ctx, const XsltParamFetcher &params) const {
    std::vector<std::string> str_params;
    int size = params.size();
    str_params.reserve(size - 1);
    for (int i = 1; i < size; ++i) {
        str_params.push_back(params.str(i));
    }
    return (this->*method_)(ctx, str_params);
}

XmlNodeHelper
MistWorker::setStateLong(Context *ctx, const std::vector<std::string> &params) const {
    State* state = ctx->state();
    if (2 != params.size()) {
        throw std::invalid_argument("bad arity");
    }
    const std::string &name = params[0];
    state->checkName(name);
    boost::int32_t val = 0;
    try {
        val = boost::lexical_cast<boost::int32_t>(params[1]);
    }
    catch (const boost::bad_lexical_cast &e) {
        val = 0;
    }
    state->setLong(name, val);

    StateNode node("long", name.c_str(), boost::lexical_cast<std::string>(val).c_str());
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::setStateString(Context *ctx, const std::vector<std::string> &params) const {
    State* state = ctx->state();
    if (2 != params.size()) {
        throw std::invalid_argument("bad arity");
    }
    const std::string &name = params[0];
    const std::string &value = params[1];
    state->checkName(name);
    state->setString(name, value);

    StateNode node("string", name.c_str(), XmlUtils::escape(value).c_str());
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::setStateDouble(Context *ctx, const std::vector<std::string> &params) const {
    State* state = ctx->state();
    if (2 != params.size()) {
        throw std::invalid_argument("bad arity");
    }
    const std::string &name = params[0];
    state->checkName(name);
    double val = 0.0;
    try {
        val = boost::lexical_cast<double>(params[1]);
    }
    catch (const boost::bad_lexical_cast &e) {
        val = 0.0;
    }
    state->setDouble(name, val);
    
    StateNode node("double", name.c_str(), boost::lexical_cast<std::string>(val).c_str());
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::setStateLongLong(Context *ctx, const std::vector<std::string> &params) const {
    State* state = ctx->state();
    if (2 != params.size()) {
        throw std::invalid_argument("bad arity");
    }
    const std::string &name = params[0];
    state->checkName(name);
    boost::int64_t val = 0;
    try {
        val = boost::lexical_cast<boost::int64_t>(params[1]);
    }
    catch (const boost::bad_lexical_cast &e) {
        val = 0;
    }
    state->setLongLong(name, val);

    StateNode node("longlong", name.c_str(), boost::lexical_cast<std::string>(val).c_str());
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::setStateRandom(Context *ctx, const std::vector<std::string> &params) const {
    State* state = ctx->state();
    if (3 != params.size()) {
        throw std::invalid_argument("bad arity");
    }
    const std::string &name = params[0];
    state->checkName(name);

    boost::int64_t lo;
    try {
        lo = boost::lexical_cast<boost::int64_t>(params[1]);
    }
    catch (const boost::bad_lexical_cast &e) {
        throw std::runtime_error("bad param: lo");
    }

    boost::int64_t hi;
    try {
        hi = boost::lexical_cast<boost::int64_t>(params[2]);
    }
    catch (const boost::bad_lexical_cast &e) {
        throw std::runtime_error("bad param: hi");
    }

    if (hi <= lo) {
        throw std::runtime_error("bad range");
    }

    boost::int64_t val = lo;
    if (static_cast<boost::int64_t>(RAND_MAX) + 1 < hi - lo) {
        log()->warn("too wide range in mist:set_state_random");
        val += random();
    }
    else if (static_cast<boost::int64_t>(RAND_MAX) + 1 == hi - lo) {
        val += random();
    }
    else {
        val += random() % (hi - lo);
    }

    state->setLongLong(name, val);

    StateNode node("random", name.c_str(), boost::lexical_cast<std::string>(val).c_str());
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::setStateDefined(Context *ctx, const std::vector<std::string> &params) const {
    State* state = ctx->state();
    if (2 != params.size()) {
        throw std::invalid_argument("bad arity");
    }
    const std::string &name = params[0];
    state->checkName(name);

    typedef boost::char_separator<char> Separator;
    typedef boost::tokenizer<Separator> Tokenizer;

    std::string val;
    Tokenizer tok(params[1], Separator(","));
    for (Tokenizer::iterator i = tok.begin(), end = tok.end(); i != end; ++i) {
        if (state->has(*i) && !state->asString(*i).empty()) {
            state->copy(*i, name);
            val = state->asString(name);
            break;
        }
    }

    StateNode node("defined", name.c_str(), XmlUtils::escape(val).c_str());
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::setStateUrlencode(Context *ctx, const std::vector<std::string> &params) const {
    State* state = ctx->state();
    if (2 != params.size() && 3 != params.size()) {
        throw std::invalid_argument("bad arity");
    }
    const std::string &name = params[0];
    state->checkName(name);

    std::string val = params[1];
    if (!val.empty()) {
        if (3 == params.size() && strcasecmp(params[2].c_str(), "utf-8")) {
            std::auto_ptr<Encoder> encoder = Encoder::createEscaping("utf-8", params[2].c_str());
            val = encoder->encode(val);
        }
        val = StringUtils::urlencode(val);
    }

    state->setString(name, val);

    StateNode node("urlencode", name.c_str(), val.c_str());
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::setStateUrldecode(Context *ctx, const std::vector<std::string> &params) const {
    State* state = ctx->state();
    if (2 != params.size() && 3 != params.size()) {
        throw std::invalid_argument("bad arity");
    }
    const std::string &name = params[0];
    state->checkName(name);

    std::string val = StringUtils::urldecode(params[1]);
    if (3 == params.size() && !val.empty() && strcasecmp(params[2].c_str(), "utf-8")) {
        std::auto_ptr<Encoder> encoder = Encoder::createEscaping(params[2].c_str(), "utf-8");
        val = encoder->encode(val);
    }

    state->setString(name, val);

    StateNode node("urldecode", name.c_str(), XmlUtils::escape(val).c_str());
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::setStateXmlescape(Context *ctx, const std::vector<std::string> &params) const {
    State* state = ctx->state();
    if (2 != params.size()) {
        throw std::invalid_argument("bad arity");
    }
    const std::string &name = params[0];

    state->checkName(name);
    std::string val = XmlUtils::escape(params[1]);
    state->setString(name, val);

    StateNode node("xmlescape", name.c_str(), XmlUtils::escape(val).c_str());
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::setStateDomain(Context *ctx, const std::vector<std::string> &params) const {
    State* state = ctx->state();
    if (2 != params.size() && 3 != params.size()) {
        throw std::invalid_argument("bad arity");
    }
    const std::string &name = params[0];
    state->checkName(name);

    boost::int32_t level = 0;
    if (3 == params.size()) {
        try {
            level = boost::lexical_cast<boost::int32_t>(params[2]);
        }
        catch (const boost::bad_lexical_cast &) {
        }
    }

    std::string domain = StringUtils::parseDomainFromURL(params[1], level);
    state->setString(name, domain);

    StateNode node("domain", name.c_str(), XmlUtils::escape(domain).c_str());
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::setStateByKeys(Context *ctx, const std::vector<std::string> &params) const {
    State* state = ctx->state();
    if (4 != params.size()) {
        throw std::invalid_argument("bad arity");
    }
    const std::string &name = params[0];
    state->checkName(name);

    typedef boost::char_separator<char> Separator;
    typedef boost::tokenizer<Separator> Tokenizer;

    Separator sep(",");
    Separator sep_keep(",", NULL, boost::keep_empty_tokens);
    Tokenizer keytok(params[1], sep), valtok(params[2], sep_keep);

    Tokenizer::iterator ki = keytok.begin(), kend = keytok.end();
    Tokenizer::iterator vi = valtok.begin(), vend = valtok.end();

    std::map<std::string, std::string> m;
    for ( ; ki != kend && vi != vend; ++ki, ++vi) {
        m.insert(std::make_pair(*ki, *vi));
    }

    std::string res;
    Tokenizer tok(params[3], sep);
    for (Tokenizer::iterator i = tok.begin(), end = tok.end(); i != end; ++i) {
        std::map<std::string, std::string>::iterator mi = m.find(*i);
        if (m.end() != mi && !mi->second.empty()) {
            res = mi->second;
            state->setString(name, res);
            break;
        }
    }

    StateNode node("keys", name.c_str(), XmlUtils::escape(res).c_str());
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::setStateByDate(Context *ctx, const std::vector<std::string> &params) const {
    State* state = ctx->state();
    if (1 != params.size()) {
        throw std::invalid_argument("bad arity");
    }
    const std::string &name = params[0];
    state->checkName(name);
    
    std::string name_timestamp = name + std::string("_timestamp");
    state->checkName(name_timestamp);

    struct timeval current;
    gettimeofday(&current, 0);

    const char* date_format_iso = "%Y-%m-%d";

    char buf[32];
    struct tm ttm;
    memset(buf, 0, sizeof(buf));

    localtime_r(&current.tv_sec, &ttm);
    strftime(buf, sizeof(buf), date_format_iso, &ttm);

    std::string now_str(buf);
    state->setString(name, now_str);
    std::string timestamp_str = boost::lexical_cast<std::string>(current.tv_sec);
    state->setLongLong(name_timestamp, current.tv_sec);

    StateNode node("date", name.c_str(), now_str.c_str());

    strftime(buf, sizeof(buf), "%z", &ttm);
    node.setProperty("zone", buf);

    strftime(buf, sizeof(buf), "%u", &ttm);
    node.setProperty("weekday", buf);

    node.setProperty("timestamp", timestamp_str.c_str());

    boost::uint64_t current_ms = (boost::uint64_t)current.tv_sec * 1000 +
            (boost::uint64_t)current.tv_usec / 1000;
    timestamp_str = boost::lexical_cast<std::string>(current_ms);
    node.setProperty("timestamp_ms", timestamp_str.c_str());

    current.tv_sec -= 86400; // seconds in one day

    localtime_r(&current.tv_sec, &ttm);
    strftime(buf, sizeof(buf), date_format_iso, &ttm);
    node.setProperty("before", buf);

    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::setStateByQuery(Context *ctx, const std::vector<std::string> &params) const {
    State* state = ctx->state();
    if (2 != params.size()) {
        throw std::invalid_argument("bad arity");
    }
    StateQueryNode node(params[0], state);
    node.build(params[1]);
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::echoQuery(Context *ctx, const std::vector<std::string> &params) const {
    (void)ctx;
    if (2 != params.size()) {
        throw std::invalid_argument("bad arity");
    }
    StateQueryNode node(params[0], NULL);
    node.build(params[1]);
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::setStateByRequest(Context *ctx, const std::vector<std::string> &params) const {
    State* state = ctx->state();
    if (1 != params.size()) {
        throw std::invalid_argument("bad arity");
    }
    StateRequestNode node(params[0], state);
    node.build(ctx->request(), false, NULL);
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::setStateByRequestUrlencoded(Context *ctx, const std::vector<std::string> &params) const {
    State* state = ctx->state();
    if (params.size() < 1 || params.size() > 2) {
        throw std::invalid_argument("bad arity");
    }

    std::auto_ptr<Encoder> encoder(NULL);
    if (2 == params.size()) {
        const std::string &encoding = params[1];
        if (strncasecmp(encoding.c_str(), "utf-8", sizeof("utf-8") - 1) != 0) {
            encoder = std::auto_ptr<Encoder>(Encoder::createEscaping("utf-8", encoding.c_str()));
        }
    }
    StateRequestNode node(params[0], state);
    node.build(ctx->request(), true, encoder.get());
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::echoRequest(Context *ctx, const std::vector<std::string> &params) const {
    if (1 < params.size()) {
        throw std::invalid_argument("bad arity");
    }
    const std::string &prefix = params.empty() ? StringUtils::EMPTY_STRING : params[0];
    StateRequestNode node(prefix, NULL);
    node.build(ctx->request(), false, NULL);
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::setStateByHeaders(Context *ctx, const std::vector<std::string> &params) const {
    State* state = ctx->state();
    if (1 != params.size()) {
        throw std::invalid_argument("bad arity");
    }
    StateHeadersNode node(params[0], state);
    node.build(ctx->request());
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::echoHeaders(Context *ctx, const std::vector<std::string> &params) const {
    if (1 < params.size()) {
        throw std::invalid_argument("bad arity");
    }
    const std::string &prefix = params.empty() ? StringUtils::EMPTY_STRING : params[0];
    StateHeadersNode node(prefix, NULL);
    node.build(ctx->request());
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::setStateByCookies(Context *ctx, const std::vector<std::string> &params) const {
    State* state = ctx->state();
    if (1 != params.size()) {
        throw std::invalid_argument("bad arity");
    }
    StateCookiesNode node(params[0], state);
    node.build(ctx->request());
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::echoCookies(Context *ctx, const std::vector<std::string> &params) const {
    if (1 < params.size()) {
        throw std::invalid_argument("bad arity");
    }
    const std::string &prefix = params.empty() ? StringUtils::EMPTY_STRING : params[0];
    StateCookiesNode node(prefix, NULL);
    node.build(ctx->request());
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::setStateByProtocol(Context *ctx, const std::vector<std::string> &params) const {
    State* state = ctx->state();
    if (1 != params.size()) {
        throw std::invalid_argument("bad arity");
    }
    StateProtocolNode node(params[0], state);
    node.build(ctx);
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::echoProtocol(Context *ctx, const std::vector<std::string> &params) const {
    if (1 < params.size()) {
        throw std::invalid_argument("bad arity");
    }
    const std::string &prefix = params.empty() ? StringUtils::EMPTY_STRING : params[0];
    StateProtocolNode node(prefix, NULL);
    node.build(ctx);
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::setStateByLocalArgs(Context *ctx, const std::vector<std::string> &params) const {
    if (1 != params.size()) {
        throw std::invalid_argument("bad arity");
    }
    StateLocalNode node(params[0], ctx->state());
    node.build(ctx);
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::echoLocalArgs(Context *ctx, const std::vector<std::string> &params) const {
    if (1 < params.size()) {
        throw std::invalid_argument("bad arity");
    }
    const std::string &prefix = params.empty() ? StringUtils::EMPTY_STRING : params[0];
    StateLocalNode node(prefix, NULL);
    node.build(ctx);
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::setStateJoinString(Context *ctx, const std::vector<std::string> &params) const {
    State* state = ctx->state();
    if (3 != params.size()) {
        throw std::invalid_argument("bad arity");
    }
    const std::string &name = params[0];
    state->checkName(name);

    std::vector<std::string> keys;
    
    const std::string &prefix = params[1];

    state->keys(keys);
    std::map<unsigned int, std::string> m;
    for (std::vector<std::string>::iterator i = keys.begin(), end = keys.end(); i != end; ++i) {
        if (i->find(prefix) == 0) {
            std::string num = i->substr(prefix.size(), std::string::npos);
            try {
                unsigned int n = boost::lexical_cast<unsigned int>(num);
                m.insert(std::make_pair(n, state->asString(*i)));
            }
            catch (const boost::bad_lexical_cast &e) {
            }
        }
    }

    std::string val;
    for (std::map<unsigned int, std::string>::iterator i = m.begin(), end = m.end(); i != end; ) {
        val.append(i->second);
        if (end != ++i) {
            val.append(params[2]);
        }
    }
    state->setString(name, val);

    StateNode node("join_string", name.c_str(), XmlUtils::escape(val).c_str());
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::setStateSplitString(Context *ctx, const std::vector<std::string> &params) const {
    State* state = ctx->state();
    if (3 != params.size()) {
        throw std::invalid_argument("bad arity");
    }
    const std::string &prefix = params[0];
    state->checkName(prefix);

    const std::string &val = params[1]; 
    const std::string &delim = params[2];
       
    std::vector<std::string> vals;
    StringUtils::split(val, delim, vals);

    StatePrefixNode node(prefix, "split_string", state);

    unsigned int count = 0;
    for(std::vector<std::string>::iterator it = vals.begin();
        it != vals.end();
        ++it) {
        std::string num = boost::lexical_cast<std::string>(count++);
        state->setString(prefix + num, *it);
        XmlChildNode child(node.getNode(), "part", it->c_str());
        child.setProperty("no", num.c_str());
    }
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::setStateConcatString(Context *ctx, const std::vector<std::string> &params) const {
    State* state = ctx->state();
    unsigned int size = params.size();
    if (size < 3) {
        throw std::invalid_argument("bad arity");
    }
    const std::string &name = params[0];
    state->checkName(name);

    std::string val;
    for(unsigned int i = 1; i < size; ++i) {
        val.append(params[i]);
    }
    
    state->setString(name, val);

    StateNode node("concat_string", name.c_str(), XmlUtils::escape(val).c_str());
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::dropState(Context *ctx, const std::vector<std::string> &params) const {
    State* state = ctx->state();
    if (1 != params.size()) {
        throw std::invalid_argument("bad arity");
    }
    const std::string &prefix = params[0];
    if (prefix.empty()) {
        state->clear();
    }
    else {
        state->erasePrefix(prefix);
    }

    StatePrefixNode node(prefix, "drop", NULL);
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::eraseState(Context *ctx, const std::vector<std::string> &params) const {
    State* state = ctx->state();
    if (1 != params.size()) {
        throw std::invalid_argument("bad arity");
    }
    const std::string &key = params[0];
    if (!key.empty()) {
        state->erase(key);
    }
    StateNode node("erase", key.c_str(), StringUtils::EMPTY_STRING.c_str());
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::dumpState(Context *ctx, const std::vector<std::string> &params) const {
    if (1 < params.size()) {
        throw std::invalid_argument("bad arity");
    }
    
    const std::string &prefix = params.empty() ? StringUtils::EMPTY_STRING : params[0];

    XmlNode node("state_dump");

    std::map<std::string, TypedValue> state_info;
    if (prefix.empty()) {
        ctx->state()->values(state_info);
    }
    else {
        ctx->state()->values(prefix, state_info);
    }
    
    for (std::map<std::string, TypedValue>::const_iterator it = state_info.begin();
        it != state_info.end();
        ++it) {
        XmlTypedVisitor visitor;
        it->second.visit(&visitor);
        XmlNodeHelper result = visitor.result();
        xmlNewProp(result.get(), (const xmlChar*)"name", (const xmlChar*)it->first.c_str());
        xmlAddChild(node.getNode(), result.release());
    }
    
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::attachStylesheet(Context *ctx, const std::vector<std::string> &params) const {
    if (1 != params.size()) {
        throw std::invalid_argument("bad arity");
    }

    ctx->rootContext()->xsltName(data_);

    XmlNode node("stylesheet");
    node.setType("attach");
    node.setContent(data_.c_str());
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::location(Context *ctx, const std::vector<std::string> &params) const {
    if (1 != params.size()) {
        throw std::invalid_argument("bad arity");
    }
    
    const std::string &location = params[0];
    ctx->response()->setStatus(302);
    ctx->response()->setHeader("Location", location);

    XmlNode node("location");
    node.setContent(XmlUtils::escape(location).c_str());
    return XmlNodeHelper(node.releaseNode());
}

XmlNodeHelper
MistWorker::setStatus(Context *ctx, const std::vector<std::string> &params) const {
    if (1 != params.size()) {
        throw std::invalid_argument("bad arity");
    }

    const std::string &val = params[0];
    boost::int32_t status = 0;
    try {
        status = boost::lexical_cast<boost::int32_t>(val);
    }
    catch (const boost::bad_lexical_cast &e) {
        throw std::runtime_error("unknown status: " + val);
    }

    ctx->response()->setStatus(status);

    XmlNode node("status");
    node.setContent(val.c_str());
    return XmlNodeHelper(node.releaseNode());
}

void
MistWorker::registerMethod(const std::string &name, MistWorker::Method method) {
    methods_[name] = method;
}

class MistWorkerMethodRegistrator {
public:
    MistWorkerMethodRegistrator();
};

MistWorkerMethodRegistrator::MistWorkerMethodRegistrator() {
    MistWorker::registerMethod("setStateLong", &MistWorker::setStateLong);
    MistWorker::registerMethod("set_state_long", &MistWorker::setStateLong);

    MistWorker::registerMethod("setStateString", &MistWorker::setStateString);
    MistWorker::registerMethod("set_state_string", &MistWorker::setStateString);

    MistWorker::registerMethod("setStateDouble", &MistWorker::setStateDouble);
    MistWorker::registerMethod("set_state_double", &MistWorker::setStateDouble);

    MistWorker::registerMethod("setStateLonglong", &MistWorker::setStateLongLong);
    MistWorker::registerMethod("set_state_longlong", &MistWorker::setStateLongLong);

    MistWorker::registerMethod("setStateRandom", &MistWorker::setStateRandom);
    MistWorker::registerMethod("set_state_random", &MistWorker::setStateRandom);

    MistWorker::registerMethod("setStateDefined", &MistWorker::setStateDefined);
    MistWorker::registerMethod("set_state_defined", &MistWorker::setStateDefined);

    MistWorker::registerMethod("set_state_urlencode", &MistWorker::setStateUrlencode);
    MistWorker::registerMethod("setStateUrlencode", &MistWorker::setStateUrlencode);

    MistWorker::registerMethod("set_state_urldecode", &MistWorker::setStateUrldecode);
    MistWorker::registerMethod("setStateUrldecode", &MistWorker::setStateUrldecode);

    MistWorker::registerMethod("set_state_xmlescape", &MistWorker::setStateXmlescape);
    MistWorker::registerMethod("setStateXmlescape", &MistWorker::setStateXmlescape);
    
    MistWorker::registerMethod("setStateDomain", &MistWorker::setStateDomain);
    MistWorker::registerMethod("set_state_domain", &MistWorker::setStateDomain);

    MistWorker::registerMethod("setStateByKey", &MistWorker::setStateByKeys);
    MistWorker::registerMethod("set_state_by_key", &MistWorker::setStateByKeys);

    MistWorker::registerMethod("setStateByKeys", &MistWorker::setStateByKeys);
    MistWorker::registerMethod("set_state_by_keys", &MistWorker::setStateByKeys);

    MistWorker::registerMethod("setStateByDate", &MistWorker::setStateByDate);
    MistWorker::registerMethod("set_state_by_date", &MistWorker::setStateByDate);

    MistWorker::registerMethod("setStateByQuery", &MistWorker::setStateByQuery);
    MistWorker::registerMethod("set_state_by_query", &MistWorker::setStateByQuery);

    MistWorker::registerMethod("setStateByRequest", &MistWorker::setStateByRequest);
    MistWorker::registerMethod("set_state_by_request", &MistWorker::setStateByRequest);

    MistWorker::registerMethod("setStateByRequestUrlencoded", &MistWorker::setStateByRequestUrlencoded);
    MistWorker::registerMethod("set_state_by_request_urlencoded", &MistWorker::setStateByRequestUrlencoded);

    MistWorker::registerMethod("setStateByHeaders", &MistWorker::setStateByHeaders);
    MistWorker::registerMethod("set_state_by_headers", &MistWorker::setStateByHeaders);

    MistWorker::registerMethod("setStateByCookies", &MistWorker::setStateByCookies);
    MistWorker::registerMethod("set_state_by_cookies", &MistWorker::setStateByCookies);

    MistWorker::registerMethod("setStateByProtocol", &MistWorker::setStateByProtocol);
    MistWorker::registerMethod("set_state_by_protocol", &MistWorker::setStateByProtocol);
    
    MistWorker::registerMethod("setStateByLocalArgs", &MistWorker::setStateByLocalArgs);
    MistWorker::registerMethod("set_state_by_local_args", &MistWorker::setStateByLocalArgs);

    MistWorker::registerMethod("echoQuery", &MistWorker::echoQuery);
    MistWorker::registerMethod("echo_query", &MistWorker::echoQuery);

    MistWorker::registerMethod("echoRequest", &MistWorker::echoRequest);
    MistWorker::registerMethod("echo_request", &MistWorker::echoRequest);

    MistWorker::registerMethod("echoHeaders", &MistWorker::echoHeaders);
    MistWorker::registerMethod("echo_headers", &MistWorker::echoHeaders);

    MistWorker::registerMethod("echoCookies", &MistWorker::echoCookies);
    MistWorker::registerMethod("echo_cookies", &MistWorker::echoCookies);

    MistWorker::registerMethod("echoProtocol", &MistWorker::echoProtocol);
    MistWorker::registerMethod("echo_protocol", &MistWorker::echoProtocol);
    
    MistWorker::registerMethod("echoLocalArgs", &MistWorker::echoLocalArgs);
    MistWorker::registerMethod("echo_local_args", &MistWorker::echoLocalArgs);

    MistWorker::registerMethod("setStateJoinString", &MistWorker::setStateJoinString);
    MistWorker::registerMethod("set_state_join_string", &MistWorker::setStateJoinString);

    MistWorker::registerMethod("setStateSplitString", &MistWorker::setStateSplitString);
    MistWorker::registerMethod("set_state_split_string", &MistWorker::setStateSplitString);

    MistWorker::registerMethod("setStateConcatString", &MistWorker::setStateConcatString);
    MistWorker::registerMethod("set_state_concat_string", &MistWorker::setStateConcatString);

    MistWorker::registerMethod("dropState", &MistWorker::dropState);
    MistWorker::registerMethod("drop_state", &MistWorker::dropState);

    MistWorker::registerMethod("eraseState", &MistWorker::eraseState);
    MistWorker::registerMethod("erase_state", &MistWorker::eraseState);

    MistWorker::registerMethod("dumpState", &MistWorker::dumpState);
    MistWorker::registerMethod("dump_state", &MistWorker::dumpState);
    
    MistWorker::registerMethod("attachStylesheet", &MistWorker::attachStylesheet);
    MistWorker::registerMethod("attach_stylesheet", &MistWorker::attachStylesheet);

    MistWorker::registerMethod("location", &MistWorker::location);

    MistWorker::registerMethod("setStatus", &MistWorker::setStatus);
    MistWorker::registerMethod("set_status", &MistWorker::setStatus);
}

static MistWorkerMethodRegistrator reg_;

} // namespace xscript
