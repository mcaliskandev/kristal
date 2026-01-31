#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct KristalServer KristalServer;

KristalServer *kristal_server_create(void);
int kristal_server_run(KristalServer *server, const char *startup_cmd);
void kristal_server_destroy(KristalServer *server);

#ifdef __cplusplus
}
#endif
