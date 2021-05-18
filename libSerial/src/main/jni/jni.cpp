#include <jni.h>
#include <string>
#include <unistd.h>
#include <libgen.h>
#include <mid_exceptions.h>
#include "src/mtc.h"
#include "src/definition.h"
#include "mtc_log.h"

static JavaVM *gJavaVM;
jobject gJavaObj;
static bool g_isInited = false;
const short g_cmd_buf = 256;
#define MTC_UART_PATH    "/dev/ttyACM0"

jint registerNativeMethods(JNIEnv *env);

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    gJavaVM = vm;
    if ((vm)->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    THROW_AND_RETURNVAL_IF(
            registerNativeMethods(env) == JNI_ERR,
            "Could not register registerNativeMethods methods",
            JNI_ERR);

    return JNI_VERSION_1_6;
}


std::string jni_mtc_handle_at(const char *cmd, unsigned int cmdLength, int msecond) {
    //swith to at mode.
    int ret_at = MTC::getInstance().atMode();
    LOGD("AT command:%s, reset at mode: %d", cmd, ret_at);
    if (ret_at != 0)
        return nullptr;
    unsigned char *data = nullptr;
    ret_at = MTC::getInstance().atHandle(cmd, cmdLength, &data, msecond);
    if (MTC::getInstance().dataMode() == 0)
        LOGD("back data mode success");
        else LOGD("back data mode fail");
    std::string resp;
    if (data != nullptr) {
        resp = std::string((char *) data);
        free(data);
    } else
        nullptr;
    return resp;
}


static jboolean
mRegisterCallback(JNIEnv *env, jobject instance, jobject callback) {
    if (callback == nullptr) {
        return MTC::getInstance().destroyCallback();
    } else {
        return MTC::getInstance().initCallback(env, callback);
    }
}

bool IS_DEBUG = false;

static void mSetIsDebug(JNIEnv *env, jclass clazz, jboolean is_debug) {
    IS_DEBUG = (bool) is_debug;
}


static jint
mInit(JNIEnv *env, jobject instance, jstring jpath, jint jSpeed, jint jStopBits, jint jDataBits, jint jParity) {
    const char *path = env->GetStringUTFChars(jpath, nullptr);
    if (jpath == nullptr || access(path, F_OK) != 0)
        return ERR_CANNOT_OPEN;

    LOGD("mInit");
    int ret = MTC::getInstance().init(MTC_MODE_UART_CLIENT, path, jSpeed, jStopBits, jDataBits, jParity);
    if (ret == 0) {
        LOGD("%s %d:open success.", __FUNCTION__, __LINE__);
        g_isInited = true;
    } else LOGE("not init.ret is:%d", ret);
    env->ReleaseStringUTFChars(jpath, path);
    return ret;
}

static void
mDestroy(JNIEnv *env, jobject instance) {
    MTC::getInstance().destroy();
}


static jint
mPing(JNIEnv *env, jobject instance) {
    char echoData[16] = "ping_test";
    int ret = MTC::getInstance().ping(echoData, strlen(echoData));
    return ret;
}

static jstring
mHandleAt(JNIEnv *env, jobject instance, jstring jAtCommand) {
    const char *cmd = env->GetStringUTFChars(jAtCommand, JNI_FALSE);
    jstring jResp = nullptr;
    std::string ret = jni_mtc_handle_at(cmd, strlen(cmd), 1500);
    if (ret.empty())
        return nullptr;
    jResp = env->NewStringUTF(ret.c_str());
    env->ReleaseStringUTFChars(jAtCommand, cmd);
    return jResp;
}


jint registerNativeMethods(JNIEnv *env) {

    JNINativeMethod gNativeAPIMethods[] = {
            {"mSetIsDebug",                 "(Z)V",                                          (void *) mSetIsDebug},
            {"mInit",                       "(Ljava/lang/String;IIII)I",                         (void *) mInit},
            {"mPing",                       "()I",                                           (void *) mPing},
            {"mDestroy",                    "()V",                                           (void *) mDestroy},
            {"mHandleAt",                   "(Ljava/lang/String;)Ljava/lang/String;",        (void *) mHandleAt},
            {"mRegisterCallback",           "(Lcom/sensetime/mtc/IReceiveCallback;)Z",       (void *) mRegisterCallback},
    };

    jint nArraySize = sizeof(gNativeAPIMethods) / sizeof((gNativeAPIMethods)[0]);

    jclass chunk_class = env->FindClass("com/sensetime/mtc/MClient");
    if (!chunk_class) {
        return JNI_ERR;
    }

    jint result = env->RegisterNatives(chunk_class, gNativeAPIMethods, nArraySize);
    if (result != JNI_OK) {
        return JNI_ERR;
    }

    return JNI_OK;
}
