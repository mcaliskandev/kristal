#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "server/kristal_c_api.h"
#include "server/Server.hpp"

static void usage(const char *prog) {
    printf("Usage: %s [-s startup command]\n", prog);
}

int main(int argc, char *argv[]) {
    char *startup_cmd = NULL;
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

    //KristalServer *server = kristal_server_create();
    KristalServer server;
	server.Create();
	/*
	if (server == NULL) {
        fprintf(stderr, "failed to allocate KristalServer\n");
        return 1;
    }
	*/

    //int rc = kristal_server_run(server, startup_cmd);
    server.Run(startup_cmd);

	//kristal_server_destroy(server);
    server.Destroy();
	return 0;
}
