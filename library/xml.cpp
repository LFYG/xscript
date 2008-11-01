#include "settings.h"

#include <stdexcept>
#include <boost/filesystem/path.hpp>

#include "xscript/policy.h"
#include "xscript/xml.h"
#include "xscript/xml_helpers.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace xscript {

namespace fs = boost::filesystem;

Xml::Xml() {
}

Xml::~Xml() {
}

std::string
Xml::fullName(const std::string &object) const {

    if (object.empty()) {
        throw std::runtime_error("Empty relative path");
    }

    fs::path path;
    if (*object.begin() == '/') {
        path = fs::path(object);
    }
    else {
        const std::string transformed = Policy::instance()->getPathByScheme(object);
        if (transformed.empty()) {
            throw std::runtime_error("Empty relative path");
        }
        if (*transformed.begin() == '/') {
            path = fs::path(transformed);
        }
        else {
            const std::string &owner_name = name();
            if (owner_name.empty()) {
                path = fs::path(transformed);
            }
            else if (*owner_name.rbegin() == '/') {
                path = fs::path(owner_name + transformed);
            }
            else {
                std::string::size_type pos = owner_name.find_last_of('/');
                if (pos == std::string::npos) {
                    path = fs::path(transformed);
                }
                else {
                    path = fs::path(owner_name.substr(0, pos + 1) + transformed);
                }
            }
        }
    }

    path.normalize();
    return path.string();
}

} // namespace xscript
