#ifndef SYSTEM_USAGE_H
#define SYSTEM_USAGE_H

extern int parseLine( char* line );

extern int getRAMUsage();

extern void initCPUUSage();

extern double getCPUUsage();

#endif  // SYSTEM_USAGE_H
