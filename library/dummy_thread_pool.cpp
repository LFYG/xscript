#include "settings.h"

#include "details/dummy_thread_pool.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace xscript {

void
DummyThreadPool::invoke(boost::function<void()> f) {
    f();
}

void
DummyThreadPool::stop() {
}

static ComponentImplRegisterer<ThreadPool> reg_(new DummyThreadPool());
//REGISTER_COMPONENT2(ThreadPool, DummyThreadPool);

} // namespace xscript
