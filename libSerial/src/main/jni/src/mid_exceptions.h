#ifndef MIDFACESDK_MID_EXCEPTIONS_H_H
#define MIDFACESDK_MID_EXCEPTIONS_H_H

#include <jni.h>


extern jclass jRuntimeException_class;

void safeThrowJavaException(JNIEnv *, jclass, const char *);

#define THROW_AND_RETURN_IF(condition, message)                         \
  do {                                                                  \
    if (condition) {                                                    \
      safeThrowJavaException(                                           \
          env,                                                          \
          jRuntimeException_class,                                      \
          message);                                                     \
      return;                                                           \
    }                                                                   \
  } while (0)

#define THROW_AND_RETURNVAL_IF(condition, message, return_value)        \
  do {                                                                  \
  if (condition) {                                                      \
      safeThrowJavaException(                                           \
          env,                                                          \
          jRuntimeException_class,                                      \
          message);                                                     \
      return return_value;                                              \
    }                                                                   \
  } while (0)

#define RETURN_IF_EXCEPTION_PENDING                                     \
  do {                                                                  \
    if (env->ExceptionCheck()) {                                        \
      return;                                                           \
    }                                                                   \
  } while (0)

#define RETURNVAL_IF_EXCEPTION_PENDING(return_value)                    \
  do {                                                                  \
    if (env->ExceptionCheck()) {                                        \
      return return_value;                                              \
    }                                                                   \
  } while (0)

#endif //MIDFACESDK_MID_EXCEPTIONS_H_H
