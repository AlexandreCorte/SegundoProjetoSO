#include "operations.h"
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_SESSIONS 20
#define MAX_PATH_SIZE 40

#define SIZE_OF_OPCODE 1
#define SIZE_OF_SESSION_ID 3
#define SIZE_OF_FHANDLE 4
#define SIZE_OF_LENGTH 4
#define SIZE_OF_FLAGS 1
#define MAX_TASKS 5
#define BLOCK_TASK 1200

#define FREE 0
#define TAKEN 1

typedef struct client {
    int session_id;
    char fifo_path[MAX_PATH_SIZE];
    int file_descriptor;
    int count;       // vai de 0 a 4
    int prodcounter; // vai de 0 a 4
    int conscounter; // vai de 0 a 4
    pthread_mutex_t mutex;
    pthread_cond_t podeConsumir;
    pthread_cond_t podeProduzir;
    pthread_t thread;
    char buffer[BLOCK_TASK * MAX_TASKS];
    int state;
} client;

client client_info[MAX_SESSIONS];


int create_session(char *client_path_name, int fd) {
    for (int i = 0; i != MAX_SESSIONS; i++) {
        if (client_info[i].state == FREE) {
            memcpy(client_info[i].fifo_path, client_path_name,
                   sizeof(client_info[i].fifo_path));
            client_info[i].file_descriptor = fd;
            client_info[i].state = TAKEN;
            return i;
        }
    }
    return -1;
}

int clear_session(int session_id) {
    for (int i = 0; i != MAX_SESSIONS; i++) {
        if (session_id == client_info[i].session_id) {
            client_info[i].state = FREE;
            memset(client_info[i].fifo_path, '\0',
                   sizeof(client_info[i].fifo_path));
            return client_info[i].file_descriptor;
        }
    }
    return -1;
}

void delete_pipes(int session_id) {
    for (int i = 0; i != MAX_SESSIONS; i++) {
        if (client_info[i].state == TAKEN && i != session_id) {
            unlink(client_info[i].fifo_path);
        }
    }
}

int write_pipe(int session_client) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (client_info[i].session_id == session_client) {
            char client_path[MAX_PATH_SIZE];
            memset(client_path, '\0', MAX_PATH_SIZE);
            memcpy(client_path, client_info[i].fifo_path,
                   sizeof(client_info[i].fifo_path));
            return client_info[i].file_descriptor;
        }
    }
    return -1;
}

void read_pipename(int fd, char clientpipename[]) {
    ssize_t command_bytes_read = 0;
    memset(clientpipename, '\0', MAX_PATH_SIZE + 1);
    command_bytes_read = read(fd, clientpipename, MAX_PATH_SIZE);
    if (command_bytes_read == -1)
        exit(1);
}

void read_sessionid(int fd, char session_id[]) {
    ssize_t command_bytes_read = 0;
    memset(session_id, '\0', SIZE_OF_SESSION_ID + 1);
    command_bytes_read = read(fd, session_id, SIZE_OF_SESSION_ID);
    if (command_bytes_read == -1)
        exit(1);
}

void read_filename(int fd, char filename[]) {
    ssize_t command_bytes_read = 0;
    memset(filename, '\0', MAX_FILE_NAME + 1);
    command_bytes_read = read(fd, filename, MAX_FILE_NAME);
    if (command_bytes_read == -1)
        exit(1);
}

void read_fhandle(int fd, char fhandle[]) {
    ssize_t command_bytes_read = 0;
    memset(fhandle, '\0', SIZE_OF_FHANDLE + 1);
    command_bytes_read = read(fd, fhandle, SIZE_OF_FHANDLE);
    if (command_bytes_read == -1)
        exit(1);
}

void read_size(int fd, char size[]) {
    ssize_t command_bytes_read = 0;
    memset(size, '\0', SIZE_OF_LENGTH + 1);
    command_bytes_read = read(fd, size, SIZE_OF_LENGTH);
    if (command_bytes_read == -1)
        exit(1);
}

void read_buffer(int fd, char buffer[], size_t size) {
    ssize_t command_bytes_read = 0;
    memset(buffer, '\0', size + 1);
    command_bytes_read = read(fd, buffer, size);
    if (command_bytes_read == -1)
        exit(1);
}

void read_flags(int fd, char *flags) {
    ssize_t command_bytes_read = 0;
    command_bytes_read = read(fd, flags, SIZE_OF_FLAGS);
    if (command_bytes_read == -1)
        exit(1);
}

int server_mount(char const *pipename) {
    char client_path_name[MAX_PATH_SIZE];
    memset(client_path_name, '\0', sizeof(client_path_name));
    memcpy(client_path_name, pipename, sizeof(client_path_name));

    int fd = open(client_path_name, O_WRONLY);
    if (fd == -1) {
        return -1;
    }

    int session_id = create_session(client_path_name, fd);

    if (write(fd, &session_id, sizeof(int)) == -1) {
        return -1;
    }
    return 0;
}

int server_unmount(char const *session_id) {
    int session = atoi(session_id);
    int pipe_to_write = write_pipe(session);
    int client_return_value = 0;
    if (pipe_to_write == -1)
        return -1;
    int return_value = clear_session(session);
    if (return_value == -1) {
        client_return_value = -1;
    }
    if (write(pipe_to_write, &client_return_value, sizeof(int)) == -1)
        return -1;
    if (return_value != -1) {
        if (close(return_value) == -1)
            return -1;
    }
    return 0;
}

int server_open(int session_id, char const *filename, char flags) {
    char file_name[MAX_PATH_SIZE];
    int flag, fd, pipe_to_write;

    memset(file_name, '\0', sizeof(file_name));

    memcpy(file_name, filename, sizeof(file_name));
    flag = flags + '0';
    pipe_to_write = write_pipe(session_id);
    if (pipe_to_write == -1)
        return -1;

    fd = tfs_open(file_name, flag);
    if (write(pipe_to_write, &fd, sizeof(int)) == -1)
        return -1;
    if (fd == -1)
        return -1;
    return 0;
}

int server_close(int session_id, char const *fhandle) {
    int return_value = 0, pipe_to_write;

    int int_fh = atoi(fhandle);

    return_value = tfs_close(int_fh);

    pipe_to_write = write_pipe(session_id);
    if (pipe_to_write == -1)
        return -1;

    if (write(pipe_to_write, &return_value, sizeof(int)) == -1)
        return -1;
    if (return_value == -1)
        return -1;
    return 0;
}

int server_write(int session_id, char const *fhandle,
                 char const *size, char const *buffer) {
    int pipe_to_write = 0;

    int file_handle = atoi(fhandle);
    size_t size_to_write = (size_t)atoi(size);

    ssize_t return_value = tfs_write(file_handle, buffer, size_to_write);

    pipe_to_write = write_pipe(session_id);
    if (pipe_to_write == -1)
        return -1;

    int return_int = (int)return_value;

    if (write(pipe_to_write, &return_int, sizeof(int)) == -1)
        return -1;
    if (return_int == -1)
        return -1;
    return 0;
}

int server_read(int session_id, char const *fhandle,
                char const *size) {
    int pipe_to_write = 0;
    int size_int = atoi(size);
    int file_handle = atoi(fhandle);

    char buffer_to_read[size_int + 1];
    memset(buffer_to_read, '\0', sizeof(buffer_to_read));
    size_t size_to_read = (size_t)size_int;

    int return_value = (int)tfs_read(file_handle, buffer_to_read, size_to_read);
    char output[size_to_read + SIZE_OF_LENGTH + 1];

    sprintf(output, "%04d%s", return_value, buffer_to_read);

    pipe_to_write = write_pipe(session_id);
    if (pipe_to_write == -1)
        return -1;

    if (write(pipe_to_write, output, sizeof(output) - 1) == -1)
        return -1;
    if (return_value == -1)
        return -1;
    return 0;
}

int server_shutdown(char const *session_client) {
    int pipe_to_write = 0;
    int session_id = atoi(session_client);

    delete_pipes(session_id);
    int return_value = tfs_destroy_after_all_closed();
    pipe_to_write = write_pipe(session_id);

    if (write(pipe_to_write, &return_value, sizeof(int)) == -1) {
        return -1;
    }
    if (return_value == -1) {
        return -1;
    }
    return 0;
}

void *consumidor(void *session) {
    int session_id = *(int *)session;
    while (1) {
        if (pthread_mutex_lock(&client_info[session_id].mutex) == -1)
            exit(1);
        while (client_info[session_id].count == 0){
            if (pthread_cond_wait(&client_info[session_id].podeConsumir,
                                  &client_info[session_id].mutex)==-1)
                                exit(1);
        }
        int offset = client_info[session_id].conscounter * BLOCK_TASK;
        char opcode;

        memcpy(&opcode, client_info[session_id].buffer+offset, SIZE_OF_OPCODE);
        if (opcode == '3'){
            char filename[MAX_FILE_NAME+1];
            char flags;
            memset(filename, '\0', MAX_FILE_NAME+1);
            memcpy(filename, client_info[session_id].buffer+offset+SIZE_OF_OPCODE, MAX_FILE_NAME);
            memcpy(&flags, client_info[session_id].buffer+offset+SIZE_OF_OPCODE+MAX_FILE_NAME, SIZE_OF_FLAGS);
            server_open(session_id, filename, flags);
        }
        if (opcode == '4'){
            char fhandle[SIZE_OF_FHANDLE+1];
            memset(fhandle, '\0', SIZE_OF_FHANDLE+1);
            memcpy(fhandle, client_info[session_id].buffer+offset+SIZE_OF_OPCODE, SIZE_OF_FHANDLE);
            server_close(session_id, fhandle);
        }
        if (opcode == '5'){
            char fhandle[SIZE_OF_FHANDLE+1];
            char size[SIZE_OF_LENGTH+1];
            memset(fhandle, '\0', SIZE_OF_FHANDLE+1);
            memset(size, '\0', SIZE_OF_LENGTH+1);
            memcpy(fhandle, client_info[session_id].buffer+offset+SIZE_OF_OPCODE, SIZE_OF_FHANDLE);
            memcpy(size, client_info[session_id].buffer+offset+SIZE_OF_OPCODE+SIZE_OF_FHANDLE, SIZE_OF_LENGTH);
            size_t size_of_buffer = (size_t)atoi(size);
            char buffer_to_write[size_of_buffer+1];
            memset(buffer_to_write, '\0', size_of_buffer+1);
            memcpy(buffer_to_write, client_info[session_id].buffer+offset+SIZE_OF_OPCODE+SIZE_OF_FHANDLE+SIZE_OF_LENGTH, size_of_buffer);
            server_write(session_id, fhandle, size, buffer_to_write);
        }
        if (opcode == '6'){
            char fhandle[SIZE_OF_FHANDLE+1];
            char size[SIZE_OF_LENGTH+1];
            memset(fhandle, '\0', SIZE_OF_FHANDLE+1);
            memset(size, '\0', SIZE_OF_LENGTH+1);
            memcpy(fhandle, client_info[session_id].buffer+offset+SIZE_OF_OPCODE, SIZE_OF_FHANDLE);
            memcpy(size, client_info[session_id].buffer+offset+SIZE_OF_OPCODE+SIZE_OF_FHANDLE, SIZE_OF_LENGTH);
            server_read(session_id, fhandle, size);
        }
        client_info[session_id].conscounter++;
        if (client_info[session_id].conscounter == MAX_TASKS) {
            client_info[session_id].conscounter = 0;
        }
        client_info[session_id].count--;
        if (pthread_cond_signal(&client_info[session_id].podeProduzir) == -1)
            exit(1);
        if (pthread_mutex_unlock(&client_info[session_id].mutex) == -1)
            exit(1);
    }
}

void struct_init() {
    for (int i = 0; i != MAX_SESSIONS; i++) {
        client_info[i].session_id = i;
        client_info[i].state = FREE;
        memset(client_info[i].fifo_path, '\0',
               sizeof(client_info[i].fifo_path));
        client_info[i].file_descriptor = -1;
        client_info[i].count = 0;
        client_info[i].conscounter = 0;
        client_info[i].prodcounter = 0;
        if (pthread_mutex_init(&client_info[i].mutex, NULL) == -1)
            exit(1);
        if (pthread_cond_init(&client_info[i].podeConsumir, NULL) == -1)
            exit(1);
        if (pthread_cond_init(&client_info[i].podeProduzir, NULL) == -1)
            exit(1);
        if (pthread_create(&client_info[i].thread, NULL, consumidor,
                           &client_info[i].session_id) == -1)
            exit(1);
    }
}

int main(int argc, char **argv) {
    char opcode;
    char clientpipename[MAX_PATH_SIZE + 1];
    char filename[MAX_FILE_NAME + 1];
    char session_id[SIZE_OF_SESSION_ID + 1];
    char fhandle[SIZE_OF_FHANDLE + 1];
    char size[SIZE_OF_LENGTH + 1];
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
        if (bytes_read == -1) {
            return -1;
        }
        if (bytes_read > 0) {
            if (opcode == '1') {
                read_pipename(fd, clientpipename);
                if (server_mount(clientpipename) == -1) {
                    return -1;
                }
            }
            if (opcode == '2') {
                read_sessionid(fd, session_id);
                if (server_unmount(session_id) == -1) {
                    return -1;
                }
            }
            if (opcode == '3' || opcode == '4' || opcode == '5' ||
                opcode == '6') {
                read_sessionid(fd, session_id);
                int session_client = atoi(session_id);
                if (pthread_mutex_lock(&client_info[session_client].mutex) ==
                    -1)
                    exit(1);
                while (client_info[session_client].count == MAX_TASKS)
                    pthread_cond_wait(&client_info[session_client].podeProduzir,
                                      &client_info[session_client].mutex);
                int offset =
                    client_info[session_client].prodcounter * BLOCK_TASK;
                if (opcode == '3') {
                    read_filename(fd, filename);
                    read_flags(fd, &flags);
                    memcpy(client_info[session_client].buffer + offset, &opcode,
                           SIZE_OF_OPCODE);
                    memcpy(client_info[session_client].buffer + offset + SIZE_OF_OPCODE,
                           filename, MAX_FILE_NAME);
                    memcpy(client_info[session_client].buffer + offset + SIZE_OF_OPCODE +
                               MAX_FILE_NAME,
                           &flags, SIZE_OF_FLAGS);
                }
                if (opcode == '4') {
                    read_fhandle(fd, fhandle);
                    memcpy(client_info[session_client].buffer + offset, &opcode,
                           SIZE_OF_OPCODE);
                    memcpy(client_info[session_client].buffer + offset + SIZE_OF_OPCODE,
                           fhandle, SIZE_OF_FHANDLE);
                }
                if (opcode == '5') {
                    read_fhandle(fd, fhandle);
                    read_size(fd, size);
                    size_t size_of_buffer = (size_t)atoi(size);
                    char *buffer_to_write = (char *)malloc(size_of_buffer + 1);
                    read_buffer(fd, buffer_to_write, size_of_buffer);
                    memcpy(client_info[session_client].buffer + offset, &opcode,
                           SIZE_OF_OPCODE);
                    memcpy(client_info[session_client].buffer + offset + SIZE_OF_OPCODE,
                           fhandle, SIZE_OF_FHANDLE);
                    memcpy(client_info[session_client].buffer + offset + SIZE_OF_OPCODE+
                               SIZE_OF_FHANDLE,
                           size, SIZE_OF_LENGTH);
                    memcpy(client_info[session_client].buffer + offset + SIZE_OF_OPCODE +
                               SIZE_OF_FHANDLE + SIZE_OF_LENGTH,
                           buffer_to_write, size_of_buffer);
                    free(buffer_to_write);
                }
                if (opcode == '6') {
                    read_fhandle(fd, fhandle);
                    read_size(fd, size);
                    memcpy(client_info[session_client].buffer + offset, &opcode,
                           SIZE_OF_OPCODE);
                    memcpy(client_info[session_client].buffer + offset + SIZE_OF_OPCODE,
                           fhandle, SIZE_OF_FHANDLE);
                    memcpy(client_info[session_client].buffer + offset + SIZE_OF_OPCODE +
                               SIZE_OF_FHANDLE,
                           size, SIZE_OF_LENGTH);
                }
                client_info[session_client].prodcounter++;
                if (client_info[session_client].prodcounter == MAX_TASKS)
                    client_info[session_client].prodcounter = 0;
                client_info[session_client].count++;
                if (pthread_cond_signal(
                        &client_info[session_client].podeConsumir) == -1)
                    exit(1);
                if (pthread_mutex_unlock(&client_info[session_client].mutex) ==
                    -1)
                    exit(1);
            }
            if (opcode == '7') {
                read_sessionid(fd, session_id);
                if (server_shutdown(session_id) == 0) {
                    unlink(pipename);
                    return 0;
                }
            }
        }
    }
    return 0;
}