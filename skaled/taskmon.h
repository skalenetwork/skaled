#ifndef TASKMON_H
#define TASKMON_H

#include <filesystem>
#include <string>
#include <vector>

// tool to monitor running and exiting tasks
// assumes that task names are more or less unique
// methods can throw exceptions in case if /proc is inaccessible
// so, ber sure to catch!
class taskmon {
public:
    // get all thread names of current process (can have duplicates)
    static std::vector< std::string > list_names();
    // get all thread ids of current process
    static std::vector< int > list_ids();
    // convert thread id to name
    static std::string id2name( int tid );
    // convert thread name to id
    static int name2id( const std::string& name );
    // get thread status from /proc/PID/task/TID/staus
    // e.g. "S" (for "sleeping")
    static std::string status( int tid );
    // get thread status by name (see above)
    static std::string status( const std::string& name );

    // return how list1 and list2 differ
    // adds '+' or '-' signs to elements
    static std::vector< std::string > lists_diff(
        const std::vector< std::string >& from, const std::vector< std::string >& to );

private:
    static std::string extract_task_name( const std::filesystem::path& task_dir_path );
};

#endif  // TASKMON_H
