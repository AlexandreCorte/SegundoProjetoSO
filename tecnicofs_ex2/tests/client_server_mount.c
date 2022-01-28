#include "../client/tecnicofs_client_api.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    char* path = "/f1";
    char *str = "AAA!";
    
    if (argc < 3) {
        printf("You must provide the following arguments: 'client_pipe_path "
               "server_pipe_path'\n");
        return 1;
    }

    assert(tfs_mount(argv[1], argv[2])==0);

    int fd = tfs_open(path, TFS_O_CREAT);
    assert(fd!=-1);

    ssize_t w = tfs_write(fd, str, strlen(str));
    assert(w==strlen(str));

    int r = tfs_close(fd);
    assert(r!=-1);

    assert(tfs_unmount()==0);
    printf("Successful test\n");
    return 0;
}