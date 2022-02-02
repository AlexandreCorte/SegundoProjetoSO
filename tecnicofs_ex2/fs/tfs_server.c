#include "operations.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_SESSIONS 20
#define MAX_PATH_SIZE 40

#define SIZE_OF_SESSION_ID 3
#define SIZE_OF_LENGTH 4
#define SIZE_OF_FHANDLE 4
#define SIZE_OF_FLAGS 1

typedef struct client {
    int session_id;
    char fifo_path[MAX_PATH_SIZE];
    int file_descriptor;
} client;

client client_info[MAX_SESSIONS];

void struct_init() {
    for (int i = 0; i != MAX_SESSIONS; i++) {
        client_info[i].session_id = -1;
        memset(client_info[i].fifo_path, '\0', sizeof(client_info[i].fifo_path));
        client_info[i].file_descriptor=-1;
    }
}

int create_session(char *client_path_name, int fd) {
    for (int i = 0; i != MAX_SESSIONS; i++) {
        if (client_info[i].session_id == -1) {
            client_info[i].session_id = i;
            memcpy(client_info[i].fifo_path, client_path_name, sizeof(client_info[i].fifo_path));
            client_info[i].file_descriptor = fd;
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
            return client_info[i].file_descriptor;
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
            return client_info[i].file_descriptor;
        }
    }
    return -1;
}

int read_commands(int fd, void* clientpipename){
    ssize_t command_bytes_read=0;
    memset(clientpipename, '\0', sizeof(*clientpipename));
    command_bytes_read = read(fd, clientpipename, sizeof(clientpipename)-1);
    if (command_bytes_read==-1)
        return -1;
    return 0;
}

int server_mount(char const* pipename) {
    char client_path_name[MAX_PATH_SIZE];
    memset(client_path_name, '\0', sizeof(client_path_name));
    memcpy(client_path_name, pipename, sizeof(client_path_name));
    
    int fd = open(client_path_name, O_WRONLY);
    if (fd == -1){
        return -1;
    }

    int session_id = create_session(client_path_name, fd);

    if (write(fd, &session_id, sizeof(int)) == -1){
        return -1;
    }
    return 0;
}

int server_unmount(char const* session_id) {
    int session = atoi(session_id);
    int pipe_to_write = write_pipe(session);
    int client_return_value=0;
    if (pipe_to_write==-1)
        return -1;
    int return_value = clear_session(session);
    if (return_value==-1){
        client_return_value=-1;
    }
    if (write(pipe_to_write, &client_return_value, sizeof(int))==-1)
        return -1;
    if (return_value!=-1){
        if (close(return_value)==-1)
            return -1;
    }
    return 0;
}

int server_open(char const* session_id, char const* filename, char flags){
    char file_name[MAX_PATH_SIZE];
    int flag, fd, session_client, pipe_to_write;

    memset(file_name, '\0', sizeof(file_name));

    memcpy(file_name, filename, sizeof(file_name));
    flag = flags + '0';
    session_client = atoi(session_id);
    pipe_to_write = write_pipe(session_client);
    if (pipe_to_write==-1)
        return -1;

    fd = tfs_open(file_name, flag);
    if (write(pipe_to_write, &fd, sizeof(int))==-1)
        return -1;
    if (fd==-1)
        return -1;
    return 0;
}

int server_close(char const* session_id, char const* fhandle){
    int return_value=0, pipe_to_write;

    int int_fh = atoi(fhandle);
    int session_client = atoi(session_id);

    return_value = tfs_close(int_fh);

    pipe_to_write = write_pipe(session_client);
    if (pipe_to_write==-1)
        return -1;

    if (write(pipe_to_write, &return_value, sizeof(int))==-1)
        return -1;
    if (return_value==-1)
        return -1;
    return 0;
}

int server_write(char const* session_client, char const* fhandle, char const* size, char const* buffer){
    int pipe_to_write=0;

    int session_id = atoi(session_client);
    int file_handle = atoi(fhandle);
    size_t size_to_write = (size_t)atoi(size);

    ssize_t return_value = tfs_write(file_handle, buffer, size_to_write);

    pipe_to_write = write_pipe(session_id);
    if (pipe_to_write==-1)
        return -1;

    int return_int = (int)return_value;

    if (write(pipe_to_write, &return_int, sizeof(int))==-1)
        return -1;
    if (return_int==-1)
        return -1;
    return 0;
}

int server_read(char const* session_client, char const* fhandle, char const* size){
    int pipe_to_write=0;

    int session_id = atoi(session_client);
    int size_int = atoi(size);
    int file_handle = atoi(fhandle); 

    char buffer_to_read[size_int+1];   
    memset(buffer_to_read, '\0', sizeof(buffer_to_read));
    size_t size_to_read = (size_t)size_int;

    int return_value = (int)tfs_read(file_handle, buffer_to_read, size_to_read);
    char output[size_to_read + SIZE_OF_LENGTH + 1];

    sprintf(output, "%04d%s", return_value, buffer_to_read);

    pipe_to_write = write_pipe(session_id);
    if (pipe_to_write==-1)
        return -1;

    if (write(pipe_to_write, output, sizeof(output)-1)==-1)
        return -1;
    if (return_value==-1)
        return -1;
    return 0;
}

int server_shutdown(char const* session_client){
    int pipe_to_write =0;  
    int session_id = atoi(session_client);

    delete_pipes(session_id);
    int return_value = tfs_destroy_after_all_closed();
    pipe_to_write = write_pipe(session_id);
    
    if (write(pipe_to_write, &return_value, sizeof(int))==-1){
        return -1;
    }
    if (return_value==-1){
        return -1;
    }        
    return 0;
}

int main(int argc, char **argv) {
    char opcode;
    char clientpipename[MAX_PATH_SIZE+1];
    char filename[MAX_FILE_NAME+1];
    char session_id[SIZE_OF_SESSION_ID+1];
    char fhandle[SIZE_OF_FHANDLE+1];
    char size[SIZE_OF_LENGTH+1];
    char flags;

    tfs_init();
    struct_init();
    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);
    unlink(pipename);
    if (mkfifo(pipename, 0777) == -1)
        return -1;

    int fd = open(pipename, O_RDONLY);
    if (fd == -1)
        return -1;
        
    while (1) {
        ssize_t bytes_read = read(fd, &opcode, sizeof(opcode));
        ssize_t command_bytes_read;
        if (bytes_read == -1){
            return -1;
        }
        if (bytes_read > 0) {
            switch (opcode) {
            case '1':
                memset(clientpipename, '\0', sizeof(clientpipename));
                command_bytes_read = read(fd, clientpipename, sizeof(clientpipename)-1);
                if (command_bytes_read==-1)
                    return -1;
                if (server_mount(clientpipename) == -1){
                    return -1;
                }
                break;
            case '2':
                memset(session_id, '\0', sizeof(session_id));
                command_bytes_read = read(fd, session_id, sizeof(session_id)-1);
                if (command_bytes_read==-1)
                    return -1;
                if (server_unmount(session_id) == -1){
                    return -1;
                }
                break;
            case '3':
                memset(session_id, '\0', sizeof(session_id));
                memset(filename, '\0', sizeof(filename));

                command_bytes_read = read(fd, session_id, sizeof(session_id)-1);
                if (command_bytes_read==-1)
                    return -1;
                command_bytes_read = read(fd, filename, sizeof(filename)-1);
                if (command_bytes_read==-1)
                    return -1;
                command_bytes_read = read(fd, &flags, sizeof(flags));
                if (command_bytes_read==-1)
                    return -1;
                if (server_open(session_id, filename, flags) == -1){
                    return -1;
                }
                break;
            case '4':
                memset(session_id, '\0', sizeof(session_id));
                memset(fhandle, '\0', sizeof(fhandle));
                command_bytes_read = read(fd, session_id, sizeof(session_id)-1);
                if (command_bytes_read==-1)
                    return -1;
                command_bytes_read = read(fd, fhandle, sizeof(fhandle)-1);
                if (command_bytes_read==-1)
                    return -1;
                if (server_close(session_id, fhandle) == -1){
                    return -1;
                }
                break;
            case '5':
                memset(session_id, '\0', sizeof(session_id));
                memset(size, '\0', sizeof(size));
                memset(fhandle, '\0', sizeof(fhandle));
                command_bytes_read = read(fd, session_id, sizeof(session_id)-1);
                if (command_bytes_read==-1)
                    return -1;
                command_bytes_read = read(fd, fhandle, sizeof(fhandle)-1);
                if (command_bytes_read==-1)
                    return -1;
                command_bytes_read = read(fd, size, sizeof(size)-1);
                if (command_bytes_read==-1)
                    return -1;
                size_t size_of_buffer = (size_t)atoi(size);
                char* buffer_to_write = (char*)malloc(size_of_buffer+1);
                memset(buffer_to_write, '\0', sizeof(*buffer_to_write));
                command_bytes_read = read(fd, buffer_to_write, sizeof(buffer_to_write)-1);
                if (command_bytes_read==-1)
                    return -1;
                if (server_write(session_id, fhandle, size, buffer_to_write) == -1){
                    return -1;
                }
                free(buffer_to_write);
                break;
            case '6':
                memset(session_id, '\0', sizeof(session_id));
                memset(size, '\0', sizeof(size));
                memset(fhandle, '\0', sizeof(fhandle));
                command_bytes_read = read(fd, session_id, sizeof(session_id)-1);
                if (command_bytes_read==-1)
                    return -1;
                command_bytes_read = read(fd, fhandle, sizeof(fhandle)-1);
                if (command_bytes_read==-1)
                    return -1;
                command_bytes_read = read(fd, size, sizeof(size)-1);
                if (command_bytes_read==-1)
                    return -1;
                if (server_read(session_id, fhandle, size) == -1){
                    return -1;
                }
                break;
            case '7':
                memset(session_id, '\0', sizeof(session_id));
                command_bytes_read = read(fd, session_id, sizeof(session_id)-1);
                if (command_bytes_read==-1)
                    return -1;
                if (server_shutdown(session_id)== 0){
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