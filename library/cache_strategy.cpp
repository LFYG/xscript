#include "settings.h"

#include <set>
#include <string>
#include <stdexcept>

#include <boost/tokenizer.hpp>

#include "xscript/cache_strategy.h"
#include "xscript/cache_strategy_collector.h"
#include "xscript/config.h"
#include "xscript/context.h"
#include "xscript/doc_cache_strategy.h"
#include "xscript/policy.h"
#include "xscript/string_utils.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace xscript {

SubCacheStrategy::SubCacheStrategy() {
}

SubCacheStrategy::~SubCacheStrategy() {
}

void
SubCacheStrategy::initContext(Context *ctx) {
    (void)ctx;
}

std::string
SubCacheStrategy::createKey(const Context *ctx) {
    (void)ctx;
    return StringUtils::EMPTY_STRING;
}

bool
SubCacheStrategy::noCache(const Context *ctx) const {
    (void)ctx;
    return false;
}

std::string
SubCacheStrategy::key() const {
    return StringUtils::EMPTY_STRING;
}

SubCacheStrategyFactory::SubCacheStrategyFactory()
{}

SubCacheStrategyFactory::~SubCacheStrategyFactory()
{}

CacheStrategy::CacheStrategy() : valid_(true)
{
}

CacheStrategy::~CacheStrategy() {
}

void
CacheStrategy::add(std::auto_ptr<SubCacheStrategy> substrategy) {
    boost::shared_ptr<SubCacheStrategy> sub_str(substrategy.release());
    substrategies_.push_back(sub_str);
    key_.append(sub_str->key());
}

void
CacheStrategy::initContext(Context *ctx) {
    for(std::vector<boost::shared_ptr<SubCacheStrategy> >::const_iterator it = substrategies_.begin();
        it != substrategies_.end();
        ++it) {
        if ((*it)->noCache(ctx)) {
            ctx->rootContext()->setNoCache();
        }
        (*it)->initContext(ctx);
    }
}

std::string
CacheStrategy::createKey(const Context *ctx) const {
    std::string result(key());
    for(std::vector<boost::shared_ptr<SubCacheStrategy> >::const_iterator it = substrategies_.begin();
        it != substrategies_.end();
        ++it) {
        result.push_back('|');
        result.append((*it)->createKey(ctx));
    }
    return result;
}

bool
CacheStrategy::noCache(const Context *ctx) const {
    for(std::vector<boost::shared_ptr<SubCacheStrategy> >::const_iterator it = substrategies_.begin();
        it != substrategies_.end();
        ++it) {
        if ((*it)->noCache(ctx)) {
            return true;
        }
    }
    return false;
}

const std::string&
CacheStrategy::key() const {
    return key_;
}

bool
CacheStrategy::valid() const {
    boost::mutex::scoped_lock lock(mutex_);
    return valid_;
}

void
CacheStrategy::valid(bool flag) {
    boost::mutex::scoped_lock lock(mutex_);
    valid_ = flag;
}

class QuerySubCacheStrategy : public SubCacheStrategy {
public:
    QuerySubCacheStrategy();
    virtual std::string createKey(const Context *ctx);
    virtual std::string key() const;
    
    friend class QuerySubCacheStrategyFactory;
private:
    typedef std::vector<StringUtils::NamedValue>::const_iterator arg_iterator;
    struct ArgLess : public std::binary_function<const arg_iterator&, const arg_iterator&, bool> {
        bool operator() (const arg_iterator &arg1, const arg_iterator &arg2) const {
            return *arg1 < *arg2;
        }
    };

private:
    bool cacheableArg(const std::string &name) const;
    
private:
    std::set<std::string> cache_args_;
    bool sort_;
    bool except_;
};

class QuerySubCacheStrategyFactory : public SubCacheStrategyFactory {
    std::auto_ptr<SubCacheStrategy> create(const Config *config, const std::string &path) {
        
        QuerySubCacheStrategy* query_strategy = new QuerySubCacheStrategy();
        std::auto_ptr<SubCacheStrategy> strategy(query_strategy);
        std::string value;
        try {
            value = config->as<std::string>(path);
        }
        catch(const std::exception &e) {
            return std::auto_ptr<SubCacheStrategy>();
        }

        std::string sort_value = config->as<std::string>(path + "/@sort", "yes");
        if (0 == strcasecmp(sort_value.c_str(), "yes")) {
        }
        else if (0 == strcasecmp(sort_value.c_str(), "no")) {
            query_strategy->sort_ = false;
        }
        else {
            throw std::runtime_error(
                "incorrect value for sort attribute in query cache strategy: " + sort_value);
        }
        
        std::string except_value = config->as<std::string>(path + "/@except", "no");
        if (0 == strcasecmp(except_value.c_str(), "yes")) {
            if (value.empty()) {
                throw std::runtime_error(
                    "Except query arg list should be specified in query cache strategy");
            }
            query_strategy->except_ = true;
        }
        else if (0 == strcasecmp(except_value.c_str(), "no")) {
        }
        else {
            throw std::runtime_error(
                "incorrect value for except attribute in query cache strategy: " + except_value);
        }

        typedef boost::char_separator<char> Separator;
        typedef boost::tokenizer<Separator> Tokenizer;
        Tokenizer tok(value, Separator(", "));
        for (Tokenizer::iterator it = tok.begin(), it_end = tok.end(); it != it_end; ++it) {
            query_strategy->cache_args_.insert(*it);
        }
        
        return strategy;
    }
};

QuerySubCacheStrategy::QuerySubCacheStrategy() : sort_(true), except_(false)
{}

bool
QuerySubCacheStrategy::cacheableArg(const std::string &name) const {
    if (cache_args_.empty()) {
        return true;
    }
    return except_ ^ (cache_args_.end() != cache_args_.find(name));
}

std::string
QuerySubCacheStrategy::createKey(const Context *ctx) {
    std::vector<arg_iterator> keys;    
    const std::vector<StringUtils::NamedValue>& args = ctx->request()->args();
    for(std::vector<StringUtils::NamedValue>::const_iterator it = args.begin(), end = args.end();
        it != end;
        ++it) {
        const std::string& name = it->first;
        if (name.empty()) {
            continue;
        }
        
        if (cacheableArg(name)) {
            keys.push_back(it);
        }
    }

    if (sort_) {
        std::sort(keys.begin(), keys.end(), ArgLess());
    }

    std::string key;
    for(std::vector<arg_iterator>::const_iterator it = keys.begin(), end = keys.end();
        it != end;
        ++it) {
        if (!key.empty()) {
            key.push_back('&');
        }
        key.append((*it)->first);
        key.push_back('=');
        key.append((*it)->second);
    }
    
    return key;
}

std::string
QuerySubCacheStrategy::key() const {
    std::string key;
    for (std::set<std::string>::const_iterator it = cache_args_.begin();
         it != cache_args_.end();
         ++it) {
        key.append(*it);
    }
    key.append(boost::lexical_cast<std::string>(sort_));
    key.append(boost::lexical_cast<std::string>(except_));
    return key;
}

class CookieSubCacheStrategy : public SubCacheStrategy {
public:
    virtual std::string createKey(const Context *ctx);
    virtual std::string key() const;
    
    friend class CookieSubCacheStrategyFactory;
private:
    std::set<std::string> cache_cookies_;
};

class CookieSubCacheStrategyFactory : public SubCacheStrategyFactory {
    std::auto_ptr<SubCacheStrategy> create(const Config *config, const std::string &path) {
               
        std::string value;
        try {
            value = config->as<std::string>(path);
        }
        catch(const std::exception &e) {
            return std::auto_ptr<SubCacheStrategy>();
        }
        
        CookieSubCacheStrategy* cookie_strategy = new CookieSubCacheStrategy();
        std::auto_ptr<SubCacheStrategy> strategy(cookie_strategy);
        
        typedef boost::char_separator<char> Separator;
        typedef boost::tokenizer<Separator> Tokenizer;
        Tokenizer tok(value, Separator(", "));
        for (Tokenizer::iterator it = tok.begin(), it_end = tok.end(); it != it_end; ++it) {
            if (!Policy::instance()->allowCachingInputCookie(it->c_str())) {
                std::stringstream ss;
                ss << "Cookie " << *it << " is not allowed in cookie strategy";
                throw std::runtime_error(ss.str());
            }
            cookie_strategy->cache_cookies_.insert(*it);
        }
        
        return strategy;
    }
};

std::string
CookieSubCacheStrategy::createKey(const Context *ctx) {
    std::string key;
    bool is_first = true;
    for (std::set<std::string>::iterator it = cache_cookies_.begin();
         it != cache_cookies_.end();
         ++it) {
        const std::string& cookie = ctx->request()->getCookie(*it);
        if (cookie.empty()) {
            continue;
        }

        if (is_first) {
            is_first = false;
        }
        else {
            key.push_back('|');
        }
        key.append(*it);
        key.push_back(':');
        key.append(cookie);
    }
    return key; 
}

std::string
CookieSubCacheStrategy::key() const {
    std::string key;
    for (std::set<std::string>::const_iterator it = cache_cookies_.begin();
         it != cache_cookies_.end();
         ++it) {
        key.append(*it);
    }
    return key;
}

class CacheStrategyHandlersRegisterer {
public:
    CacheStrategyHandlersRegisterer() {
        CacheStrategyCollector::instance()->addPageStrategyHandler(
            "query", boost::shared_ptr<SubCacheStrategyFactory>(new QuerySubCacheStrategyFactory()));
        CacheStrategyCollector::instance()->addPageStrategyHandler(
            "cookie", boost::shared_ptr<SubCacheStrategyFactory>(new CookieSubCacheStrategyFactory()));
    }
};

static CacheStrategyHandlersRegisterer reg_;

} // namespace xscript
