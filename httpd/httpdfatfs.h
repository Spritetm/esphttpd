#ifndef HTTPDFATFS_H
#define HTTPDFATFS_H

#include "httpd.h"

int cgiFatFsHook(HttpdConnData *connData);
int cgiFatFsDirHook(HttpdConnData *connData);

#endif