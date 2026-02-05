#include <getopt.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#include "server/Server.hpp"

static void usage(const char *prog) {
    std::printf("Usage: %s [-s startup command]\n", prog);
}

int main(int argc, char *argv[]) {
    char *startup_cmd = nullptr;
    int c;

    while ((c = getopt(argc, argv, "s:h")) != -1) {
        switch (c) {
        case 's':
            startup_cmd = optarg;
            break;
        default:
            usage(argv[0]);
            return 0;
        }
    }
    if (optind < argc) {
        usage(argv[0]);
        return 0;
    }

    KristalCompositor server;
	server.Create();
    server.Run(startup_cmd);
    server.Destroy();
	return 0;
}
