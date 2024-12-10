#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "main.h"

BOOL grant_privilege(PWSTR privilege);
DWORD get_process_id(PWSTR process_name);
BOOL inject_dll(DWORD process_id);
UINT64 get_time();

#endif /* FUNCTIONS_H */