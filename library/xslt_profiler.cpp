#include "settings.h"
#include "xscript/xslt_profiler.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace xscript {

XsltProfiler::XsltProfiler() {
}

XsltProfiler::~XsltProfiler() {
}

void
XsltProfiler::insertProfileDoc(const std::string& name, xmlDocPtr doc) {
    (void)name;
    (void)doc;
}

void
XsltProfiler::dumpProfileInfo(boost::shared_ptr<Context> ctx) {
    (void)ctx;
}

static ComponentRegisterer<XsltProfiler> reg;

} // namespace xscript
