#include "taskmon.h"

#include <fstream>
#include <sstream>
#include <algorithm>

#include <unistd.h>

using namespace std;
namespace fs = filesystem;

vector<string> taskmon::list_names() {
    vector<string> task_names;
    int self_pid = getpid();
    fs::path task_dir_path = "/proc/" + to_string(self_pid) + "/task";

    if (!fs::exists(task_dir_path))
        return task_names;

    for (const auto& task_entry : fs::directory_iterator(task_dir_path)) {
        if (task_entry.path().filename().string()[0] == '.')
            continue;

        string task_name = extract_task_name(task_entry);
        if (!task_name.empty())
            task_names.push_back(task_name);
    }

    return task_names;
}

vector<int> taskmon::list_ids() {
    vector<int> task_ids;
    int self_pid = getpid();
    fs::path task_dir_path = "/proc/" + to_string(self_pid) + "/task";

    if (!fs::exists(task_dir_path)) {
        return task_ids;
    }

    for (const auto& task_entry : fs::directory_iterator(task_dir_path)) {
        if (task_entry.path().filename().string()[0] == '.')
            continue;

        int task_id = stoi(task_entry.path().filename().string());
        task_ids.push_back(task_id);
    }

    return task_ids;
}

string taskmon::id2name(int tid) {
    string task_name;
    int self_pid = getpid();
    fs::path task_dir_path = "/proc/" + to_string(self_pid) + "/task/" + to_string(tid);

    if (!fs::exists(task_dir_path))
        return task_name;

    task_name = extract_task_name(task_dir_path);

    return task_name;
}

int taskmon::name2id(const string& name) {

    vector<int> ids = list_ids();

    int res = -1;
    for_each(ids.begin(), ids.end(), [&name, &res](int tid){
        if(id2name(tid)==name)
            res = tid;
    });

    return res;
}

string taskmon::status(int tid) {
    string task_status;
    int self_pid = getpid();
    fs::path task_dir_path = "/proc/" + to_string(self_pid) + "/task/" + to_string(tid);

    if (!fs::exists(task_dir_path))
        return task_status;

    ifstream status_file(task_dir_path / "status");
    string line;

    while (getline(status_file, line)) {
        if (line.find("State:") != string::npos) {
            istringstream iss(line);
            string state_str;
            iss >> state_str >> task_status;
            break;
        }
    }

    return task_status;
}

string taskmon::status(const string& name) {
    int task_id = name2id(name);
    if (task_id == -1) {
        return "";
    }
    return status(task_id);
}

vector<string> taskmon::lists_diff(const vector<string>& from, const vector<string>& to) {
    vector<string> diff;

    vector<string> from_copy = from;
    vector<string> to_copy = to;

    remove_if(from_copy.begin(), from_copy.end(), [&to](const string& s)->bool{
        return find(to.begin(), to.end(), s) != to.end();
    });

    // TODO complete it

    return diff;
}

string taskmon::extract_task_name(const fs::path& task_dir_path) {
    string task_name;

    ifstream comm_file(task_dir_path / "comm");
    getline(comm_file, task_name);

    return task_name;
}
