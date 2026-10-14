// main/sntp/Ds1307.h
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool ds1307_init(void);
bool ds1307_read_system_time(void);
bool ds1307_write_system_time(void);
bool ds1307_have_time(void);

#ifdef __cplusplus
}
#endif
