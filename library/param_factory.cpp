#include "settings.h"

#include <cctype>
#include <sstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <algorithm>

#include <boost/current_function.hpp>

#include "xscript/util.h"
#include "xscript/range.h"
#include "xscript/logger.h"
#include "details/param_factory.h"
#include "internal/algorithm.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace xscript
{

ParamFactory::ParamFactory() 
{
}

ParamFactory::~ParamFactory() {
}

void
ParamFactory::registerCreator(const char *name, ParamCreator creator) {
	
	try {
		Range range = trim(createRange(name));
		std::pair<std::string, ParamCreator> pair;
		
		pair.first.reserve(range.size());
		std::transform(range.begin(), range.end(), std::back_inserter(pair.first), &tolower);

		CreatorMap::iterator i = creators_.find(pair.first);
		if (creators_.end() == i) {
			pair.second = creator;
			creators_.insert(pair);
		}
		else {
			std::stringstream stream;
			stream << "duplicate param: " << pair.first;
			throw std::invalid_argument(stream.str());
		}
	}
	catch (const std::exception &e) {
		std::cerr <<  e.what() << std::endl;
		throw;
	}
}

std::auto_ptr<Param>
ParamFactory::param(Object *owner, xmlNodePtr node) {
	
	assert(node);
	assert(owner);
	try {
		const char *attr = XmlUtils::attrValue(node, "type");
		if (NULL != attr) {
			Range range = trim(createRange(attr));
			
			std::string name;
			name.reserve(range.size());
			std::transform(range.begin(), range.end(), std::back_inserter(name), &tolower);
			
			ParamCreator c = findCreator(name);
			std::auto_ptr<Param> p = (*c)(owner, node);
			
			assert(p.get());
			p->parse();
			return p;
		}
		else {
			throw std::logic_error("param without type");
		}
	}
	catch (const std::exception &e) {
		log()->error("%s, caught exception: %s", BOOST_CURRENT_FUNCTION, e.what());
		throw;
	}
}

ParamCreator
ParamFactory::findCreator(const std::string &name) const {
	CreatorMap::const_iterator i = creators_.find(name);
	if (creators_.end() != i) {
		return i->second;
	}
	else {
		std::stringstream stream;
		stream << "nonexistent parameter type: " << name;
		throw std::invalid_argument(stream.str());
	}
}

CreatorRegisterer::CreatorRegisterer(const char *name, ParamCreator c) {
	assert(name);
	ParamFactory::instance()->registerCreator(name, c);
}

} // namespace xscript
