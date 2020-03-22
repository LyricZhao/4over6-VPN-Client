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
# include <netdb.h>
# include <sys/socket.h>

// Defines & Macros
# define debug(...) __android_log_print(ANDROID_LOG_DEBUG, __func__, __VA_ARGS__)
# define error(...) __android_log_print(ANDROID_LOG_ERROR, __func__, __VA_ARGS__)

typedef unsigned char u8;
typedef unsigned int u32;

// Parameters
# define DATA_MAX_LENGTH              4096
# define PRINT_BUFFER_LENGTH          128
# define REQUEST_TIMEOUT_USEC         2000000
# define REQUEST_CHECK_INTERVAL_USEC  5000
# define RECV_CHECK_INTEVAL           100
# define RECONNECT_LIMIT              30

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

// File descriptor & socket info
int sockfd = -1, tunfd = -1;
sockaddr* sock_addr; socklen_t sock_len;

// Counters
u32 time_connected, time_last_heartbeat, time_send_heartbeat;
u32 bytes_sent, bytes_recv;
volatile bool running = false;

int send_raw(void* ptr, u32 length) {
  assert(sockfd != -1);
  int sent = send(sockfd, ptr, length, 0);
  if (sent < length) {
    error("Failed to write raw sockets");
    return -1;
  }
  return sent;
}

int recv_raw(u8 *buffer, u32 length) {
  int received = 0;
  int time_reconnect = 0;
  while (received < length) {
    int single = recv(sockfd, buffer + received, length - received, 0);
    if (single < 0) {
      // Reconnect
      usleep(RECV_CHECK_INTEVAL);
      ++ time_reconnect;
      if (time_reconnect > RECONNECT_LIMIT) {
        debug("Too many reconnects, lost connection");
        break;
      }
      debug("Reconnecting");
      connect(sockfd, sock_addr, sock_len);
    } else if (single == 0) {
      // Not arrived
      usleep(RECV_CHECK_INTEVAL);
      time_reconnect = 0;
      continue;
    } else {
      // Read
      time_reconnect = 0;
      received += single;
    }
  }
  return received;
}

int send_heartbeat() {
  return send_raw((void *) &heartbeat, heartbeat.length);
}

int send_ip_request() {
  return send_raw((void *) &ip_request, ip_request.length);
}

bool recv_message(Message &message) {
  int size;
  size = recv_raw(sockfd, (void *) &message, sizeof(u32));
  if (size < sizeof(u32)) {
    return false;
  }

  size = recv_raw(sockfd, (void *) &packet + sizeof(u32), message.length - sizeof(u32));
  return (size + sizeof(u32)) == message.length;
}

void send_thread() {
  Message message;
  while (running) { // 'running' is volatile
    memset(&message, 0, sizeof(message));
    u32 length = read(tunfd, message.data, DATA_MAX_LENGTH);
    if (length > 0) {
      message.length = length + sizeof(u32) + sizeof(u8);
      message.type = NET_REQUEST;

      send_raw(sockfd, (void*) &message, message.length);

      bytes_sent += message.length;
    }
  }
}

void recv_thread() {
  Message message;
  while (running) {
    if (!recv_message(message)) {
      break;
    }

    switch (message.type) {
      case NET_REPLY:
        int length = message.length - sizeof(u32) - sizeof(u8);
        if (length != write(tunfd, message.data, length)) {
          debug("System tunnel down");
          goto end;
        }
        bytes_recv += message.length;
        break;
      case HEARTBEAT:
        time_last_heartbeat = time_connected;
        debug("Heartbeat received (time: %d)", time_last_heartbeat);
        break;
      default:
        debug("Unknown type (%d) packet received", message.type);
        break;
    }
  }
end:
}

// APIs
// Tik-tok
extern "C" JNIEXPORT jstring JNICALL Java_com_lyricz_a4over6vpn_VPNService_tik(JNIEnv* env, jobject /* this */) {
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

// Handler system network in/out flow
extern "C" JNIEXPORT void JNICALL Java_com_lyricz_a4over6vpn_VPNService_backend(JNIEnv* env, jobject /* this */, jint fd) {
  tunfd = fd;
  running = true;

  // Send & receive thread
  pthread_t receiver, sender;
  pthread_create(&receiver, nullptr, recv_thread, nullptr);
  pthread_create(&sender, nullptr, send_thread, nullptr);

  // Waiting for terminate
  pthread_join(receiver, nullptr);
  pthread_join(sender, nullptr);

  close(sockfd);
  sockfd = -1;
  tunfd = -1;
}

// Apply for a global socket (addr can be a hostname)
extern "C" JNIEXPORT jint JNICALL Java_com_lyricz_a4over6vpn_VPNService_open(JNIEnv* env, jobject /* this */, jstring j_addr, jstring j_port) {
  const char* addr = env -> GetStringUTFChars(j_addr, 0);
  const char* port = env -> GetStringUTFChars(j_port, 0);

  addrinfo hint, *list;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  debug("Trying to connect %s (port: %s)", addr, port);
  if (getaddrinfo(addr, port, &hint, &list)) {
    return (jint) (-1);
  }

  for (addinfo *ptr = list; ptr != nullptr; ptr = ptr -> ai_next) {
    debug("Creating socket at family@%d, type@%d, protocol@%d", ptr -> ai_family, ptr -> ai_socktype, ptr -> ai_protocol);
    sockfd = socket(ptr -> ai_family, ptr -> ai_socktype, ptr -> ai_protocol);
    if (sockfd < 0) {
      debug("socket() failed");
      continue;
    }

    u32 enable = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(u32));
    if (connect(sockfd, ptr -> ai_addr, ptr -> ai_addrlen)) {
      debug("Success");
      sock_addr = ptr -> ai_addr;
      sock_len = ptr -> ai_addrlen;
      break;
    } else {
      debug("connect() failed");
    }
  }

  sockfd = sockfd < 0 ? -1 : sockfd;
  return (jint) sockfd;
}

// Apply for a VPN Address
extern "C" JNIEXPORT jstring JNICALL Java_com_lyricz_a4over6vpn_VPNService_request(JNIEnv* env, jobject /* this */) {
  if (sockfd == -1) {
    goto failed;
  }
  send_ip_request();

  // Waiting for reply
  u32 elapsed = 0;
  Message message;
  while (elapsed < REQUEST_TIMEOUT_USEC) {
    if (!recv_message(message)) {
      goto failed;
    }

    if (message.type == IP_REPLY) {
      debug("Received IP reply: %s", message.data);
      return env -> NewStringUTF(message.data);
    }

    usleep(REQUEST_CHECK_INTERVAL_USEC);
    elapsed += REQUEST_CHECK_INTERVAL_USEC;
  }

failed:
  sockfd = -1;
  return env -> NewStringUTF("");
}

// Terminate all
extern "C" JNIEXPORT void JNICALL Java_com_lyricz_a4over6vpn_VPNService_terminate(JNIEnv* env, jobject /* this */) {
  if (sockfd == -1) return;

  assert(running);
  running = false;
}
