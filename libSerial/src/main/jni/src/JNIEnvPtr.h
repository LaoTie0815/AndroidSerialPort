//
// Created by huangtiebing_vendor on 2021/3/7.
//

#include "../mtc_log.h"
#include <unistd.h>
#include <libgen.h>

#ifndef MY_APPLICATION_JNIENVPTR_H
#define MY_APPLICATION_JNIENVPTR_H
class JNIEnvPtr {
public:
    JNIEnvPtr(JavaVM *m_javavm);
    ~JNIEnvPtr();
    void detachCurrentThread();
    JNIEnv* operator->() {
        return env_;
    }

private:
    JNIEnvPtr(const JNIEnvPtr&) = delete;
    JNIEnvPtr& operator=(const JNIEnvPtr&) = delete;

private:
    JNIEnv* env_;
    bool need_detach_;
    JavaVM *m_javavm = nullptr;
};

#endif //MY_APPLICATION_JNIENVPTR_H
