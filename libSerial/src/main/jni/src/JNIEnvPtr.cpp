//
// Created by huangtiebing_vendor on 2021/3/7.
//

#include <jni.h>
#include "JNIEnvPtr.h"

JNIEnvPtr::JNIEnvPtr(JavaVM *m_javavm) : env_{nullptr}, need_detach_{false} {
    this->m_javavm = m_javavm;
    if (m_javavm != nullptr && m_javavm->GetEnv((void **) &env_, JNI_VERSION_1_6) == JNI_EDETACHED) {
        m_javavm->AttachCurrentThread(&env_, nullptr);
        need_detach_ = true;
    } else {
        LOGE("can not AttachCurrentThread");
    }
    LOGD("env_ is null:%d", env_ == nullptr);
}

void JNIEnvPtr::detachCurrentThread() {
    LOGD("need_detach_:%d", need_detach_);
    if (need_detach_) {
        m_javavm->DetachCurrentThread();
        this->m_javavm = nullptr;
        this->env_ = nullptr;
        need_detach_ = false;
    }
}

JNIEnvPtr::~JNIEnvPtr() {
    LOGD("need_detach_:%d", need_detach_);
    if (need_detach_) {
        m_javavm->DetachCurrentThread();
        this->m_javavm = nullptr;
        this->env_ = nullptr;
        need_detach_ = false;
    }
}


