#ifndef SCHAINPATCH_H
#define SCHAINPATCH_H

#include <libdevcore/Log.h>

class SchainPatch {
public:
    static void printInfo( const std::string& _patchName, time_t _timeStamp ) {
        if ( _timeStamp == 0 ) {
            cnote << "Patch " << _patchName << " is disabled";
        } else {
            cnote << "Patch " << _patchName << " is set at timestamp " << _timeStamp;
        }
    }
};

#endif  // SCHAINPATCH_H
