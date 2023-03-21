#include <skutils/btrfs.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

static _Thread_local char errbuf[256];
static _Thread_local char last_cmd[256];

static int shell_call(const char* cmd){

    strncpy(last_cmd, cmd, sizeof(last_cmd));
    last_cmd[sizeof(last_cmd)-1] = 0;

    char buf[strlen(cmd) + strlen(" 2>&1") + 1];
    strcpy(buf, cmd);
    strcat(buf, " 2>&1");

    fputs(cmd, stderr);
    fputs("\n", stderr);
    FILE* fp = popen(buf, "r");

    if(fp==NULL){
        strerror_r(errno, errbuf, sizeof(errbuf));
        fputs(errbuf, stderr);
        fputs("\n", stderr);
        return -1;
    }

    errbuf[0] = (char) 0;
    char * rs = fgets(errbuf, sizeof(errbuf), fp);
    (void)rs;

    // read everything else
    while(getc(fp) != EOF);

    int res = pclose(fp);

    if(res < 0){
        strerror_r(errno, errbuf, sizeof(errbuf));
        fputs(errbuf, stderr);
        fputs("\n", stderr);
    }

    // errors go here too:
    return WEXITSTATUS(res);
}

const char* btrfs_strerror(){
    return errbuf;
}

const char* btrfs_last_cmd(){
    return last_cmd;
}


int btrfs_present(const char* path){
    const char fmt[] = "btrfs filesystem df %s";

    int len = 1 + snprintf(NULL, 0, fmt, path);

    char* cmd = (char*) malloc(len);

    snprintf(cmd, len, fmt, path);

    int res = shell_call(cmd);
    free(cmd);

    if(strstr(btrfs_strerror(), "not a btrfs") == NULL)
        return 0;

    return res;
}

int btrfs_subvolume_list(const char* path){
    char fmt[] = "btrfs subvolume list %s";

    int len = 1 + snprintf(NULL, 0, fmt, path);

    char* cmd = (char*) malloc(len);

    snprintf(cmd, len, fmt, path);

    int res = shell_call(cmd);
    free(cmd);
    return res;
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
    char fmt[] = "btrfs subvolume delete -c %s";

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
    char* tmp_cmd;
    if ( parent == NULL ) {
        tmp_cmd = "btrfs send -q -f %s %s";
    } else {
        tmp_cmd = "btrfs send -q -p %s -f %s %s";
    }

    const char* fmt = tmp_cmd;

    unsigned len;
    if ( parent == NULL ) {
        len = 1 + snprintf(NULL, 0, fmt, file, vol);
    } else {
        len = 1 + snprintf(NULL, 0, fmt, parent, file, vol);
    }

    char* cmd = (char*) malloc(len);

    if ( parent == NULL ) {
        snprintf(cmd, len, fmt, file, vol);
    } else {
        snprintf(cmd, len, fmt, parent, file, vol);
    }

    int res = shell_call(cmd);
    free(cmd);
    return res;
}

int btrfs_subvolume_property_set(const char* path, const char* name, const char* value){
    char fmt[] = "btrfs property set -ts -f %s %s %s";

    int len = 1 + snprintf(NULL, 0, fmt, path, name, value);

    char* cmd = (char*) malloc(len);

    snprintf(cmd, len, fmt, path, name, value);

    int res = shell_call(cmd);
    free(cmd);
    return res;
}

btrfs_t btrfs = {btrfs_strerror, btrfs_last_cmd, btrfs_present, {btrfs_subvolume_list, btrfs_subvolume_create, btrfs_subvolume_delete, btrfs_subvolume_snapshot, btrfs_subvolume_snapshot_r}, btrfs_receive, btrfs_send, btrfs_subvolume_property_set};
