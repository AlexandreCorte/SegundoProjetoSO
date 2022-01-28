#include "operations.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define MAX_SESSIONS 20
#define MAX_PATH_SIZE 40

typedef struct client {
    int session_id;
    char fifo_path[MAX_PATH_SIZE];
} client;

client client_info[MAX_SESSIONS];

void struct_init() {
    for (int i = 0; i != MAX_SESSIONS; i++) {
        client_info[i].session_id = -1;
        memset(client_info[i].fifo_path, '\0', sizeof(client_info[i].fifo_path));
    }
}

int create_session(char *client_path_name) {
    for (int i = 0; i != MAX_SESSIONS; i++) {
        if (client_info[i].session_id == -1) {
            client_info[i].session_id = i;
            memcpy(client_info[i].fifo_path, client_path_name, sizeof(client_info[i].fifo_path));
            return i;
        }
    }
    return -1;
}

int clear_session(int session_id) {
    for (int i = 0; i != MAX_SESSIONS; i++) {
        if (session_id == client_info[i].session_id) {
            client_info[i].session_id = -1;
            memset(client_info[i].fifo_path, '\0', sizeof(client_info[i].fifo_path));
            return 0;
        }
    }
    return -1;
}

void delete_pipes(int session_id){
    for (int i=0; i!=MAX_SESSIONS; i++){
        if (client_info[i].session_id!=-1 && i!=session_id){
            unlink(client_info[i].fifo_path);
        }
    }
}

int write_pipe(int session_client){
    for(int i=0; i<MAX_SESSIONS; i++){
        if (client_info[i].session_id==session_client){
            char client_path[MAX_PATH_SIZE];
            memset(client_path, '\0', MAX_PATH_SIZE);
            memcpy(client_path, client_info[i].fifo_path, sizeof(client_info[i].fifo_path));
            int fd = open(client_path, O_WRONLY);
            if (fd==-1)
                return -1;
            return fd;
        }
    }
    return -1;
}

int server_mount(char const* buffer) {
    char client_path_name[MAX_PATH_SIZE];
    memset(client_path_name, '\0', sizeof(client_path_name));
    int i=0;
    for (i=2; buffer[i]!='|'; i++){
        client_path_name[i-2] = buffer[i];
    }
    client_path_name[i-2]='\0';
    
    int session_id = create_session(client_path_name);
    if (session_id==-1){
        return -1;
    }
    int fd = open(client_path_name, O_WRONLY);
    if (fd == -1){
        return -1;
    }
    if (write(fd, &session_id, sizeof(int)) == -1){
        return -1;
    }
    return 0;
}

int server_unmount(char const* buffer) {
    char session_id[sizeof(int)];
    int i=0;
    for (i = 2; buffer[i] != '|'; i++) {
        session_id[i - 2] = buffer[i];
    }
    session_id[i-2]='\0';
    int session = atoi(session_id);
    int pipe_to_write = write_pipe(session);
    int return_value = clear_session(session);
    if (write(pipe_to_write, &return_value, sizeof(int))==-1)
        return -1;
    return 0;
}

int server_open(char const* buffer){
    char file_name[MAX_PATH_SIZE], name[MAX_PATH_SIZE], flags[2], session_id[sizeof(int)];
    int flag, fd, session_client, pipe_to_write, i=0, j=0;

    memset(name, '\0', sizeof(name));

    for (i=2; buffer[i]!='|'; i++){
        session_id[i-2]=buffer[i];
    }
    session_id[i-2]='\0';
    i++;
    for (j=0; buffer[i]!='|'; i++, j++){
        file_name[j]=buffer[i];
    }
    file_name[j]='\0';
    i++;
    flags[0] = buffer[i];
    flags[1] = '\0';

    memcpy(name, file_name, sizeof(name));
    flag = atoi(flags);
    session_client = atoi(session_id);
    pipe_to_write = write_pipe(session_client);

    fd = tfs_open(name, flag);
    if (write(pipe_to_write, &fd, sizeof(int))==-1)
        return -1;

    return 0;
}

int server_close(char const* buffer){
    int i=0, j=0, return_value=0, pipe_to_write;
    char session_client[sizeof(int)];
    char fhandle[sizeof(int)];

    for (i=2; buffer[i]!='|'; i++){
        session_client[i-2]=buffer[i];
    }
    session_client[i-2]='\0';
    i++;
    for (j=0; buffer[i]!='|'; i++, j++){
        fhandle[j]=buffer[i];
    }
    fhandle[j]='\0';
    int int_fh = atoi(fhandle);
    int session_id = atoi(session_client);

    return_value = tfs_close(int_fh);

    pipe_to_write = write_pipe(session_id);

    if (write(pipe_to_write, &return_value, sizeof(int))==-1)
        return -1;
    return 0;
}

int server_write(char const* buffer){
    int i=0, j=0, k=0, a=0, pipe_to_write=0;
    char session_client[sizeof(int)];
    char size[sizeof(size_t)];
    char fhandle[sizeof(int)];

    for (i=2; buffer[i]!='|'; i++){
        session_client[i-2]=buffer[i];
    }
    session_client[i-2]='\0';
    i++;

    for (j=0; buffer[i]!='|'; i++, j++){
        fhandle[j]=buffer[i];
    }
    fhandle[j]='\0';
    i++;

    for (k=0; buffer[i]!='|'; i++, k++){
        size[k]=buffer[i];
    }
    size[k]='\0';
    i++;

    int session_id = atoi(session_client);
    int size_int = atoi(size);
    int file_handle = atoi(fhandle);

    char buffer_to_write[size_int];
    for (a=0; buffer[i]!='|'; i++, a++){
        buffer_to_write[a]=buffer[i];
    }
    buffer_to_write[a]='\0';

    size_t size_to_write = (size_t)size_int;

    ssize_t return_value = tfs_write(file_handle, buffer_to_write, size_to_write);

    pipe_to_write = write_pipe(session_id);

    int return_int = (int)return_value;

    if (write(pipe_to_write, &return_int, sizeof(int))==-1)
        return -1;
    return 0;
}

int server_shutdown(char const* buffer){
    int i=0, pipe_to_write =0;
    char session_client[sizeof(int)];
    for (i=2; buffer[i]!='|'; i++){
        session_client[i-2]=buffer[i];
    }
    session_client[i-2]='\0';
    int session_id = atoi(session_client);

    delete_pipes(session_id);
    int return_value = tfs_destroy_after_all_closed();

    pipe_to_write = write_pipe(session_id);

    if (write(pipe_to_write, &return_value, sizeof(int))==-1)
        return -1;
    return 0;
}

int main(int argc, char **argv) {
    char buffer[2048];
    tfs_init();
    struct_init();
    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);
    if (mkfifo(pipename, 0777) == -1)
        return -1;

    int fd = open(pipename, O_RDONLY);
    if (fd == -1)
        return -1;
        
    while (1) {
        memset(buffer, '\0', 2048);
        ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
        if (bytes_read == -1){
            return -1;
        }
        if (bytes_read > 0) {
            char op_code = buffer[0];
            switch (op_code) {
            case '1':
                if (server_mount(buffer) == -1){
                    return -1;
                }
                break;
            case '2':
                if (server_unmount(buffer) == -1){
                    return -1;
                }
                break;
            case '3':
                if (server_open(buffer) == -1){
                    return -1;
                }
                break;
            case '4':
                if (server_close(buffer) == -1){
                    return -1;
                }
                break;
            case '5':
                if (server_write(buffer) == -1){
                    return -1;
                }
                break;
            case '7':
                if (server_shutdown(buffer)== 0){
                    unlink(pipename);
                    return 0;
                }
                return -1;
                break;
            default:
                return -1;
                break;
            }
        }
    }
    return 0;
}