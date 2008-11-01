#ifndef _XSCRIPT_POLICY_H_
#define _XSCRIPT_POLICY_H_

#include <vector>
#include <string>

#include "xscript/component.h"

namespace xscript {

class Request;

class Policy: public virtual Component<Policy> {
public:
    Policy();
    virtual ~Policy();

    virtual void getProxyHttpHeaders(const Request *req, std::vector<std::string> &headers) const;   
    virtual std::string getPathBySchema(const std::string &url) const;

protected:
    virtual bool isSkippedProxyHeader(const std::string &header) const;
};

} // namespace xscript

#endif // _XSCRIPT_POLICY_H_
