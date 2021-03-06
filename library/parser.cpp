#include "settings.h"

#include <stdexcept>

#include <libxml/xmlstring.h>

#include "xscript/algorithm.h"
#include "xscript/encoder.h"
#include "xscript/http_utils.h"
#include "xscript/logger.h"

#include "internal/parser.h"
#include "internal/request_impl.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace xscript {

const Range Parser::RN_RANGE = createRange("\r\n");
const Range Parser::NAME_RANGE = createRange("name");
const Range Parser::FILENAME_RANGE = createRange("filename");
const Range Parser::HEADER_RANGE = createRange("HTTP_");
const Range Parser::COOKIE_RANGE = createRange("HTTP_COOKIE");
const Range Parser::EMPTY_LINE_RANGE = createRange("\r\n\r\n");
const Range Parser::CONTENT_TYPE_RANGE = createRange("CONTENT_TYPE");
const Range Parser::CONTENT_TYPE_MULTIPART_RANGE = createRange("Content-Type");

static const std::string STR_BOUNDARY_PREFIX = "--";

std::string
Parser::getBoundary(const Range &range) {

    Range head, tail;
    split(range, ';', head, tail);

    tail = trim(tail);

    if (strncasecmp("boundary", tail.begin(), sizeof("boundary") - 1) == 0) {
        Range key, value;
        split(tail, '=', key, value);
        Range boundary = trim(value);

        Range comma = createRange("\"");
        if (startsWith(boundary, comma) && endsWith(boundary, comma)) {
            std::string res;
            res.reserve(STR_BOUNDARY_PREFIX.size() + boundary.size() - 2);
            return res.append(STR_BOUNDARY_PREFIX).append(boundary.begin() + 1, boundary.end() - 1);
        }

        std::string res;
        res.reserve(STR_BOUNDARY_PREFIX.size() + boundary.size());
        return res.append(STR_BOUNDARY_PREFIX).append(boundary.begin(), boundary.end());
    }
    throw std::runtime_error("no boundary found");
}

void
Parser::addCookie(RequestImpl *req, const Range &range, Encoder *encoder) {

    Range part = trim(range), head, tail;
    split(part, '=', head, tail);
    if (!head.empty()) {
        std::string key = StringUtils::urldecode(head), value = StringUtils::urldecode(tail);
        if (!xmlCheckUTF8((const xmlChar*) key.c_str())) {
            encoder->encode(key).swap(key);
        }
        if (!xmlCheckUTF8((const xmlChar*) value.c_str())) {
            encoder->encode(value).swap(value);
        }
        req->cookies_[key].swap(value);
    }
}

void
Parser::addHeader(RequestImpl *req, const Range &key, const Range &value, Encoder *encoder) {
    std::string header = HttpUtils::normalizeInputHeaderName(key);
    
    Range checked_value = value;
    if (strcmp(header.c_str(), RequestImpl::HOST_KEY.c_str()) == 0) {
        checked_value = HttpUtils::checkHost(value);
    }
        
    Range norm_value = checked_value;
    std::string result;
    if (HttpUtils::normalizeHeader(header, checked_value, result)) {
        norm_value = createRange(result);
    }
    
    if (!xmlCheckUTF8((const xmlChar*) norm_value.begin())) {
        encoder->encode(norm_value).swap(req->headers_[header]);
    }
    else {
        std::string &str = req->headers_[header];
        str.reserve(norm_value.size());
        str.assign(norm_value.begin(), norm_value.end());
    }
}

void
Parser::parse(RequestImpl *req, char *env[], Encoder *encoder) {
    for (int i = 0; NULL != env[i]; ++i) {
        log()->info("env[%d] = %s", i, env[i]);
        Range key, value;
        split(createRange(env[i]), '=', key, value);
        if (COOKIE_RANGE == key) {
            parseCookies(req, value, encoder);
            addHeader(req, truncate(key, HEADER_RANGE.size(), 0), trim(value), encoder);
        }
        else if (CONTENT_TYPE_RANGE == key) {
            addHeader(req, key, trim(value), encoder);
        }
        else if (startsWith(key, HEADER_RANGE)) { 
            addHeader(req, truncate(key, HEADER_RANGE.size(), 0), trim(value), encoder);
        }
        else {
            std::string name(key.begin(), key.end());
            std::string query_value;
            if (strcmp(name.c_str(), RequestImpl::QUERY_STRING_KEY.c_str()) == 0) {
                query_value = HttpUtils::checkUrlEscaping(value);
                value = createRange(query_value);
            }
            
            if (!xmlCheckUTF8((const xmlChar*) value.begin())) {
                encoder->encode(value).swap(req->vars_[name]);
            }
            else {
                std::string &str = req->vars_[name];
                str.reserve(value.size());
                str.assign(value.begin(), value.end());
            }
        }
    }
}

void
Parser::parseCookies(RequestImpl *req, const Range &range, Encoder *encoder) {
    Range part = trim(range), head, tail;
    while (!part.empty()) {
        split(part, ';', head, tail);
        addCookie(req, head, encoder);
        part = trim(tail);
    }
}

void
Parser::parseLine(Range &line, std::map<Range, Range, RangeCILess> &m) {

    Range head, tail, name, value;
    while (!line.empty()) {
        split(line, ';', head, tail);        
        if (head.size() >= CONTENT_TYPE_MULTIPART_RANGE.size() &&
            strncasecmp(head.begin(),
                    CONTENT_TYPE_MULTIPART_RANGE.begin(),
                    CONTENT_TYPE_MULTIPART_RANGE.size()) == 0) {
            split(head, ':', name, value);
            value = trim(value);
        }
        else {
            split(head, '=', name, value);
            if (NAME_RANGE == name || FILENAME_RANGE == name) {
                value = truncate(value, 1, 1);
            }
        }
        m.insert(std::make_pair(name, value));
        line = trim(tail);
    }
}

void
Parser::parsePart(RequestImpl *req, Range &part, Encoder *encoder) {

    Range headers, content, line, tail;
    std::map<Range, Range, RangeCILess> params;

    split(part, EMPTY_LINE_RANGE, headers, content);
    while (!headers.empty()) {
        split(headers, RN_RANGE, line, tail);
        parseLine(line, params);
        headers = tail;
    }
    std::map<Range, Range, RangeCILess>::iterator i = params.find(NAME_RANGE);
    if (params.end() == i) {
        return;
    }

    std::string name(i->second.begin(), i->second.end());
    if (!xmlCheckUTF8((const xmlChar*) name.c_str())) {
        encoder->encode(name).swap(name);
    }

    if (params.end() != params.find(FILENAME_RANGE)) {
        req->insertFile(name, params, content);
    }
    else {
        std::pair<std::string, std::string> p;
        p.first.swap(name);
        p.second.assign(content.begin(), content.end());
        if (!xmlCheckUTF8((const xmlChar*) p.second.c_str())) {
            encoder->encode(p.second).swap(p.second);
        }
        req->args_.push_back(p);
    }
}

void
Parser::parseMultipart(RequestImpl *req, Range &data, const std::string &boundary, Encoder *encoder) {
    Range head, tail, bound = createRange(boundary);
    while (!data.empty()) {
        split(data, bound, head, tail);
        if (!head.empty()) {
            head = truncate(head, 2, 2);
        }
        if (!head.empty()) {
            parsePart(req, head, encoder);
        }
        data = tail;
    }
}

} // namespace xscript
