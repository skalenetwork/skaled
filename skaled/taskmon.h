#ifndef TASKMON_H
#define TASKMON_H

#include <vector>
#include <string>
#include <filesystem>

// tool to monitor running and exiting tasks
// assumes that task names are more or less unique
class taskmon {
public:
    static std::vector<std::string> list_names();
    static std::vector<int> list_ids();
    static std::string id2name(int tid);
    static int name2id(const std::string& name);
    static std::string status(int tid);
    static std::string status(const std::string& name);

    static std::vector<std::string> lists_diff(const std::vector<std::string>& from, const std::vector<std::string>& to);
private:
    static std::string extract_task_name(const std::filesystem::path& task_dir_path);
};

#endif // TASKMON_H
