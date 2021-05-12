#include "mid_exceptions.h"


jclass jRuntimeException_class;

void safeThrowJavaException(JNIEnv *env, jclass exceptionCls, const char *msg) {
    if (!env->ExceptionCheck()) {
        env->ThrowNew(exceptionCls, msg);
    }
}