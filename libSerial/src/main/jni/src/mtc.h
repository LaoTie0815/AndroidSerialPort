#ifndef MTC_H
#define MTC_H

#include <jni.h>
#include <string>
#include <string.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <vector>
#include "definition.h"
#include "ringbuffer.h"
#include "tlv.h"
#include "serial.h"
#include "JNIEnvPtr.h"

typedef struct MTC_BUFFER
{
    unsigned char *data=nullptr;
    size_t read_len=0;
    void clean(){
        if(this->data){
            free(this->data);
            this->data = nullptr;
            read_len = 0;
        }
    }
}MTC_BUFFER;

typedef struct MTC_CONFIG{
    Serial *serial=nullptr;
    //SOCKET *sokt;
    int mode;           //socket&uart client ||socket&uart server
}MTC_CONFIG;

typedef struct MTC_RECEIVER{
    std::mutex mutex;
    std::condition_variable cond;
    MTC_BUFFER buffer;
    bool isRouseSerial = false;  //唤醒串口标记
    int mode = MTC_RECEIVER_MODE_AT;           // AT   || tlv data
    int status = MTC_RECEIVER_STATE_DIE;         // running || not run.
    int read_invalid_data_count;
    int clean_serial_buffer_threshold;
}MTC_RECEIVER;

typedef struct MTC_IMAGE
{
    int image_length = 0;
    char *image = nullptr;
    int id_length = 0;
    char id[MTC_IMAGE_ID_MAX_LENGTH];
}MTC_IMAGE;

typedef struct MTC_JSON
{
    unsigned int json_length = 0;
    unsigned char *json = nullptr;
}MTC_JSON;


typedef struct MTC_FEATURE{ //通过特征值导入库.
    char ret;
    unsigned int feature_length = 0;
    unsigned char *feature = nullptr;
    unsigned int id_length = 0;
    char id[MTC_IMAGE_ID_MAX_LENGTH];
};

typedef struct ID_LIST{ //通过特征值导入库.
    char ret;
    unsigned int is_end = 0;
    unsigned int position = 0;
    int length = 0;
    char *values = nullptr;
};

typedef struct MTC_TLVS
{
    struct MTC_TLVS *next = nullptr;
    unsigned short tag = 0;
    unsigned int length = 0;
    unsigned char value[1] = {0};
}__attribute__((packed)) MTC_TLVS; //取消对齐

typedef struct Fields_IdataCallback{
    jmethodID onTracking;
    jmethodID onVerify;
    jmethodID onPhotoData;
    jmethodID onFeatureData;
    jmethodID onQrCodeData;
    jmethodID onDisconnect;

    unsigned char *img_data = nullptr;
    size_t img_len = 0;
    short pkg_done = -1;

    int status = MTC_RECEIVER_STATE_DIE;
    Ringbuffer<unsigned char*,256> ringbuf;
    std::mutex mutex;
    std::condition_variable cond;
}Fields_IdataCallback;

typedef struct {
    jmethodID disConnect;
}ConnectCallback;



class MTC
{
private:
    MTC();
    MTC(const MTC& other);
    MTC & operator=(const MTC & other);
//    static MTC *m_Instace;
    MTC_CONFIG m_config;
    MTC_RECEIVER m_recever;

    volatile char uploadAiInfoMode = 0x00;
    volatile char rgbHeadImg = 0x00;
    volatile char rgbBgImg = 0x00;
    volatile char irImg = 0x00;
    volatile char m_is_frame_ai_info = 0x00;

    int initSerial(const char *device, const int speed, const int stopBits, const int dataBits, const int parity);
    void initReveiver();
    void destroySerial() noexcept;
    void processSerialThread();

    int processRecvTLV(Serial *serial,unsigned char *dst_data,size_t dst_length);
    unsigned char *readData(size_t &length,int msecond, const unsigned short &tag);
    int writeData(const unsigned char* data,size_t length);
    static void serialCallbackFn(Serial* serial,void* mtc);

    MTC_TLVS *coreCreateTLVS(unsigned short tag, unsigned int length, unsigned char *data);
    MTC_TLVS *coreParseTLVS(unsigned char* data);
    MTC_TLVS *coreRecvTLVS(int msecond, const unsigned short &tag);
    void coreFreeTLVS(MTC_TLVS* tlvs);
    int coreSendTLVS(MTC_TLVS* tlvs);
    MTC_TLVS *coreSendTLVS(MTC_TLVS *tlvs, int msecond);
    int coreSendRecvAT(const unsigned char* cmd,size_t cmd_len,unsigned char** resp,size_t &resp_len,int msecond);
    int coreSendRecvAT(const unsigned char *cmd, size_t cmd_len,unsigned  char **resp, int msecond);
    inline void coreFreeAT(unsigned char* data);
    int coreSendAT(const unsigned char *data,size_t length);
    unsigned char *coreRecvAT(size_t &length,int msecond);
    unsigned char *coreRecvAT(int msecond);
    int pkgSplicing(const unsigned char *src,size_t src_len, short pkg_done);

public:
    /*** JNI ***/
    JavaVM *m_javavm;
    jweak m_listener;
    Fields_IdataCallback m_dataCallback_fields;

    int logLevel = 0;

    int getLogLevel(){
        return logLevel;
    }

    static MTC &getInstance();
    void destroy();
    int init(int mode, const char* device, const int speed, const int stopBits, const int dataBits, const int parity);
    bool initCallback(JNIEnv *env, jobject callback);
    bool destroyCallback() noexcept ;


    /*--||  command  ||--*/
    int atMode();
    int dataMode();
    int atHandle(const char *atCmd, size_t atLength,unsigned  char **resp, size_t &respLen, int msecond); //resp must release[free] by manual.
    int atHandle(const char *atCmd, size_t atLength,unsigned  char **resp, int msecond); //resp must release[free] by manual.
    int ping(char* data,size_t length);

    int package_fill_crc32(unsigned char *data);
    unsigned int package_calc_crc32(unsigned char *data, unsigned int length);
    unsigned int package_get_length(unsigned char* data) noexcept ;
    int package_fill_length(unsigned char *data, unsigned int length);

    char *shell(char* cmd,size_t length);

    void nativeDisconnect();

};

#endif // MTC_H
