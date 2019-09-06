#include <skutils/btrfs.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

static _Thread_local char errbuf[256];
static int shell_call(const char* cmd){

    FILE* fp = popen(cmd, "r");
    fputs(cmd, stderr);
    fputs("\n", stderr);

    if(fp==NULL){
        strerror_r(errno, errbuf, sizeof(errbuf));
        fputs(errbuf, stderr);
        fputs("\n", stderr);
        return -1;
    }

    fgets(errbuf, sizeof(errbuf)-1, fp);

    int res = pclose(fp);

    if(res < 0){
        strerror_r(errno, errbuf, sizeof(errbuf));
        fputs(cmd, stderr);
        fputs("\n", stderr);
    }
    return res;
}

const char* btrfs_strerror(){
    return errbuf;
}

int btrfs_subvolume_create(const char* path){
    char fmt[] = "btrfs subvolume create %s";

    int len = 1 + snprintf(NULL, 0, fmt, path);

    char* cmd = (char*) malloc(len);

    snprintf(cmd, len, fmt, path);

    int res = shell_call(cmd);
    free(cmd);
    return res;
}

int btrfs_subvolume_delete(const char* path){
    char fmt[] = "btrfs subvolume delete %s";

    int len = 1 + snprintf(NULL, 0, fmt, path);

    char* cmd = (char*) malloc(len);

    snprintf(cmd, len, fmt, path);

    int res = shell_call(cmd);
    free(cmd);
    return res;
}

int btrfs_subvolume_snapshot(const char* from, const char* to){
    char fmt[] = "btrfs subvolume snapshot %s %s";

    int len = 1 + snprintf(NULL, 0, fmt, from, to);

    char* cmd = (char*) malloc(len);

    snprintf(cmd, len, fmt, from, to);

    int res = shell_call(cmd);
    free(cmd);
    return res;
}

int btrfs_subvolume_snapshot_r(const char* from, const char* to){
    char fmt[] = "btrfs subvolume snapshot -r %s %s";

    int len = 1 + snprintf(NULL, 0, fmt, from, to);

    char* cmd = (char*) malloc(len);

    snprintf(cmd, len, fmt, from, to);

    int res = shell_call(cmd);
    free(cmd);
    return res;
}

int btrfs_receive(const char* file, const char* path){
    char fmt[] = "btrfs receive -f %s %s";

    int len = 1 + snprintf(NULL, 0, fmt, file, path);

    char* cmd = (char*) malloc(len);

    snprintf(cmd, len, fmt, file, path);

    int res = shell_call(cmd);
    free(cmd);
    return res;
}

int btrfs_send(const char* parent, const char* file, const char* vol){
    char* fmt = "btrfs send -q -p %s %s |cat >>%s";

    int len = 1 + snprintf(NULL, 0, fmt, parent, vol, file);

    char* cmd = (char*) malloc(len);

    snprintf(cmd, len, fmt, parent, vol, file);

    int res = shell_call(cmd);
    free(cmd);
    return res;
}

btrfs_t btrfs = {btrfs_strerror, {btrfs_subvolume_create, btrfs_subvolume_delete, btrfs_subvolume_snapshot, btrfs_subvolume_snapshot_r}, btrfs_receive, btrfs_send};
