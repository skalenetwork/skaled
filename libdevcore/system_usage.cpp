#include "system_usage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sys/times.h"
//#include "sys/vtimes.h"

int parseLine( char* line ) {
    // This assumes that a digit will be found and the line ends in " Kb".
    int i = strlen( line );
    const char* p = line;
    while ( *p < '0' || *p > '9' )
        p++;
    line[i - 3] = '\0';
    i = atoi( p );
    return i;
}

int getRAMUsage() {  // Note: this value is in KB!
    FILE* file = fopen( "/proc/self/status", "r" );
    int result = -1;
    char line[128];

    while ( fgets( line, 128, file ) != NULL ) {
        if ( strncmp( line, "VmRSS:", 6 ) == 0 ) {
            result = parseLine( line );
            break;
        }
    }
    fclose( file );
    return result;
}

static clock_t lastCPU, lastSysCPU, lastUserCPU;
static int numProcessors;

void initCPUUSage() {
    FILE* file;
    struct tms timeSample;
    char line[128];

    lastCPU = times( &timeSample );
    lastSysCPU = timeSample.tms_stime;
    lastUserCPU = timeSample.tms_utime;

    file = fopen( "/proc/cpuinfo", "r" );
    numProcessors = 0;
    while ( fgets( line, 128, file ) != NULL ) {
        if ( strncmp( line, "processor", 9 ) == 0 )
            numProcessors++;
    }
    fclose( file );
}

double getCPUUsage() {
    initCPUUSage();
    struct tms timeSample;
    clock_t now;
    double percent;

    now = times( &timeSample );
    if ( now <= lastCPU || timeSample.tms_stime < lastSysCPU ||
         timeSample.tms_utime < lastUserCPU ) {
        // Overflow detection. Just skip this value.
        percent = -1.0;
    } else {
        percent = ( timeSample.tms_stime - lastSysCPU ) + ( timeSample.tms_utime - lastUserCPU );
        percent /= ( now - lastCPU );
        percent /= numProcessors;
        percent *= 100;
    }
    lastCPU = now;
    lastSysCPU = timeSample.tms_stime;
    lastUserCPU = timeSample.tms_utime;

    return percent;
}
