#include "tecnicofs_client_api.h"

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    char client_path[MAX_PATH_SIZE];
    char server_path[MAX_PATH_SIZE];
    int session_client; //maximum size
    char op_code = TFS_OP_CODE_MOUNT+'0'; //convert to char

    memset(client_path, '\0', sizeof(client_path));
    memset(server_path, '\0', sizeof(server_path));
    memcpy(client_path, client_pipe_path, sizeof(client_path)); // \0 in client path
    memcpy(server_path, server_pipe_path, sizeof(server_path)); // \0 in server path
    memcpy(fifo_client_path, client_pipe_path, sizeof(fifo_client_path)); //save client fifo path

    unlink(client_path);
    if (mkfifo(client_path, 0777)==-1) //create client fifo
        return -1;

    int fd_server_path = open(server_path, O_WRONLY); //open server fifo
    if (fd_server_path==-1)
        return -1;

    file_server_handle = fd_server_path; //copy server fd

    char msg[sizeof(client_path)+sizeof(op_code)+3]; //create buffer
    
    sprintf(msg, "%c|%s|", op_code, client_path);

    if (write(file_server_handle, msg, sizeof(msg))==-1) //write msg to server fifo
        return -1;

    int fd_client_path = open(client_path, O_RDONLY); //open client fifo to read
    if (fd_client_path==-1)
        return -1;
    file_client_handle = fd_client_path; //save client fifo fd

    if (read(file_client_handle, &session_client, sizeof(int))==-1) //read session id
        return -1;
    session_id = session_client; //save session_id

    return 0;
}

int tfs_unmount() {
    char op_code = TFS_OP_CODE_UNMOUNT+'0';

    char msg[sizeof(op_code)+sizeof(int)+3]; //create msg
    int return_value;

    sprintf(msg, "%c|%d|", op_code, session_id);

    if (write(file_server_handle, msg, sizeof(msg))==-1)
        return -1;
    if (read(file_client_handle, &return_value, sizeof(int))==-1)
        return -1;
    if (close(file_client_handle)==-1)
        return -1;
    if (close(file_server_handle)==-1)
        return -1;
    if (unlink(fifo_client_path)==-1)
        return -1;

    if (return_value==0)
        return 0;
    return -1;
}

int tfs_open(char const*name, int flags){
    char file_name[MAX_PATH_SIZE];
    char op_code = TFS_OP_CODE_OPEN + '0';
    int return_value=-1;

    memset(file_name, '\0', sizeof(file_name));
    memcpy(file_name, name, sizeof(file_name));
    
    char msg[sizeof(op_code)+sizeof(int)+strlen(file_name)+sizeof(flags)+5];
    
    sprintf(msg, "%c|%d|%s|%d|", op_code, session_id, file_name, flags);
    
    if (write(file_server_handle, msg, sizeof(msg))==-1)
        return -1;

    if (read(file_client_handle, &return_value, sizeof(int))==-1)
        return -1;

    return return_value;
}

int tfs_close(int fhandle){
    int return_value;
    char op_code = TFS_OP_CODE_CLOSE + '0';

    char msg[sizeof(op_code)+sizeof(session_id)+sizeof(fhandle)+4];

    sprintf(msg, "%c|%d|%d|", op_code, session_id, fhandle);

    if (write(file_server_handle, msg, sizeof(msg))==-1)
        return -1;

    if (read(file_client_handle, &return_value, sizeof(int))==-1)
        return -1;

    return return_value;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len){
    char op_code = TFS_OP_CODE_WRITE + '0';
    int return_value;
    unsigned int size = (unsigned int)len;

    char msg[sizeof(op_code)+sizeof(session_id)+sizeof(fhandle)+sizeof(len)+size+6];
    
    sprintf(msg, "%c|%d|%d|%d|%s|", op_code, session_id, fhandle, (int)len, (char*)buffer);

    if (write(file_server_handle, msg, sizeof(msg))==-1)
        return -1;

    if (read(file_client_handle, &return_value, sizeof(int))==-1)
        return -1;

    return (ssize_t)return_value;

}

ssize_t tfs_read(int fhandle, void*buffer, size_t len){
    char op_code = TFS_OP_CODE_READ + '0';
    int i=0;
    unsigned int size = (unsigned int)len;
    char return_value[sizeof(int)];

    char output[size + sizeof(len)+3];
    char msg[sizeof(op_code)+sizeof(session_id)+sizeof(fhandle)+sizeof(len)+5];
    
    sprintf(msg, "%c|%d|%d|%d|", op_code, session_id, fhandle, (int)len);
    if (write(file_server_handle, msg, sizeof(msg))==-1)
        return -1;

    if (read(file_client_handle, output, sizeof(output))==-1)
        return -1;

    for (i=0; output[i]!='|'; i++){
        return_value[i]=output[i];
    }
    return_value[i]='\0';
    i++;

    memcpy(buffer, output+i, len);

    int return_int = atoi(return_value);

    return return_int;
}

int tfs_shutdown_after_all_closed(){
    char op_code = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED + '0';
    int return_value;

    char msg[sizeof(op_code)+sizeof(session_id)+3];

    sprintf(msg, "%c|%d|", op_code, session_id);

    if (write(file_server_handle, msg, sizeof(msg))==-1)
        return -1;

    if (read(file_client_handle, &return_value, sizeof(int))==-1)
        return -1;

    unlink(fifo_client_path);
    return return_value;
}