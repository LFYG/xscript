#include "settings.h"

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "xscript/context.h"
#include "xscript/exception.h"
#include "xscript/script.h"
#include "xscript/script_factory.h"
#include "xscript/state.h"
#include "xscript/test_utils.h"
#include "xscript/xml_util.h"

#include "../file-block/file_block.h"
#include "../file-block/file_extension.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

using namespace xscript;

class FileTest : public CppUnit::TestFixture {
public:
    CPPUNIT_TEST_SUITE(FileTest);
    CPPUNIT_TEST_EXCEPTION(testUnknowMethod, ParseError);
    CPPUNIT_TEST(testLoad);
    CPPUNIT_TEST(testInclude);
    CPPUNIT_TEST_SUITE_END();

public:
    void testUnknowMethod();
    void testLoad();
    void testInclude();
};

#ifdef HAVE_FILE_BLOCK

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(FileTest, "file");
CPPUNIT_REGISTRY_ADD("file", "xscript");

#endif

void
FileTest::testUnknowMethod() {
    boost::shared_ptr<Context> ctx = TestUtils::createEnv("./file-unknownMethod.xml");
}


void
FileTest::testLoad() {
    boost::shared_ptr<Context> ctx = TestUtils::createEnv("./file-load.xml");
    ContextStopper ctx_stopper(ctx);
    XmlDocSharedHelper doc = ctx->script()->invoke(ctx);
    CPPUNIT_ASSERT(NULL != doc.get());
    CPPUNIT_ASSERT(XmlUtils::xpathExists(doc.get(), "//include-data"));
}

void
FileTest::testInclude() {
    boost::shared_ptr<Context> ctx = TestUtils::createEnv("./file-include.xml");
    ContextStopper ctx_stopper(ctx);
    XmlDocSharedHelper doc = ctx->script()->invoke(ctx);
    CPPUNIT_ASSERT(NULL != doc.get());
    CPPUNIT_ASSERT(XmlUtils::xpathExists(doc.get(), "//include-data"));
}
