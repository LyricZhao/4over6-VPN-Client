// C++ backend of 4over6 VPN client
// 2020 Network Training, Tsinghua University
// Chenggang Zhao & Yuxian Gu

// Android Includes
# include <android/log.h>
# include <jni.h>

// Native C++
# include <cassert>
# include <cstdio>
# include <cstring>
# include <errno.h>
# include <fcntl.h>
# include <pthread.h>
# include <string>
# include <sys/stat.h>
# include <unistd.h>

// Networks
# include <arpa/inet.h>
# include <netdb.h>
# include <netinet/in.h>
# include <netinet/ip.h>
# include <netinet/tcp.h>
# include <sys/socket.h>

// Defines & Macros
# define debug(...) __android_log_print(ANDROID_LOG_DEBUG, __func__, __VA_ARGS__)
# define error(...) __android_log_print(ANDROID_LOG_ERROR, __func__, __VA_ARGS__)

typedef unsigned char u8;
typedef unsigned int u32;

// Parameters
# define DATA_MAX_LENGTH              4096
# define PRINT_BUFFER_LENGTH          128
# define REQUEST_LIMIT                3
# define RECV_CHECK_INTEVAL           100
# define SOCKET_TIMEOUT               4     // also IP request timeout

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
addrinfo *list;

// Counters
u32 time_connected, time_last_heartbeat, time_send_heartbeat;
u32 bytes_sent, bytes_recv, bytes_sent_sec, bytes_recv_sec;
volatile bool running = false, ip_requesting = false;
bool error_occured;

// Utilities - print pretty time and size
std::string pretty(u32 value, u32 scale, const char* *units, int m) {
  int count = 0;
  while (value > scale && count < m - 1) {
    value /= scale;
    count += 1;
  }
  char buffer[PRINT_BUFFER_LENGTH];
  sprintf(buffer, "%d %s", value, units[count]);
  return std::string(buffer);
}

std::string prettySize(u32 size) {
  static const char* units[5] = {"Bytes", "KBytes", "MBytes", "GBytes"};
  return pretty(size, 1024, units, 5);
}

std::string prettyTime(u32 time) {
  static const char* units[2] = {"s", "min(s)"};
  return pretty(time, 60, units, 2);
}

// Send raw
int send_raw(u8* ptr, u32 length) {
  // Already terminate
  if (!running && !ip_requesting) {
    return -1;
  }

  int sent = send(sockfd, ptr, length, 0);
  if (sent < length) {
    error("Failed to write raw sockets (%d/%d)", sent, length);
    return -1;
  }
  return sent;
}

// Receive raw
int recv_raw(u8 *buffer, u32 length) {
  int received = 0;
  // Note: there will be no auto shutdown because the protocol does not include an end signal, so the only way to stop is via heartbeat
  while ((running || ip_requesting) && (received < length)) {
    int single = recv(sockfd, buffer + received, length - received, 0);
    if (single <= 0) {
      if (ip_requesting) {
        debug("IP Request timeout");
        break;
      }
      usleep(RECV_CHECK_INTEVAL);
      continue;
    } else {
      // Read
      received += single;
    }
  }
  return received;
}

// Send heartbeat
int send_heartbeat() {
  return send_raw((u8 *) &heartbeat, heartbeat.length);
}

// Send IP request
int send_ip_request() {
  return send_raw((u8 *) &ip_request, ip_request.length);
}

// Waiting for a message
bool recv_message(Message &message) {
  int size;
  size = recv_raw((u8 *) &message, sizeof(u32));
  if (size < sizeof(u32) || (!running && !ip_requesting)) {
    return false;
  }

  size = recv_raw(((u8 *) &message) + sizeof(u32), message.length - sizeof(u32));
  return (size + sizeof(u32)) == message.length;
}

// Sender thread
void* send_thread(void *_) {
  Message message;
  while (running) { // 'running' is volatile
    memset(&message, 0, sizeof(Message));
    int length = read(tunfd, message.data, DATA_MAX_LENGTH);
    if (length > 0) {
      message.length = length + sizeof(u32) + sizeof(u8);
      message.type = NET_REQUEST;

      // debug("Sending from send_thread with length = %d", length);
      send_raw((u8*) &message, message.length);

      bytes_sent += message.length;
      bytes_sent_sec += message.length;
    }
  }
  debug("Sender thread ends");
  return nullptr;
}

// Receiver thread
void* recv_thread(void *_) {
  Message message;
  while (running) {
    // debug("recv_thread waiting for new message");
    if (!recv_message(message)) {
      running = false;
      break;
    }

    bytes_recv += message.length;
    bytes_recv_sec += message.length;
    if (message.type == NET_REPLY) {
      int length = message.length - sizeof(u32) - sizeof(u8);
      // debug("Received net reply with length = %d", message.length);
      if (length != write(tunfd, message.data, length)) {
        debug("System tunnel down");
        break;
      }
    } else if (message.type == HEARTBEAT) {
      time_last_heartbeat = time_connected;
      debug("Heartbeat received (time: %d)", time_last_heartbeat);
    } else {
      debug("Unknown type (%d) or IP reply packet received", message.type);
    }
  }
  debug("Recv thread ends");
  return nullptr;
}

void cleanup() {
  shutdown(sockfd, SHUT_RDWR);
  freeaddrinfo(list);
  sockfd = -1;
}

// APIs
// Tik-tok
extern "C" JNIEXPORT jstring JNICALL Java_com_lyricz_a4over6vpn_VPNService_tik(JNIEnv* env, jobject /* this */) {
  debug("Tik-tok");
  if (sockfd == -1 || !running) { // 'running' for UI delay
    return env -> NewStringUTF("");
  }

  ++ time_connected;
  if (time_connected - time_last_heartbeat > 60) {
    debug("Not receiving heartbeat for 60 seconds, terminate");
    error_occured = true;
    running = false;
    return env -> NewStringUTF("");
  }

  ++ time_send_heartbeat;
  if (time_send_heartbeat == 20) {
    time_send_heartbeat = 0;
    debug("Time up for 20s, sending heartbeat");
    send_heartbeat();
  }

  char str[PRINT_BUFFER_LENGTH];
  sprintf(str, "Sent: %s (%s/s)\nReceived: %s (%s/s)\nTime connected: %s\n",
    prettySize(bytes_sent).c_str(), prettySize(bytes_sent_sec).c_str(),
    prettySize(bytes_recv).c_str(), prettySize(bytes_recv_sec).c_str(),
    prettyTime(time_connected).c_str());

  bytes_sent_sec = bytes_recv_sec = 0;
  return env -> NewStringUTF(str);
}

// Handler system network in/out flow
extern "C" JNIEXPORT jboolean JNICALL Java_com_lyricz_a4over6vpn_VPNService_backend(JNIEnv* env, jobject /* this */, jint fd) {
  tunfd = fd;
  running = true;
  error_occured = false;

  // Send & receive thread
  pthread_t receiver, sender;
  pthread_create(&receiver, nullptr, recv_thread, nullptr);
  pthread_create(&sender, nullptr, send_thread, nullptr);

  // Waiting for terminate
  pthread_join(receiver, nullptr);
  pthread_join(sender, nullptr);

  // Terminate
  debug("Socket shutdown (normal case)");
  cleanup();
  debug("Backend thread quits");

  return (jboolean) error_occured;
}

// Apply for a global socket (addr can be a hostname)
extern "C" JNIEXPORT jint JNICALL Java_com_lyricz_a4over6vpn_VPNService_open(JNIEnv* env, jobject /* this */, jstring j_addr, jstring j_port) {
  // Cleanup
  bytes_recv = bytes_sent = 0;
  time_connected = 0;
  time_last_heartbeat = time_send_heartbeat = 0;
  bytes_sent_sec = bytes_recv_sec = 0;
  assert(sockfd == -1);

  const char* addr = env -> GetStringUTFChars(j_addr, 0);
  const char* port = env -> GetStringUTFChars(j_port, 0);

  addrinfo hint;
  memset(&hint, 0, sizeof(addrinfo));
  hint.ai_family = AF_UNSPEC;
  hint.ai_socktype = SOCK_STREAM;

  debug("Trying to connect %s (port: %s)", addr, port);
  if (getaddrinfo(addr, port, &hint, &list)) {
    return (jint) (-1);
  }

  for (addrinfo *ptr = list; ptr != nullptr; ptr = ptr -> ai_next) {
    debug("Creating socket at family@%d, type@%d, protocol@%d", ptr -> ai_family, ptr -> ai_socktype, ptr -> ai_protocol);
    sockfd = socket(ptr -> ai_family, ptr -> ai_socktype, ptr -> ai_protocol);
    if (sockfd < 0) {
      debug("socket() failed");
      continue;
    }

    u32 enable = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(u32));

    // Set timeout
    timeval timeout = {SOCKET_TIMEOUT, 0};
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    if (connect(sockfd, ptr -> ai_addr, ptr -> ai_addrlen) == 0) {
      debug("Success");
      sock_addr = ptr -> ai_addr;
      sock_len = ptr -> ai_addrlen;
      break;
    } else {
      debug("%s", strerror(errno));
      shutdown(sockfd, SHUT_RDWR);
      sockfd = -1;
      debug("connect() failed");
    }
  }

  debug("Open sockfd = %d\n", sockfd);
  if (sockfd == -1) {
    freeaddrinfo(list);
  }
  return (jint) sockfd;
}

// Apply for a VPN Address
extern "C" JNIEXPORT jstring JNICALL Java_com_lyricz_a4over6vpn_VPNService_request(JNIEnv* env, jobject /* this */) {
  if (sockfd == -1) {
      return env -> NewStringUTF("");
  }
  debug("Sending IP request");
  ip_requesting = true;
  send_ip_request();

  // Waiting for reply

  u32 times_try = 0;
  Message message;
  while (times_try < REQUEST_LIMIT) {
    debug("Waiting for IP reply");
    if (!recv_message(message)) {
      break;
    }

    if (message.type == IP_REPLY) {
      ip_requesting = false;
      debug("Received IP reply: %s", message.data);
      return env -> NewStringUTF((const char *) message.data);
    }

    debug("Not an IP reply");
    ++ times_try;
  }
  ip_requesting = false;

  debug("Socket shutdown (IP Request timeout)");
  cleanup();
  return env -> NewStringUTF("");
}

// Terminate all
extern "C" JNIEXPORT void JNICALL Java_com_lyricz_a4over6vpn_VPNService_terminate(JNIEnv* env, jobject /* this */) {
  debug("Terminate by API");
  running = false;
}
