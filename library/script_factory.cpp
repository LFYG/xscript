#include "settings.h"
#include "xscript/script.h"
#include "xscript/script_factory.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace xscript
{

ScriptFactory::ScriptFactory()
{
}

ScriptFactory::~ScriptFactory() {
}

void
ScriptFactory::init(const Config *config) {
}

boost::shared_ptr<Script>
ScriptFactory::create(const std::string &name) {
	return boost::shared_ptr<Script>(new Script(name));
}

} // namespace xscript
