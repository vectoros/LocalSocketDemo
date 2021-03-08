#include <jni.h>
#include <string>
#include <vector>

#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/un.h>
#include <unistd.h>
#include <pthread.h>
#include <android/log.h>


#define TAG "LocalSocketJni"

#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

struct SocketContext{
public:
    int fd;
    int controlFd;
    int numClients;
    int epollFd;
    std::vector<int> clients;
    pthread_t tid;
};

const char *sSocketName = "socket_demo";
const int sMaxClients = 10;
SocketContext gContext;
void* serverThread(void* arg){
    ALOGD("%s started.", __FUNCTION__ );
    SocketContext* pContext = (SocketContext*)arg;

    epoll_ctl(pContext->epollFd, EPOLL_CTL_ADD, pContext->fd, NULL);

    for(;;){
        int pollCount = pContext->numClients + 1; // no control interface yet
        epoll_event epollEvent[pollCount];

        int recvNum = epoll_wait(pContext->epollFd, epollEvent, pollCount, 1000);
        for(int i = 0; i < recvNum; i++){
            if(epollEvent[i].data.fd == pContext->fd){
                // new client connect.
                if(epollEvent[i].events & EPOLLIN){
                    int clientFd = accept(pContext->fd, NULL, NULL);
                    pContext->numClients ++;
                    pContext->clients.push_back(clientFd);
                    ALOGD("%s new client[%d] connected.", __FUNCTION__ , epollEvent[i].data.fd);
                }
            }else{
                // recv client data.
                ALOGD("%s read data from client[%d].", __FUNCTION__ , epollEvent[i].data.fd);
            }
        }
    }
    ALOGD("%s done.", __FUNCTION__ );
}


static void init(){
    ALOGD("%s started.", __FUNCTION__ );
    gContext.numClients = 0;
    gContext.fd = -1;
    gContext.controlFd = -1;
    gContext.epollFd = epoll_create(sMaxClients);

    sockaddr_un un;
    if((gContext.fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0){
        ALOGE("%s create socket server failed.", __FUNCTION__ );
        return;
    }

    unlink(sSocketName);
    memset(&un, 0, sizeof(sockaddr_un));
    un.sun_family = AF_UNIX;
    strcpy(un.sun_path, sSocketName);

    int len;
    len = offsetof(struct sockaddr_un, sun_path) + strlen(sSocketName);
    do{
        if(bind(gContext.fd, (struct sockaddr*)&un, len) < 0){
            break;
        }

        if(listen(gContext.fd, sMaxClients) < 0){
            break;
        }

        pthread_create(&gContext.tid, NULL, serverThread, (void*)&gContext);
        ALOGD("%s socket server started.", __FUNCTION__ );
        return;
    }while (0);

    close(gContext.fd);
    ALOGD("%s not started.", __FUNCTION__ );
}


static void stop(){
    ALOGD("%s start.", __FUNCTION__ );
    if(gContext.fd > 0){
        close(gContext.fd);
    }

    for(int i = 0; i < gContext.numClients; i++){
        close(gContext.clients[i]);
    }
    ALOGD("%s done.", __FUNCTION__ );
}



extern "C" JNIEXPORT jstring JNICALL
Java_com_vectoros_localsocketdemo_MainActivity_stringFromJNI(
        JNIEnv* env,
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
}

extern "C"
JNIEXPORT void JNICALL
Java_com_vectoros_localsocketdemo_MainActivity_responseData(JNIEnv *env, jobject thiz, jint msg) {
    // TODO: implement responseData()
}