#ifndef MIDFACESDK_MID_LOG_H
#define MIDFACESDK_MID_LOG_H

#include <android/log.h>
#include <string.h>
#include "src/Utils.h"

#define TAG "MClient"

#ifdef Windows
#define __FILENAME__ (strrchr(__FILE__, '\\') + 1) // Windows下文件目录层级是'\\'
#elif Linux
#define __FILENAME__ (strrchr(__FILE__, '/') + 1) // Linux下文件目录层级是'/'
#else
#define __FILENAME__ (strrchr(__FILE__, '/') + 1) // 默认使用这种方式
#endif

#define LOG_LEVEL 	0

#define VERBOSE		0
#define DEBUG  		1
#define INFO		2
#define WARN		3
#define ERROR		4

extern bool IS_DEBUG;

#define LOGV(FMT, ...) if (IS_DEBUG) __android_log_print(ANDROID_LOG_VERBOSE, TAG, "[%d*%s:%d:/**/%s]:" FMT,	\
         	 	 	 	 	 	 gettid(), basename(__FILENAME__), __LINE__, __FUNCTION__, ## __VA_ARGS__)

#define LOGD(FMT, ...) if (IS_DEBUG) __android_log_print(ANDROID_LOG_DEBUG, TAG, "[%d*%s:%d:/**/%s]:" FMT,	\
         	 	 	 	 	 	 gettid(), basename(__FILENAME__), __LINE__, __FUNCTION__, ## __VA_ARGS__)

#define LOGI(FMT, ...) if (IS_DEBUG) __android_log_print(ANDROID_LOG_INFO, TAG, "[%d*%s:%d:/**/%s]:" FMT,	\
         	 	 	 	 	 	 gettid(), basename(__FILENAME__), __LINE__, __FUNCTION__, ## __VA_ARGS__)

#define LOGW(FMT, ...) if (LOG_LEVEL <= WARN) __android_log_print(ANDROID_LOG_WARN, TAG, "[%d*%s:%d:/**/%s]:" FMT,	\
         	 	 	 	 	 	 gettid(), basename(__FILENAME__), __LINE__, __FUNCTION__, ## __VA_ARGS__)

#define LOGE(FMT, ...) if (LOG_LEVEL <= ERROR) __android_log_print(ANDROID_LOG_ERROR, TAG, "[%d*%s:%d:/**/%s]:" FMT,	\
         	 	 	 	 	 	 gettid(), basename(__FILENAME__), __LINE__, __FUNCTION__, ## __VA_ARGS__)

#endif


