#include "settings.h"
#include "details/loader.h"
#include "xscript/component.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace xscript
{

Component::Component() 
{
	loader_ = Loader::instance();
}

Component::~Component() {
}

} // namespace xscript
