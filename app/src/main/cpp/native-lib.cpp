// C++ backend of 4over6 VPN client

// Android Includes
# include <android/log.h>
# include <jni.h>

// Native C++
# include <cassert>
# include <cstdio>
# include <cstring>
# include <string>

// Networks
# include <sys/socket.h>

// Defines & Macros
# define debug(...) __android_log_print(ANDROID_LOG_DEBUG, __func__, __VA_ARGS__)
# define error(...) __android_log_print(ANDROID_LOG_ERROR, __func__, __VA_ARGS__)

typedef unsigned char u8;
typedef unsigned int u32;

// Parameters
# define DATA_MAX_LENGTH      4096
# define PRINT_BUFFER_LENGTH  128

# define IP_REQUEST   100
# define IP_REPLY     101
# define NET_REQUEST  102
# define NET_REPLY    103
# define HEARTBEAT    104

// Message
struct Message {
  u32 length; // includes 'length', 'type' and 'data'
  u8 type;
  u8 data[DATA_MAX_LENGTH];
};

// Constant message
const Message ip_request = {sizeof(u32) + sizeof(u8), IP_REQUEST};
const Message heartbeat = {sizeof(u32) + sizeof(u8), HEARTBEAT};

// Socket to server
int sockfd = -1;

// Counters
u32 time_connected, time_last_heartbeat, time_send_heartbeat;
u32 bytes_sent, bytes_recv;

int send_raw(void* ptr, u32 length) {
  assert(sockfd != -1);
  int sent = send(sockfd, ptr, length, 0);
  if (sent < length) {
    error("Failed to write raw sockets");
    return -1;
  }
  return sent;
}

int send_heartbeat() {
  return send_raw((void *) &heartbeat, heartbeat.length);
}

int send_ip_request() {
  return send_raw((void *) &ip_request, ip_request.length);
}

void terminate() {
  sockfd = -1;
}

// APIs
extern "C" JNIEXPORT jstring JNICALL Java_com_lyricz_a4over6vpn_MainActivity_tik(JNIEnv* env, jobject /* this */) {
  ++ time_connected;
  if (time_connected - time_last_heartbeat > 60) {
    terminate();
    return env -> NewStringUTF("");
  }

  ++ time_send_heartbeat;
  if (time_send_heartbeat == 20) {
    time_send_heartbeat = 0;
    send_heartbeat();
  }

  char str[PRINT_BUFFER_LENGTH];
  if (time_connected >= 60) {
    sprintf(str, "Sent: %d bytes\nReceived: %d bytes\nTime connected: %d mins", bytes_sent, bytes_recv, time_connected / 60);
  } else {
    sprintf(str, "Sent: %d bytes\nReceived: %d bytes\nTime connected: %d s", bytes_sent, bytes_recv, time_connected);
  }
  return env -> NewStringUTF(str);
}

extern "C" JNIEXPORT jint JNICALL Java_com_lyricz_a4over6vpn_MainActivity_requestSocket(JNIEnv* env, jobject /* this */) {
  return (jint) 0;
}

extern "C" JNIEXPORT jstring JNICALL Java_com_lyricz_a4over6vpn_MainActivity_requestAddress(JNIEnv* env, jobject /* this */) {
  return env -> NewStringUTF("");
}

extern "C" JNIEXPORT void JNICALL Java_com_lyricz_a4over6vpn_MainActivity_terminateTunnel(JNIEnv* env, jobject /* this */) {
  if (sockfd == -1) return;

  terminate();
}
