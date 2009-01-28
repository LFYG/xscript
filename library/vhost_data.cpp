#include "settings.h"
#include "xscript/vhost_data.h"
#include "xscript/util.h"
#include "xscript/string_utils.h"

#include "xscript/logger.h"

#include <boost/lexical_cast.hpp>

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace xscript {

const std::string VirtualHostData::DOCUMENT_ROOT = "DOCUMENT_ROOT";

REGISTER_COMPONENT(VirtualHostData);

VirtualHostData::VirtualHostData() : server_(NULL) {
}

VirtualHostData::~VirtualHostData() {
}

void
VirtualHostData::set(const Request* request) {
    request_provider_.reset(new RequestProvider(request));
}

const Request*
VirtualHostData::get() const {
    RequestProvider* provider = request_provider_.get();
    if (NULL == provider) {
        return NULL;
    }

    return provider->get();
}

void
VirtualHostData::setServer(const Server* server) {
    server_ = server;
}

const Server*
VirtualHostData::getServer() const {
    return server_;
}

bool
VirtualHostData::hasVariable(const Request* request, const std::string& var) const {
    if (NULL == request) {
        request = get();
        if (NULL == request) {
            return false;
        }
    }

    return request->hasVariable(var);
}

std::string
VirtualHostData::getVariable(const Request* request, const std::string& var) const {
    if (NULL == request) {
        request = get();
        if (NULL == request) {
            return StringUtils::EMPTY_STRING;
        }
    }

    return request->getVariable(var);
}

bool
VirtualHostData::checkVariable(const Request* request, const std::string& var) const {

    if (hasVariable(request, var)) {
        std::string value = VirtualHostData::instance()->getVariable(request, var);
        try {
            if (strncasecmp("yes", value.c_str(), sizeof("yes") - 1) == 0 ||
                strncasecmp("true", value.c_str(), sizeof("true") - 1) == 0 ||
                boost::lexical_cast<bool>(value) == 1) {
                return true;
            }
        }
        catch(boost::bad_lexical_cast &) {
            std::stringstream stream;
            stream << "Cannot cast to bool environment variable " << var
                   << ". Value: " << value;
            throw std::runtime_error(stream.str());
        }
    }

    return false;
}

std::string
VirtualHostData::getDocumentRoot(const Request* request) const {
    if (NULL == request) {
        request = get();
        if (NULL == request) {
            return StringUtils::EMPTY_STRING;
        }
    }

    std::string root = request->getDocumentRoot();
    while(!root.empty() && *root.rbegin() == '/') {
        root.erase(root.length() - 1, 1);
    }
    return root;
}

} // namespace xscript
