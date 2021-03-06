#include "settings.h"

#include <map>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <cstring>

#include "xscript/config.h"
#include "xscript/vhost_data.h"

#include "proc_server.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

void
parse(const char *argv, std::multimap<std::string, std::string> &m) {
    const char *pos = strchr(argv, '=');
    if (NULL != pos) {
        m.insert(std::pair<std::string, std::string>(std::string(argv, pos), std::string(pos + 1)));
    }
    else {
        m.insert(std::pair<std::string, std::string>(argv, ""));
    }
}

std::ostream&
processUsage(std::ostream &os) {
    os << "Usage:\n"
    " xscript-proc --config=file file | url [options]\n"
    " options:\n"
    "  --docroot=<value> | --root-dir=<value>\n"
    "  --header=<value> [ .. --header=<value> ]\n"
    "  --profile | --norman\n"
    "  --stylesheet=<value>\n"
    "  --dont-apply-stylesheet | --dont-apply-stylesheet=all\n"
    "  --dont-use-remote-call\n"
    "  --noout";
    return os;
}

int
main(int argc, char *argv[]) {

    using namespace xscript;
    try {
        std::auto_ptr<Config> config = Config::create(argc, argv, true);
        if (NULL == config.get()) {
            config = Config::create("/etc/xscript/offline.conf");
            argc--;
        }

        std::string url;
        std::multimap<std::string, std::string> args;
        for (int i = 1; i <= argc; ++i) {
            if (strncmp(argv[i], "--", sizeof("--") - 1) == 0) {
                parse(argv[i] + sizeof("--") - 1, args);
            }
            else if (url.empty()) {
                url.assign(argv[i]);
            }
            else {
                throw std::runtime_error("url defined twice");
            }
        }
        if (argc == 0 || args.end() != args.find("help")) {
            processUsage(std::cout) << std::endl;
            return EXIT_SUCCESS;
        }

        VirtualHostData::instance()->setConfig(config.get());
        ProcServer server(config.get(), url, args);
        server.run();

        return EXIT_SUCCESS;
    }
    catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
