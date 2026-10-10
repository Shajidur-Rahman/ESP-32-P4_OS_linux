#pragma once

#include <stdint.h>
#include <stddef.h>

#define WIFI_TERMINAL_PORT 23

#ifdef __cplusplus
extern "C" {
#endif

void wifi_terminal_start(void);
int wifi_terminal_send_data(const char *data, int len);

#ifdef __cplusplus
}
#endif
