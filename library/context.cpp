#include "settings.h"

#include <cassert>
#include <stdexcept>
#include <algorithm>

#include <boost/bind.hpp>
#include <boost/thread/xtime.hpp>
#include <boost/current_function.hpp>

#include "xscript/block.h"
#include "xscript/state.h"
#include "xscript/logger.h"
#include "xscript/script.h"
#include "xscript/context.h"
#include "xscript/authorizer.h"
#include "xscript/stylesheet.h"
#include "xscript/request_data.h"
#include "xscript/vhost_data.h"

#include "details/writer_impl.h"
#include "details/extension_list.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace xscript
{

Context::Context(const boost::shared_ptr<Script> &script, const RequestData &data) :
	stopped_(false), request_(data.request()), response_(data.response()),
	xslt_name_(script->xsltName()), state_(data.state()), script_(script),
	writer_()
{
	assert(script_.get());
	ExtensionList::instance()->initContext(this);
}

Context::~Context() {
	ExtensionList::instance()->destroyContext(this);
	std::for_each(results_.begin(), results_.end(), boost::bind(&xmlFreeDoc, _1));
}

void
Context::wait(int millis) {
	
	log()->debug("%s, setting timeout: %d", BOOST_CURRENT_FUNCTION, millis);
	
	boost::xtime xt = delay(millis);
	boost::mutex::scoped_lock sl(results_mutex_);
	bool timedout = !condition_.timed_wait(sl, xt, boost::bind(&Context::resultsReady, this));
	
	if (timedout) {
		for (std::vector<xmlDocPtr>::size_type i = 0; i < results_.size(); ++i) {
			if (NULL == results_[i]) {
				results_[i] = script_->block(i)->errorResult("timed out").release();
			}
		}
	}
	ExtensionList::instance()->stopContext(this);
	stopped_ = true;
}

void
Context::expect(unsigned int count) {
	
	boost::mutex::scoped_lock sl(results_mutex_);
	if (stopped_) {
		throw std::logic_error("already stopped");
	}
	if (results_.size() == 0) {
		results_.resize(count);
	}
	else {
		throw std::logic_error("already started");
	}
}

void
Context::result(unsigned int n, xmlDocPtr doc) {
	
	log()->debug("%s: %d, result of %u block: %p", BOOST_CURRENT_FUNCTION, 
		static_cast<int>(stopped_), n, doc);

	XmlDocHelper doc_ptr(doc);
	boost::mutex::scoped_lock sl(results_mutex_);
	if (!stopped_ && results_.size() != 0) {
		assert(NULL == results_[n]);
		results_[n] = doc_ptr.release();
		condition_.notify_all();
	}
	else {
		log()->debug("%s, error in block %u: context not started or timed out", BOOST_CURRENT_FUNCTION, n);
	}
}

bool
Context::resultsReady() const {
	log()->debug("%s", BOOST_CURRENT_FUNCTION);
	for (std::vector<xmlDocPtr>::const_iterator i = results_.begin(), end = results_.end(); i != end; ++i) {
		if (NULL == (*i)) {
			return false;
		}
	}
	return true;
}

boost::xtime
Context::delay(int millis) const {
	
	boost::xtime xt;
	boost::xtime_get(&xt, boost::TIME_UTC);
	boost::uint64_t usec = (xt.sec * 1000000) + (xt.nsec / 1000) + (millis * 1000);
	
	xt.sec = usec / 1000000;
	xt.nsec = (usec % 1000000) * 1000;
	
	return xt;
}

xmlDocPtr
Context::result(unsigned int n) const {
	
	boost::mutex::scoped_lock sl(results_mutex_);
	if (results_.size() > 0) {
		return results_[n];
	}
	else {
		throw std::logic_error("not started");
	}
}

std::string
Context::xsltName() const {
	boost::mutex::scoped_lock sl(params_mutex_);
	return xslt_name_;
}

void
Context::xsltName(const std::string &value) {
	boost::mutex::scoped_lock sl(params_mutex_);
	xslt_name_ = value;
}

void
Context::authContext(std::auto_ptr<AuthContext> auth) {
	auth_ = auth;
}

DocumentWriter*
Context::documentWriter() {
	if (NULL == writer_.get()) {
		writer_ = std::auto_ptr<DocumentWriter>(
			new XmlWriter(VirtualHostData::instance()->getOutputEncoding(request_)));
	}

	return writer_.get();
}

void
Context::createDocumentWriter(const boost::shared_ptr<Stylesheet> &sh) {
	if ("xml" == sh->outputMethod()) {
		writer_ = std::auto_ptr<DocumentWriter>(new XmlWriter(sh->outputEncoding()));
	}
	else {
		writer_ = std::auto_ptr<DocumentWriter>(new HtmlWriter(sh));
	}
}

} // namespace xscript
