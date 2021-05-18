#include "mtc.h"
#include "Utils.h"
#include "JNIEnvPtr.h"
#include <unistd.h>
//#include <atomic>
#include <cstring>
#include <chrono>
#include <cstddef>
#include <functional>
#include <ostream>
#include <typeinfo>
#include <libgen.h>

MTC::MTC() {}

MTC::MTC(const MTC &other) {}


void MTC::initReveiver() {
    this->m_recever.clean_serial_buffer_threshold = 4;
    this->m_recever.mode = MTC_RECEIVER_MODE_TLV;
    std::thread thd_serial(std::bind(&MTC::processSerialThread, &(MTC::getInstance())));
    thd_serial.detach();
}

bool MTC::initCallback(JNIEnv *env, jobject callback) {
    this->destroyCallback();
    env->GetJavaVM(&m_javavm);

    this->m_listener = env->NewWeakGlobalRef(callback);
    jclass clz = env->GetObjectClass(MTC::getInstance().m_listener);
    this->m_dataCallback_fields.onTracking = env->GetMethodID(clz, "onTracking",
                                                              "(Ljava/lang/String;)V");
    this->m_dataCallback_fields.onVerify = env->GetMethodID(clz, "onVerify",
                                                            "(Ljava/lang/String;)V");
    this->m_dataCallback_fields.onQrCodeData = env->GetMethodID(clz, "onQrCodeData",
                                                                "(Ljava/lang/String;)V");
    this->m_dataCallback_fields.onFeatureData = env->GetMethodID(clz, "onFeatureData",
                                                                 "(Ljava/lang/String;Ljava/nio/ByteBuffer;)V");
    this->m_dataCallback_fields.onPhotoData = env->GetMethodID(clz, "onPhotoData",
                                                               "(ILjava/lang/String;Ljava/nio/ByteBuffer;Ljava/nio/ByteBuffer;Ljava/nio/ByteBuffer;)V");
    this->m_dataCallback_fields.onDisconnect = env->GetMethodID(clz, "onDisconnect", "(Ljava/nio/ByteBuffer;)V");

    this->m_dataCallback_fields.status = MTC_RECEIVER_STATE_ALIVE;
    this->m_dataCallback_fields.img_data = (unsigned char *) calloc(1, MTC_RECEIVER_IMG_MAX_BUF);//300k
    this->m_dataCallback_fields.img_len = 0;
    return true;
}

int MTC::initSerial(const char *device, const int speed, const int stopBits, const int dataBits, const int parity) {
    if (this->m_config.serial != nullptr) {
        this->destroySerial();
    }
    Serial *serial = (Serial *) malloc(sizeof(Serial));
    int ret = serial->init(device, speed, stopBits, dataBits, parity);
    if (ret) {
        return ret;
    }
    this->m_config.serial = serial;
    return ret;
}

bool MTC::destroyCallback() noexcept {
    LOGW("destroyCallback,,, this->m_dataCallback_fields.status:%d", this->m_dataCallback_fields.status);
    if (this->m_dataCallback_fields.status) {
        this->m_dataCallback_fields.status = MTC_RECEIVER_STATE_DIE;
        this->m_dataCallback_fields.cond.notify_all();
        unsigned char *total_data = nullptr;
        while (this->m_dataCallback_fields.ringbuf.remove(total_data)) {
            free(total_data);
        }
    }
    this->m_listener = nullptr;
    this->m_dataCallback_fields.onTracking = nullptr;
    this->m_dataCallback_fields.onVerify = nullptr;
    this->m_dataCallback_fields.onQrCodeData = nullptr;
    this->m_dataCallback_fields.onFeatureData = nullptr;
    this->m_dataCallback_fields.onPhotoData = nullptr;
    this->m_dataCallback_fields.onDisconnect = nullptr;
    this->m_javavm = nullptr;
    return true;
}

void MTC::destroySerial() noexcept {
    if (this->m_config.serial != nullptr) {
        LOGE("destroySerial");
        this->m_config.serial->destroy();
        free(this->m_config.serial);
        this->m_config.serial = nullptr;
        this->m_recever.buffer.clean();
    }
}

void MTC::processSerialThread() {
    this->m_recever.status = MTC_RECEIVER_STATE_ALIVE;
    this->m_config.serial->uartReadData((MTC::serialCallbackFn), this); // while(true) loop
    this->m_recever.status = MTC_RECEIVER_STATE_DIE; //uartReadData() was done.
}


unsigned char *MTC::readData(size_t &length, int msecond, const unsigned short &tag) {
    std::unique_lock<std::mutex> lck(this->m_recever.mutex);
    LOGV("readData mutex in.");
    if (this->m_recever.buffer.data != nullptr && tag == *(unsigned short *) (this->m_recever.buffer.data + 1 + 4)) {
        LOGV("received data faster than wait notify.");
    } else {
        if (this->m_recever.cond.wait_for(lck, std::chrono::milliseconds(msecond)) == std::cv_status::timeout && this->m_recever.buffer.data == nullptr) {
            //等待时间超时后也得检查一下是否有数据了，有可能没有通知到。
            LOGW("wait timeout.");
            return nullptr;
        }
    }

    if (this->m_recever.buffer.data == nullptr || tag != *(unsigned short *) (this->m_recever.buffer.data + 1 + 4)) {
        LOGE("invalid data.");
        return nullptr;
    }
    length = this->m_recever.buffer.read_len;
    unsigned char *data = nullptr;
    if (length) {
        data = (unsigned char *) calloc(length, sizeof(char));
        memcpy(data, this->m_recever.buffer.data, length);
        this->m_recever.buffer.clean();
    }
    return data;
}


int MTC::writeData(const unsigned char *data, size_t length) {
    if (data == nullptr || length < 2)
        return ERR_INVALID_DATA;
    if (!this->m_config.serial)
        return ERR_NOT_INIT;
    if (this->m_recever.status == MTC_RECEIVER_STATE_ALIVE)
        return this->m_config.serial->uartWriteData(data, length);
    return ERR_RECEIVER_INVALID_STAT;
}

/******************************************************************************
Function   : serialCallbackFn
Description:
Input      : tag: data tag
             length: data length
             data: raw data
Output     : none
Return     : tlvs data
******************************************************************************/
void MTC::serialCallbackFn(Serial *serial, void *mtc) {
    MTC *m = (MTC *) mtc;
    LOGV("serialCallbackFn start");
//    std::unique_lock<std::mutex> lck(m->getInstance().m_recever.mutex);//NOTE 因为这里加锁的话，如果正在请求数据比如添加用户或者固件升级，突然某个时刻串口断了，会导致一直进入error0或者error11，进入死循环，导致请求数据的接口也卡在readData里的lock函数上面，不能退出，所以改了个位置。
    unsigned char *total_data = nullptr;
    size_t total_length = 0, once_length = 0;
    if (0 == serial->getFD()) {
        m->getInstance().destroySerial();
        LOGW("notify disconnect.");
        m->getInstance().nativeDisconnect();
        return;
    }

    total_data = (unsigned char *) calloc(MTC_RECEIVER_BUFFER_CHUNK_SIZE, sizeof(char));
    do {
        if (total_length + MTC_RECEIVER_BUFFER_CHUNK_SIZE >= MTC_RECEIVER_BUFFER_DATA_MAX_LENGTH)
            break;//if resize would fail, don't read more, return what we have.
        if (total_length != 0) {
            unsigned char *tmp = (unsigned char *) realloc((void *) total_data,
                                                           MTC_RECEIVER_BUFFER_CHUNK_SIZE);
            if (tmp != nullptr)
                total_data = tmp;
            else//if resize would fail, don't read more, return what we have.
                break;
        }
        once_length = static_cast<size_t>(read(serial->getFD(), total_data + total_length,
                                               MTC_RECEIVER_BUFFER_CHUNK_SIZE));
        if (once_length > 0)
            total_length += once_length;
        if (once_length == 0) {
            if (total_data != nullptr)
                free(total_data);
            serial->destroy();
            return;
        }
    } while (once_length == MTC_RECEIVER_BUFFER_CHUNK_SIZE);

    std::unique_lock<std::mutex> lck(m->getInstance().m_recever.mutex);
    if (total_length) {
        m->getInstance().m_recever.buffer.clean();
        m->getInstance().m_recever.buffer.data = total_data;
        m->getInstance().m_recever.buffer.read_len = total_length;
    }
//    m->getInstance().m_recever.cond.notify_one();

    short tag = *(short *) (total_data + 5);
    if (tag == MTC_TAG_AI_CONTROL_UPLOAD_INFO || tag == MTC_TAG_AI_CONTROL_FRAME_PRIVATE) {
        return;
    } else if (m->getInstance().m_recever.buffer.read_len == 0 || m->getInstance().m_recever.buffer.data == nullptr) {
        //Todo received data was faster than waiting to notify, you don't need to notify and then you can avoid notifying another wait.
        return;
    }
    m->getInstance().m_recever.cond.notify_all();
    LOGV("start notify");
}

/******************************************************************************
Function   : coreCreateTLVS
Description: creata tlvs data.
Input      : tag: data tag.
             length: data length.
             data: raw data.
Output     : none
Return     : tlvs data
******************************************************************************/
MTC_TLVS *MTC::coreCreateTLVS(unsigned short tag, unsigned int length, unsigned char *data) {

    LOGD("sizeof(MTC_TLVS):%d", sizeof(MTC_TLVS));
    MTC_TLVS *tlvs = (MTC_TLVS *) malloc(sizeof(MTC_TLVS) + length);
    memset(tlvs, 0, sizeof(MTC_TLVS) + length);
    tlvs->tag = tag;
    tlvs->length = length;

    if (data != nullptr) {
        memcpy(tlvs->value, data, length);
    }
    return tlvs;
}

/******************************************************************************
Function   : coreParseTLVS
Description: convert char* data to tlvs data.
Input      : char* data
Output     : none
Return     : tlvs data
******************************************************************************/
MTC_TLVS *MTC::coreParseTLVS(unsigned char *data) {
    MTC_TLVS *head = nullptr;
    MTC_TLVS *prev = nullptr;
    MTC_TLVS *tlvs = nullptr;
    TLV *tlv = nullptr;

    tlv = TLV::package_get_first_tlv((data));
    while (tlv != nullptr) {
        tlvs = coreCreateTLVS(tlv->tag, tlv->length, tlv->value);
        if (head == nullptr)
            head = tlvs;
        if (prev != nullptr)
            prev->next = tlvs;
        prev = tlvs;
        tlv = TLV::package_get_next_tlv(data, tlv);
    }
    free(data);
    return head;
}

MTC_TLVS *MTC::coreSendTLVS(MTC_TLVS *tlvs, int msecond) {
    int ret = this->coreSendTLVS(tlvs);
    LOGV("tlv data send,ret: %d.  send's tag:%hd, send's length:%d", ret, tlvs->tag, tlvs->length);
    if (ret)
        return nullptr;
    return this->coreRecvTLVS(msecond, tlvs->tag);
}

int MTC::coreSendRecvAT(const unsigned char *cmd, size_t cmd_len, unsigned char **resp,
                        size_t &resp_len, int msecond) {
    int ret = this->coreSendAT(cmd, cmd_len);
    if (ret) {
        return ret;
    }
    *resp = this->coreRecvAT(resp_len, msecond);
    return ret;
}

int MTC::coreSendRecvAT(const unsigned char *cmd, size_t cmd_len, unsigned char **resp, int msecond) {
    int ret = this->coreSendAT(cmd, cmd_len);
    if (ret) {
        return ret;
    }
    *resp = this->coreRecvAT(msecond);
    return ret;
}

void MTC::coreFreeAT(unsigned char *data) {
    if (data != nullptr)
        free(data);
}

MTC_TLVS *MTC::coreRecvTLVS(int msecond, const unsigned short &tag) {
    size_t length = 0;
    unsigned char *data = this->readData(length, msecond, tag);
    LOGE("length:%d", length);
    if (data == nullptr)
        return nullptr;
    TLV tlv;
    if (tlv.package_validity_check(data)) {
        free(data);
        LOGE("%s  invalid tlv data.\n", __FUNCTION__);
        return nullptr; //invalid data
    }
    return this->coreParseTLVS(data);
}

/******************************************************************************
Function   : coreFreeTLVS
Description: free tlvs data.
Input      : tlvs list
Output     : none
Return     : none
******************************************************************************/
void MTC::coreFreeTLVS(MTC_TLVS *tlvs) {
    MTC_TLVS *head = nullptr;
    while (tlvs != nullptr) {
        head = tlvs;
        tlvs = tlvs->next;
        free(head);
    }
}

/******************************************************************************
Function   : coreSendTLVS
Description: send tlvs.
Input      : tlvs list
Output     : none
Return     : 0 is success, other is fail
******************************************************************************/
int MTC::coreSendTLVS(MTC_TLVS *tlvs) {
    size_t length = 0;
    MTC_TLVS *head = tlvs;
    TLV *tlv = nullptr;
    while (tlvs != nullptr) {
        length += MTC_PACKAGE_TLV_T_LENGTH + MTC_PACKAGE_TLV_L_LENGTH + tlvs->length;
        tlvs = tlvs->next;
    }
    length = MTC_PACKAGE_PROTOCOL_LENGTH + MTC_PACKAGE_LENGTH_LENGTH + length + MTC_PACKAGE_CRC32_LENGTH;
    unsigned char *data = (unsigned char *) calloc(length, sizeof(char));
    if (data == nullptr)
        return ERR_INVALID_ALLOC_MEM;
    if (this->m_config.mode == MTC_MODE_UART_CLIENT || this->m_config.mode == MTC_MODE_SOCKET_CLIENT) {
        tlv->package_fill_protocol(data, MTC_PACKAGE_PROTOCOL_CLIENT);
    } else {
        free(data);
        return ERR_INVALID_PROTOCOL;
    }
    tlv->package_fill_length(data, 0);
    tlvs = head;
    while (tlvs != nullptr) {
        tlv = (TLV *) (&(tlvs->tag));
        tlv->package_fill_tlv(data, tlv);
        tlvs = tlvs->next;
    }
    //only for debug
//    char * p = "hello";
//    unsigned char * test = (unsigned char *)calloc(10, sizeof(char));
//    test[0] = 't';
//    memcpy(test +1 ,tlv->value, 5);
//    LOGE("test:%s",test);
//    LOGE("data:%s",data);
    //only for debug

    tlv->package_fill_crc32(data);
    LOGE("crc:%d", *(int *) (data + length - 4));
    int ret = this->writeData(data, length);
    free(data);
    return ret;
}

int MTC::coreSendAT(const unsigned char *data, size_t length) {
    return this->writeData(data, length);
}

unsigned char *MTC::coreRecvAT(size_t &length, int msecond) {
    return this->readData(length, msecond, 0);
}

unsigned char *MTC::coreRecvAT(int msecond) {
    size_t length = 0;
    return this->readData(length, msecond, 0);
}

MTC &MTC::getInstance() {
    static MTC mtc;
    return mtc;
}


int MTC::init(int mode, const char *device, const int speed, const int stopBits, const int dataBits, const int parity) {
    this->m_config.mode = mode;
    int ret = this->initSerial(device, speed, stopBits, dataBits, parity);
    if (ret) return ret;
    this->initReveiver();
    return ret;
}


int MTC::atMode() {
    MTC_TLVS *send_tlvs = nullptr;
    MTC_TLVS *recv_tlvs = nullptr;
    send_tlvs = coreCreateTLVS(MTC_TAG_MODE_AT, 0, nullptr);
    recv_tlvs = coreSendTLVS(send_tlvs, MTC_RECEIVER_MAX_TIMEOUT / 3);
    if (recv_tlvs != nullptr)
        this->coreFreeTLVS(recv_tlvs);
    this->coreFreeTLVS(send_tlvs);
    this->m_recever.mode = MTC_RECEIVER_MODE_AT;
    return 0;
}


int MTC::atHandle(const char *atCmd, size_t atLength, unsigned char **resp, int msecond) {
    int ret = this->coreSendRecvAT((const unsigned char *) (atCmd), atLength, resp, msecond);
//    LOGD("%s %d: ret : %s",__FUNCTION__,__LINE__,ret);
    return ret;
}

int MTC::dataMode() {
    const char *cmd = "ATDATA\r\n";
    unsigned char *resp_data = nullptr;
    int ret = this->atHandle(cmd, strlen(cmd), &resp_data, MTC_RECEIVER_MAX_TIMEOUT / 3);
//  LOGD("%s %d: AT mode ret: %d,resp: %s",__FUNCTION__,__LINE__,ret,resp_data);
    this->coreFreeAT(resp_data);
    this->m_recever.mode = MTC_RECEIVER_MODE_TLV;
    return ret;
}

int MTC::package_fill_length(unsigned char *data, unsigned int length) {

    data[1] = (length & 0xFF000000) >> 24;
    data[2] = (length & 0x00FF0000) >> 16;
    data[3] = (length & 0x0000FF00) >> 8;
    data[4] = (length & 0x000000FF);
    return 0;
}

unsigned int MTC::package_get_length(unsigned char *data) noexcept {
    //以防大小端问题
    return static_cast<unsigned int>(data[1] << 24 | data[2] << 16 | data[3] << 8 | data[4]);
}

unsigned int MTC::package_calc_crc32(unsigned char *data, unsigned int length) {
    unsigned int crc32_table[256] = {0};
    unsigned int crc32 = 0;
    unsigned int index = 0;
    unsigned int pos = 0;

    for (index = 0; index < 256; index++) {
        crc32 = index;
        for (pos = 0; pos < 8; pos++) {
            if (crc32 & 1)
                crc32 = 0xedb88320L ^ (crc32 >> 1);
            else
                crc32 = crc32 >> 1;
        }
        crc32_table[index] = crc32;
    }

    crc32 = 0xffffffff;
    for (index = 0; index < length; index++) {
        crc32 = crc32_table[(crc32 ^ data[index]) & 0xff] ^ (crc32 >> 8);
    }

    return crc32;
}

int MTC::package_fill_crc32(unsigned char *data) {
    unsigned int length = this->package_get_length(data);
    LOGE("length:%d", length);
    unsigned int crc32 = this->package_calc_crc32(data, length + MTC_PACKAGE_PROTOCOL_LENGTH + MTC_PACKAGE_LENGTH_LENGTH);
    unsigned char *crc32_data = data + MTC_PACKAGE_PROTOCOL_LENGTH + MTC_PACKAGE_LENGTH_LENGTH + length;

    crc32_data[0] = (crc32 & 0xFF000000) >> 24;
    crc32_data[1] = (crc32 & 0x00FF0000) >> 16;
    crc32_data[2] = (crc32 & 0x0000FF00) >> 8;
    crc32_data[3] = (crc32 & 0x000000FF);

    return 0;
}

/**
 * 由于我的调试设备是得用tlv协议才能连通，所以这里还是手动拼接了tlv协议内容，用户不需要请自行更改。
 * @param data
 * @param length
 * @return
 */
int MTC::ping(char *data, size_t length) {
    //这部分内容，依据自己的调试设备的交互协议，进行修改。
    unsigned char *pingData = (unsigned char *) calloc(length + 15, sizeof(char));
    pingData[0] = MTC_PACKAGE_PROTOCOL_CLIENT;
    this->package_fill_length(pingData, 2 + 4 + length);
    *(unsigned  short*) (pingData + 5) = MTC_TAG_PING_TEST;
    *(int *) (pingData + 7) = length;
    memcpy(pingData + 11, data, length);
    package_fill_crc32(pingData);
    int ret = this->writeData(pingData, 24);

    //这部分内容，依据自己调试设备的交互协议，进行修改。

    LOGD("ret:%d",ret);
    free(pingData);
    size_t len = 0;
    unsigned char *receiveData = this->readData(len, MTC_RECEIVER_MAX_TIMEOUT, MTC_TAG_PING_TEST);
    LOGD("receiveData:%s", receiveData);
    if (receiveData == nullptr){
        ret = -1;
    }
    return ret;
}

void MTC::nativeDisconnect() {
    LOGD("Get disconnect cb,lst:0x%x,discb:0x%x", this->m_listener, this->m_dataCallback_fields.onDisconnect);
    if (this->m_javavm == nullptr) {
        LOGE("callback is null");
        return;
    }
    if (this->m_listener != nullptr && this->m_dataCallback_fields.onDisconnect != nullptr) {
        JNIEnvPtr *jniEnvPtr = new JNIEnvPtr(this->m_javavm);
        JNIEnv *env = jniEnvPtr->operator->();

        env->CallVoidMethod(this->m_listener, this->m_dataCallback_fields.onDisconnect, nullptr);
        env->ExceptionClear();
        jniEnvPtr->detachCurrentThread();
        jniEnvPtr = nullptr;
    } else {
        LOGW("not reg disconnect.");
    }
    //Don't do anything else, Leave it to the onDisconnect cb fn.
}


void MTC::destroy() {
    this->destroySerial();
    this->destroyCallback();
}


