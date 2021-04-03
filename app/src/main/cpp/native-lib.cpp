#include <jni.h>
#include <string>
#include <vector>

#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/un.h>
#include <unistd.h>
#include <pthread.h>
#include <android/log.h>
#include "sockets.h"


#define TAG "LocalSocketJni"

#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

enum {
    CMD_QUIT = 0,
    CMD_START = 1,
    CMD_STOP = 2
};

struct SocketContext {
public:
    int fd;
    int numClients;
    int epollFd;
    int controlFd[2];
    bool init;
    std::vector<int> clients;
    pthread_t tid;
};

static int
epoll_register(int epoll_fd, int fd) {
    struct epoll_event ev;
    int ret, flags;
    /* important: make the fd non-blocking */
    flags = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    ev.events = EPOLLIN;
    ev.data.fd = fd;
    do {
        ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
    } while (ret < 0 && errno == EINTR);
    return ret;
}


static int
epoll_deregister(int epoll_fd, int fd) {
    int ret;
    do {
        ret = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    } while (ret < 0 && errno == EINTR);
    return ret;
}

static void stop();

static void init();

const char *sSocketName = "socket_demo";
const int sMaxClients = 10;
SocketContext gContext;

void *serverThread(void *arg) {
    ALOGD("%s started.", __FUNCTION__);
    SocketContext *pContext = (SocketContext *) arg;
    int serverFd = pContext->fd;
    int controlFd = pContext->controlFd[1];
    bool start = false;
    epoll_register(pContext->epollFd, serverFd);
    epoll_register(pContext->epollFd, controlFd);
    epoll_ctl(pContext->epollFd, EPOLL_CTL_ADD, serverFd, NULL);

    for (;;) {
        if (!pContext->init) {
            ALOGD("%s exit thread ....", __FUNCTION__);
            break;
        }
        int pollCount = pContext->numClients + 1; // no control interface yet
        epoll_event epollEvent[pollCount];
        ALOGD("%s wait epoll events", __FUNCTION__);
        int recvNum = epoll_wait(pContext->epollFd, epollEvent, pollCount, 1000);
        for (int i = 0; i < recvNum; i++) {
            int fd = epollEvent[i].data.fd;
            ALOGD("%s receive %d events, process fd[%d] event:", __FUNCTION__, recvNum, fd);
            if (fd == serverFd) {
                // new client connect.
                if (epollEvent[i].events & EPOLLIN) {
                    int clientFd = accept(serverFd, NULL, NULL);
                    pContext->numClients++;
                    pContext->clients.push_back(clientFd);
                    epoll_register(pContext->epollFd, clientFd);
                    ALOGD("%s new client[%d] connected.", __FUNCTION__, clientFd);
                }
            } else if (fd == controlFd) {
                // recv control cmd.
                char cmd = 255;
                int ret;
                do {
                    ret = read(fd, &cmd, 1);
                } while (ret < 0 && errno == EINTR);
                ALOGD("%s recv control cmd: %d", __FUNCTION__, cmd);
                if (cmd == CMD_QUIT) {
                    pContext->init = false;
                    break;
                } else if (cmd == CMD_START) {
                    if (!start) {
                        start = true;
                    }
                } else {
                    if (start) {
                        start = false;
                    }
                }
                if (!pContext->clients.empty()) {
                    for (int i = 0; i < pContext->clients.size(); i++) {
                        do {
                            ret = write(pContext->clients[i], &cmd, 1);
                        } while (ret < 0 && errno == EINTR);
                        ALOGD("%s write cmd[%d] to client[%d]", __FUNCTION__, cmd,
                              pContext->clients[i]);
                        if (ret == 0) {
                            ALOGE("%s client[%d] write return 0, socket closed.", __FUNCTION__,
                                  pContext->clients[i]);
                            close(pContext->clients[i]);
                            pContext->numClients--;
                            epoll_deregister(pContext->epollFd, pContext->clients[i]);
                        }
                    }
                }
            } else {
                // recv client data.
                ALOGD("%s read data from client[%d].", __FUNCTION__, epollEvent[i].data.fd);
            }
        }
    }
    void *dummy = nullptr;
    ALOGD("%s thread done, exit.", __FUNCTION__);
    return dummy;
}


static void init() {
    ALOGD("%s started.", __FUNCTION__);
    gContext.numClients = 0;
    gContext.fd = -1;
    gContext.controlFd[0] = -1; // controller write
    gContext.controlFd[1] = -1; // thread recv
    gContext.epollFd = epoll_create(sMaxClients + 2);
    gContext.init = true;
    if ((gContext.fd = socket_local_server(sSocketName,
                                           ANDROID_SOCKET_NAMESPACE_ABSTRACT,
                                           SOCK_STREAM)) < 0) {
        ALOGE("%s create socket server failed: %s", __FUNCTION__, strerror(errno));
        return;
    }
    do {
        if (listen(gContext.fd, sMaxClients) < 0) {
            ALOGE("%s listen socket failed: %s", __FUNCTION__, strerror(errno));
            break;
        }

        if (socketpair(AF_LOCAL, SOCK_STREAM, 0, gContext.controlFd) < 0) {
            ALOGE("%s could not create thread control socket pair: %s", __FUNCTION__,
                  strerror(errno));
            break;
        }

        pthread_create(&gContext.tid, NULL, serverThread, (void *) &gContext);
        ALOGD("%s socket server started.", __FUNCTION__);
        return;
    } while (0);

    close(gContext.fd);
    ALOGD("%s not started.", __FUNCTION__);
}


static void stop() {
    ALOGD("%s start.", __FUNCTION__);
    char cmd = CMD_QUIT;
    write(gContext.controlFd[0], &cmd, 1);
    void *dummy;
    pthread_join(gContext.tid, &dummy);

    if (gContext.fd > 0) {
        close(gContext.fd);
    }

    for (int i = 0; i < gContext.numClients; i++) {
        if (close(gContext.clients[i]) > 0) {
            close(gContext.clients[i]);
        }
    }
    gContext.numClients = 0;
    gContext.clients.clear();
    ALOGD("%s done.", __FUNCTION__);
}


extern "C" JNIEXPORT jstring JNICALL
Java_com_vectoros_localsocketdemo_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

extern "C"
JNIEXPORT void JNICALL
Java_com_vectoros_localsocketdemo_MainActivity_startServer(JNIEnv *env, jobject thiz) {
    // TODO: implement startServer()
    init();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_vectoros_localsocketdemo_MainActivity_stopServer(JNIEnv *env, jobject thiz) {
    // TODO: implement stopServer()
    stop();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_vectoros_localsocketdemo_MainActivity_connectToServer(JNIEnv *env, jobject thiz) {
    // TODO: implement connectToServer()
}

extern "C"
JNIEXPORT void JNICALL
Java_com_vectoros_localsocketdemo_MainActivity_serverCommand(JNIEnv *env, jobject thiz, jint cmd) {
    // TODO: implement serverCommand()
    write(gContext.controlFd[0], &cmd, 1);
    ALOGD("%s server command: ", __FUNCTION__, cmd);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_vectoros_localsocketdemo_MainActivity_responseData(JNIEnv *env, jobject thiz, jint msg) {
    // TODO: implement responseData()
}