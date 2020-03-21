// C++ backend of 4over6 VPN client

// Android Includes
# include <android/log.h>
# include <jni.h>

// Native C++
# include <string>

// Defines & Macros
# define TAG "Backend"
# define debug(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
# define error(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

typedef unsigned char u8;
typedef unsigned int u32;

// Parameters
# define DATA_MAX_LENGTH 4096

struct Message {
  u32 length;
  u8 type;
  u8 data[DATA_MAX_LENGTH];
};

extern "C" JNIEXPORT void JNICALL
Java_com_lyricz_a4over6vpn_MainActivity_createBackendTunnel(
        JNIEnv* env,
        jobject /* this */) {

}

extern "C" JNIEXPORT void JNICALL
Java_com_lyricz_a4over6vpn_MainActivity_terminateBackendTunnel(
        JNIEnv* env,
        jobject /* this */) {
  debug("Unimplemented");
}
