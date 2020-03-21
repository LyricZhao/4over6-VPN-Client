// C++ backend of 4over6 VPN client

// Android Includes
# include <android/log.h>
# include <jni.h>

// Native C++
# include <string>

// Networks
# include <sys/socket.h>

// Defines & Macros
# define debug(...) __android_log_print(ANDROID_LOG_DEBUG, __func__, __VA_ARGS__)
# define error(...) __android_log_print(ANDROID_LOG_ERROR, __func__, __VA_ARGS__)

typedef unsigned char u8;
typedef unsigned int u32;

// Parameters
# define DATA_MAX_LENGTH 4096

# define IP_REQUEST   100
# define IP_REPLY     101
# define NET_REQUEST  102
# define NET_REPLY    103
# define HEARTBEAT    104

struct Message {
  u32 length; // includes 'length', 'type' and 'data'
  u8 type;
  u8 data[DATA_MAX_LENGTH];
};

const Message ip_request = {sizeof(u32) + sizeof(u8), IP_REQUEST};
const Message heartbeat = {sizeof(u32) + sizeof(u8), HEARTBEAT};

int send_raw(int sockfd, void* ptr, u32 length) {
  int sent = send(sockfd, ptr, length, 0);
  if (sent < length) {
    error("Failed to write raw sockets");
    return -1;
  }
  return sent;
}

int send_heartbeat(int sockfd) {
  return send_raw(sockfd, (void *) &heartbeat, heartbeat.length);
}

int send_ip_request(int sockfd) {
  return send_raw(sockfd, (void *) &ip_request, ip_request.length);
}

// APIs
extern "C" JNIEXPORT void JNICALL
Java_com_lyricz_a4over6vpn_MainActivity_createBackendTunnel(
        JNIEnv* env,
        jobject /* this */) {
  debug("Backend starts running");
}

extern "C" JNIEXPORT void JNICALL
Java_com_lyricz_a4over6vpn_MainActivity_terminateBackendTunnel(
        JNIEnv* env,
        jobject /* this */) {
  debug("Unimplemented");
}
