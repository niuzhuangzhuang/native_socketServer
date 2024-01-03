#include <cutils/log.h>
#include <cutils/sockets.h>
#include <string>

#define TAG "LogLocalSocket"
#define MAX 10
#define LOGD(TAG,...) if(1) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__);

struct Param {
    std::string cmd;
    int fd;
};

char logserver[16] = "detcserver";
std::string start = "start_logcat";//命令开始的标志
std::string stop = "stop_logcat";//命令结束的标志
int stopfd[MAX];

static void* execute_cmd(void *arg) {
    Param param = *(Param *)arg;
    std::string cmd = param.cmd;
    int receive_fd = param.fd;
    FILE *ptr = NULL;
    char buf_result[1024];
    if ((ptr = popen(cmd.c_str(), "r")) != NULL) {
        LOGD(TAG," shell  success\n");
        while(fgets(buf_result, sizeof(buf_result), ptr) != NULL) {
            LOGD(TAG," result = %s \n", buf_result);
            if (stopfd[receive_fd] == receive_fd) {//判断命令是否需要停止
                stopfd[receive_fd] = 0;
                break;
            }
            send(receive_fd, buf_result, strlen(buf_result), 0);
            memset(buf_result, 0, sizeof(buf_result));
        }
        pclose(ptr);
        ptr = NULL;
    } else {
        LOGD(TAG," shell  failed\n");
    }
    close(receive_fd);
    return ((void *)0);
}

static void* recever_thread_exe(void *arg) {
    int receive_fd = *(int *)arg;
    int numbytes = -1;;
    char buff_cmd[1024];
    memset(buff_cmd, 0, sizeof(buff_cmd));
    while((numbytes = recv(receive_fd, buff_cmd, sizeof(buff_cmd), 0)) != -1) {
        std::string cmd(buff_cmd);
        LOGD(TAG," cmd  = %s \n", cmd.c_str());
        if (cmd.find(start, 0) != std::string::npos) {//客户端传递的命令是否包含开始
            cmd = cmd.erase(0, start.length());
            LOGD(TAG," cmd start = %s \n", cmd.c_str());
            Param param;
            param.cmd = cmd;
            param.fd = receive_fd;
            pthread_t id;
            pthread_create(&id, NULL, execute_cmd, &param);
        } else if (cmd.find(stop, 0) != std::string::npos) {//客户端传递的命令是否包含停止
            //接收到停止命令
            stopfd[receive_fd] = receive_fd;
            break;
        }
    }
    LOGD(TAG,"recv %d\n", errno);
    return ((void *)0);
}

int main() {

    int socket_fd = -1;
    int receive_fd = -1;
    int result;

    struct sockaddr addr;
    socklen_t alen;
    alen = sizeof(addr);

    socket_fd = android_get_control_socket(logserver);
    if (socket_fd < 0) {
        LOGD(TAG,"Failed to get socket log_server  %s errno:%d\n", logserver, errno); 
        exit(-1);
    }
    LOGD(TAG,"android_get_control_socket success\n");


    result = listen(socket_fd, MAX);
    if (result < 0) {
        perror("listen\n");
        LOGD(TAG,"listen error\n");
        exit(-1);
    }
    LOGD(TAG,"listen success\n");
    while(1) {
        memset(&addr, 0, sizeof(addr));
        receive_fd= accept(socket_fd, &addr, &alen);
        LOGD(TAG,"Accept_fd %d\n",receive_fd);
        if (receive_fd < 0 ) {
            LOGD(TAG,"receive_fd %d\n",errno);
            perror("accept error");
            exit(-1);
        }

        fcntl(receive_fd, F_SETFD, FD_CLOEXEC);
        pthread_t id;
        if(pthread_create(&id, NULL, recever_thread_exe, &receive_fd) )
        {
            LOGD(TAG,"rece thread create fail\n");
        }
    }

    close(socket_fd);
    return 0;
}
