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
    std::thread thd_jni(std::bind(&MTC::processJniCallbackThread, &(MTC::getInstance())));
    thd_jni.detach();
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

void MTC::processJniCallbackThread() {
    LOGV("start jni cb thd.");
    JNIEnvPtr *jniEnvPtr = new JNIEnvPtr(this->m_javavm);
    while (this->m_dataCallback_fields.status) {
        std::unique_lock<std::mutex> lck(this->m_dataCallback_fields.mutex);
        if (this->m_dataCallback_fields.ringbuf.isEmpty()) {
            this->m_dataCallback_fields.cond.wait(lck);
            if (!this->m_dataCallback_fields.status) break;
        }
        TLV *tlv;
        unsigned char *total_data = nullptr;
        while (!this->m_dataCallback_fields.ringbuf.remove(total_data));
        if (total_data == nullptr)
            continue;
        tlv = TLV::package_get_first_tlv(total_data);

        switch (tlv->tag) {
            case MTC_TAG_AI_TRACK_UPLOAD:
                this->nativeTracking(total_data, jniEnvPtr->operator->());
                break;
            case MTC_TAG_AI_VERIFY_UPLOAD:
                this->nativeVerify(total_data, jniEnvPtr->operator->());
                break;
            case MTC_TAG_AI_IMAGE_UPLOAD:
                this->nativePictureDataNew(total_data, jniEnvPtr->operator->());
                break;
            case MTC_TAG_AI_FEATURE_UPLOAD:
                this->nativeFeature(total_data, jniEnvPtr->operator->());
                break;
            case MTC_TAG_QR_CODE_RECOGNIZE:
                this->nativeQrCode(total_data, jniEnvPtr->operator->());
                break;
            default://maybe others.
                break;
        }
    }
//    this->nativeDisconnect(jniEnvPtr->operator->());
    jniEnvPtr->detachCurrentThread();
    jniEnvPtr = nullptr;

    LOGW("JNI CB thread exit");
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

int MTC::processRecvTLV(Serial *serial, unsigned char *dst_data, size_t dst_length) {
    size_t once_length = 0;
    size_t read_pos = 0;
    size_t try_count = 15;
    int err;
    while (try_count > 0) {
        once_length = static_cast<size_t>(read(serial->getFD(), dst_data + read_pos, dst_length - read_pos));
        if (once_length <= 0) {
            err = errno;
            LOGE("len: %d,errno:%d, %s", once_length, err, strerror(err));
            if (EAGAIN == err || EINTR == err) {
                usleep(1000);
                try_count--;
                continue;
            }
            return -2; //other err.
        }
        read_pos += once_length;
        if (read_pos == dst_length) {
            return 0;
        }
    }
    LOGE("may disconnect.");
    this->destroySerial();
    return -1; //it's mean disconnect.
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

    if (m->getInstance().m_recever.mode == MTC_RECEIVER_MODE_TLV) {
        unsigned char head[1 + 4] = {0}; //protocol + length
        int ret = 0;
        if ((ret = m->getInstance().processRecvTLV(serial, head, sizeof(head))) != 0) {
            if (-1 == ret) {
                LOGW("notify disconnect.");
                m->getInstance().nativeDisconnect();
            }
            return;
        }

        if (head[0] != MTC_PACKAGE_PROTOCOL_CLIENT && head[0] != MTC_PACKAGE_PROTOCOL_SERVER) {
            if (m->getInstance().m_recever.read_invalid_data_count > m->getInstance().m_recever.clean_serial_buffer_threshold) {
                LOGE("invalid data read count:%d great than Threshold: %d", m->getInstance().m_recever.read_invalid_data_count,
                     m->getInstance().m_recever.clean_serial_buffer_threshold);
                m->getInstance().aiControlAutoUploadAiInfo(m->getInstance().uploadAiInfoMode, m->getInstance().rgbHeadImg, m->getInstance().rgbBgImg,
                                                           m->getInstance().irImg);
                m->getInstance().mUploadFrameAiInfo(m->getInstance().m_is_frame_ai_info);
                serial->cleanSeriaBuffer();
                m->getInstance().aiControlAutoUploadAiInfo(m->getInstance().uploadAiInfoMode, m->getInstance().rgbHeadImg, m->getInstance().rgbBgImg,
                                                           m->getInstance().irImg);
                m->getInstance().mUploadFrameAiInfo(m->getInstance().m_is_frame_ai_info);
                m->getInstance().m_recever.read_invalid_data_count = 0;
                return;
            }
            LOGE("Unknow protocol data.");
            m->getInstance().m_recever.read_invalid_data_count++;
            return;
        }
        TLV *tlv;
        size_t tlv_length = tlv->package_get_length(head);
        total_length = tlv_length + sizeof(head) + 4;
        total_data = (unsigned char *) calloc(total_length, sizeof(char));
        memcpy(total_data, head, sizeof(head));
        if ((ret = m->getInstance().processRecvTLV(serial, total_data + sizeof(head), (tlv_length+4) )) != 0) {
            free(total_data);
            if (-1 == ret) {
                LOGD("notify disconnect.");
                m->getInstance().nativeDisconnect();
            }
            return;
        }
        if (total_data[0] == MTC_PACKAGE_PROTOCOL_CLIENT || total_data[0] == MTC_PACKAGE_PROTOCOL_SERVER) {
            LOGV(" tlvs length: %d,tag:0x%x", *(int *) (total_data + 7), *(short *) (total_data + 5));
            if (tlv->package_validity_check(total_data) == 0) {
                tlv = TLV::package_get_first_tlv(total_data);

                LOGV("check pass,tag: 0x%x.   value:0x%x", tlv->tag, tlv->value);
                switch (tlv->tag) {
                    case MTC_TAG_AI_TRACK_UPLOAD:
                    case MTC_TAG_AI_VERIFY_UPLOAD:
                    case MTC_TAG_AI_IMAGE_UPLOAD:
                    case MTC_TAG_AI_FEATURE_UPLOAD:
                    case MTC_TAG_QR_CODE_RECOGNIZE:
                        if (MTC_TAG_AI_IMAGE_UPLOAD == tlv->tag) {
                            m->getInstance().dataAck(MTC_TAG_AI_IMAGE_UPLOAD);
                        }
                        if (m->getInstance().m_dataCallback_fields.ringbuf.isFull()) {
                            LOGE("ringbuf is full");
                            unsigned char *remove_data = nullptr;
                            while (m->getInstance().m_dataCallback_fields.ringbuf.remove(remove_data));
                            if (remove_data != nullptr)
                                free(remove_data);
                        }
                        m->getInstance().m_dataCallback_fields.ringbuf.insert(total_data);
                        m->getInstance().m_dataCallback_fields.cond.notify_one();
                        return;
                    default://maybe others.
                        break;
                }
            }
        }
    } else {
        LOGV("%s %d: read AT data.", __FUNCTION__, __LINE__);
        total_data = (unsigned char *) calloc(MTC_RECEIVER_BUFFER_CHUNK_SIZE, sizeof(char));
        do {
            if (total_length + MTC_RECEIVER_BUFFER_CHUNK_SIZE >=
                MTC_RECEIVER_BUFFER_DATA_MAX_LENGTH)
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
    }

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

    LOGD("sizeof(MTC_TLVS):%d",sizeof(MTC_TLVS));
    MTC_TLVS *tlvs = (MTC_TLVS *) malloc(sizeof(MTC_TLVS) + length);
    memset(tlvs, 0, sizeof(MTC_TLVS) + length);
    tlvs->tag = tag;
    tlvs->length = length;
    //test
    //test
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
    if (this->m_config.mode == MTC_MODE_UART_CLIENT || this->m_config.mode == MTC_MODE_SOCKET_CLIENT)
        tlv->package_fill_protocol(data, MTC_PACKAGE_PROTOCOL_CLIENT);
//    else if (this->m_config.mode == MTC_MODE_UART_CLIENT ||
//             this->m_config.mode == MTC_MODE_SOCKET_CLIENT)
//        tlv->package_fill_protocol(data, MTC_PACKAGE_PROTOCOL_SERVER);
    else {
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
    tlv->package_fill_crc32(data);
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

int MTC::dataAck(unsigned short tag) {
    MTC_TLVS *send_tlvs = nullptr;
    send_tlvs = this->coreCreateTLVS(tag, 0, nullptr);
    int ret = this->coreSendTLVS(send_tlvs);
    LOGD("ret:%d", ret);
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

int MTC::atHandle(const char *atCmd, size_t atLength, unsigned char **resp, size_t &respLen, int msecond) {
    int ret = this->coreSendRecvAT((const unsigned char *) (atCmd), atLength, resp, respLen, msecond);
    return ret;
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
//    LOGD("%s %d: AT mode ret: %d,resp: %s",__FUNCTION__,__LINE__,ret,resp_data);
    this->coreFreeAT(resp_data);
    this->m_recever.mode = MTC_RECEIVER_MODE_TLV;
    return ret;
}

int MTC::ping(char *data, size_t length) {
    MTC_TLVS *send_tlvs = nullptr;
    MTC_TLVS *recv_tlvs = nullptr;
    int ret = 0;
    send_tlvs = this->coreCreateTLVS(MTC_TAG_PING_TEST, length, (unsigned char *) (data));
    recv_tlvs = this->coreSendTLVS(send_tlvs, MTC_RECEIVER_MAX_TIMEOUT / 3);
    this->coreFreeTLVS(send_tlvs);
    if (recv_tlvs == nullptr)
        return ERR_WAIT_RECV_TIMEOUT;
    if (!(recv_tlvs->length == length && memcmp(data, recv_tlvs->value, length) == 0))
        ret = ERR_INVALID_DATA;
    this->coreFreeTLVS(recv_tlvs);
    return ret;
}


char *MTC::shell(char *cmd, size_t length) {
    MTC_TLVS *send_tlvs = nullptr;
    MTC_TLVS *recv_tlvs = nullptr;
    send_tlvs = this->coreCreateTLVS(MTC_TAG_SHELL_CMD, length, (unsigned char *) (cmd));
    recv_tlvs = this->coreSendTLVS(send_tlvs, MTC_RECEIVER_MAX_TIMEOUT);
    this->coreFreeTLVS(send_tlvs);
    if (recv_tlvs == nullptr)
        return nullptr;
    char *data = (char *) calloc(recv_tlvs->length + 1, sizeof(char));  //manual release outside this fn.
    memcpy(data, recv_tlvs->value, recv_tlvs->length);
    this->coreFreeTLVS(recv_tlvs);
    LOGD("%s %d: %s\n", __FUNCTION__, __LINE__, data);
    return data;
}

int MTC::aiControlAutoUploadAiInfo(unsigned char mode, int rgbFace, int rgbBg, int ir) {
    MTC_TLVS *send_tlvs = nullptr;
//    MTC_TLVS *recv_tlvs = nullptr;
    int ret = ERR_WAIT_RECV_TIMEOUT;
    send_tlvs = this->coreCreateTLVS(MTC_TAG_AI_CONTROL_UPLOAD_INFO, 2, &mode);

    LOGD("rgbFace:%d, rgbBg:%d, ir:%d", rgbFace, rgbBg, ir);
    int choiceImg = 0b001;
    choiceImg ^= (choiceImg & (1 << 0)) ^ (rgbFace << 0);
    choiceImg ^= (choiceImg & (1 << 1)) ^ (rgbBg << 1);
    choiceImg ^= (choiceImg & (1 << 2)) ^ (ir << 2);

    send_tlvs->value[1] = static_cast<unsigned char>(choiceImg);

//    itoa(choiceImg, s, 2);   //转换成字符串，进制基数为2
    LOGD("choiceImg:%d", choiceImg);
//    send_tlvs = this->coreCreateTLVS(MTC_TAG_AI_CONTROL_UPLOAD_INFO, 1, nullptr);
//    send_tlvs->value[0] = 0x04;
    ret = this->coreSendTLVS(send_tlvs);

    this->coreFreeTLVS(send_tlvs);
//    usleep(10000);//因为没有等待回应超时,所以此处等待10ms,防止过快执行下个函数但获取的值却为此函数的响应
    LOGD("ret:%d", ret);
    return ret;
}

int MTC::mUploadFrameAiInfo(unsigned char mode) {
    m_is_frame_ai_info = mode;
    MTC_TLVS *send_tlvs = nullptr;
    int ret = ERR_WAIT_RECV_TIMEOUT;
    send_tlvs = this->coreCreateTLVS(MTC_TAG_AI_CONTROL_FRAME_PRIVATE, 1, &mode);

    ret = this->coreSendTLVS(send_tlvs);

    this->coreFreeTLVS(send_tlvs);
//    usleep(10000);//因为没有等待回应超时,所以此处等待10ms,防止过快执行下个函数但获取的值却为此函数的响应
    LOGD("ret:%d", ret);
    return ret;
}


static unsigned int hexStringToInt(char *data, int len) {
    char tmpStr[4] = {0};
    char *pEnd;
    memcpy(tmpStr, data, len);
    return strtoul(tmpStr, &pEnd, 16);
}

void MTC::nativeTracking(unsigned char *data, JNIEnv *env) {
    if (env == nullptr || this->m_listener == nullptr || this->m_dataCallback_fields.onTracking == nullptr) {
        free(data);
        return;
    }
    MTC_TLVS *tlvs = this->coreParseTLVS(data);
    int val_length = *(int *) (tlvs->value);
    char *val_data;
    if (val_length > 0 && (val_length + 4) <= tlvs->length) {
        val_data = (char *) calloc(val_length + 1, sizeof(char));
        memcpy(val_data, tlvs->value + 4, val_length);
    } else {
        return;
    }

    this->coreFreeTLVS(tlvs);

    jstring jval = env->NewStringUTF(val_data);

    if (this->m_listener != nullptr && this->m_dataCallback_fields.onTracking != nullptr) {
        env->CallVoidMethod(this->m_listener, this->m_dataCallback_fields.onTracking, jval);
        env->ExceptionClear();
        env->DeleteLocalRef(jval);
    }
    free(val_data);
}


void MTC::nativeVerify(unsigned char *data, JNIEnv *env) {
    if (env == nullptr || this->m_listener == nullptr || this->m_dataCallback_fields.onVerify == nullptr) {
        free(data);
        return;
    }
    LOGV("%s %d: in", __FUNCTION__, __LINE__);
    MTC_TLVS *tlvs = this->coreParseTLVS(data);
    int val_length = *(int *) (tlvs->value);
    unsigned int surplus_len = tlvs->length - 4;
    char *val_data;
    if (val_length > 0 && (val_length + 4) <= tlvs->length) {
        val_data = (char *) calloc(val_length + 1, sizeof(char));
        memcpy(val_data, tlvs->value + 4, val_length);
    } else {
        return;
    }

    this->coreFreeTLVS(tlvs);
    jstring jval = env->NewStringUTF(val_data);
    if (this->m_listener != nullptr && this->m_dataCallback_fields.onVerify != nullptr) {
        env->CallVoidMethod(this->m_listener, this->m_dataCallback_fields.onVerify, jval);
        env->ExceptionClear();
        env->DeleteLocalRef(jval);
    }
    free(val_data);
}

int MTC::pkgSplicing(const unsigned char *src, size_t src_len, short pkg_done) {
    if (src == nullptr || src_len == 0 || this->m_dataCallback_fields.pkg_done != (pkg_done - 1)) {
        return -1;
    }
    memcpy(this->m_dataCallback_fields.img_data + this->m_dataCallback_fields.img_len, src, src_len);
    this->m_dataCallback_fields.img_len += src_len;
    this->m_dataCallback_fields.pkg_done = pkg_done;
    return 0;
}

void MTC::nativePictureDataNew(unsigned char *data, JNIEnv *env) {
    if (env == nullptr || this->m_listener == nullptr || this->m_dataCallback_fields.onPhotoData == nullptr) {
        free(data);
        return;
    }
    MTC_TLVS *tlvs = this->coreParseTLVS(data);
    time_t pkg_ts = *(time_t *) (tlvs->value);
    short pkg_count = *(short *) (tlvs->value + 4);
    short pkg_done = *(short *) (tlvs->value + 4 + 2);
    if (!pkg_done) {
        LOGD("memset 0 pkg_done:%d", pkg_done);
        this->m_dataCallback_fields.img_len = 0;
        this->m_dataCallback_fields.pkg_done = -1;
        memset(this->m_dataCallback_fields.img_data, 0, MTC_RECEIVER_IMG_MAX_BUF);
    }

    LOGV("pkg_count:%d,  pkg_done:%d,  pkg's time:%d", pkg_count, pkg_done, pkg_ts);

    if (this->m_dataCallback_fields.pkg_done == (pkg_done - 1) && (tlvs->length - 8) > 0) {
        memcpy(this->m_dataCallback_fields.img_data + this->m_dataCallback_fields.img_len, tlvs->value + 8, tlvs->length - 8);
        this->m_dataCallback_fields.img_len += tlvs->length - 8;
        this->m_dataCallback_fields.pkg_done = pkg_done;
    } else {
        LOGE("have received pkg_done:%d,  tlvs->length:%d", this->m_dataCallback_fields.pkg_done, tlvs->length);
        return;
    }

//    this->pkgSplicing(tlvs->value + 8, tlvs->length - 8, pkg_done);
    this->coreFreeTLVS(tlvs);
    if (pkg_count != (pkg_done + 1)) {
        return;
    }
    unsigned char *img_data = this->m_dataCallback_fields.img_data;
    unsigned int img_len = this->m_dataCallback_fields.img_len;
//    JNIEnv *env = nullptr;

//    if (this->m_javavm != nullptr) {
//        int ret = this->m_javavm->GetEnv((void **) &env, JNI_VERSION_1_6);
//        ret = this->m_javavm->AttachCurrentThread(&env, NULL);
//    }

    int read_len = 0, len_recognize_id = 0, len_rgb_head = 0, len_rgb_bg = 0, len_ir_bg = 0;

    char *val_recognize_id = nullptr, *val_rgb_head = nullptr, *val_rgb_bg = nullptr, *val_ir_bg = nullptr;

    jstring j_val_recognize_id = nullptr;
    jobject j_val_rgb_head = nullptr, j_val_rgb_bg = nullptr, j_val_ir_bg = nullptr;
    jint j_val_track_id = *(unsigned int *) (img_data);
    read_len += 4;
    len_recognize_id = *(unsigned int *) (img_data + read_len);
    read_len += 4;
    LOGV(" read_len:%d", read_len);
    if (len_recognize_id > 0 && (len_recognize_id + read_len) <= img_len) {
        val_recognize_id = (char *) calloc(len_recognize_id + 1, sizeof(char));
        memcpy(val_recognize_id, img_data + read_len, len_recognize_id);
        read_len += len_recognize_id;
        LOGV("len_recognize_id:%d,  val_recognize_id :%s,  read_len:%d", len_recognize_id, val_recognize_id, read_len);
        if (env != nullptr) {
            j_val_recognize_id = env->NewStringUTF(val_recognize_id);
        }
    } else {
        LOGE("len_recognize_id:%d,  read_len:%d,  img_len:%d", len_recognize_id, read_len, img_len);
    }

    if (read_len < img_len) {
        len_rgb_head = *(int *) (img_data + read_len);
        read_len += 4;
        LOGV("len_rgb_head:%d,  read_len:%d", len_rgb_head, read_len);
        if (len_rgb_head > 0 && (len_rgb_head + read_len) <= img_len) {
            val_rgb_head = (char *) calloc(len_rgb_head, sizeof(char));
            memcpy(val_rgb_head, img_data + read_len, len_rgb_head);
            read_len += len_rgb_head;
            LOGV("len_rgb_head:%d,  val_rgb_head :%s,  read_len:%d", len_rgb_head, val_rgb_head, read_len);
            if (env != nullptr) {
                j_val_rgb_head = env->NewDirectByteBuffer(val_rgb_head, len_rgb_head);
            }
        } else {
            LOGE("len_rgb_head:%d,  read_len:%d,  img_len:%d", len_rgb_head, read_len, img_len);
        }
    }

    if (read_len < img_len) {
        len_rgb_bg = *(int *) (img_data + read_len);
        read_len += 4;
        LOGV("len_rgb_bg:%d,  read_len:%d", len_rgb_bg, read_len);
        if (len_rgb_bg > 0 && (len_rgb_bg + read_len) <= img_len) {
            val_rgb_bg = (char *) calloc(len_rgb_bg, sizeof(char));
            memcpy(val_rgb_bg, img_data + read_len, len_rgb_bg);
            read_len += len_rgb_bg;
            LOGV("len_rgb_bg:%d,  val_rgb_bg :%s,  read_len:%d", len_rgb_bg, val_rgb_bg, read_len);
            if (env != nullptr) {
                j_val_rgb_bg = env->NewDirectByteBuffer(val_rgb_bg, len_rgb_bg);
            }
        } else {
            LOGE("len_rgb_bg:%d,  read_len:%d,  img_len:%d", len_rgb_bg, read_len, img_len);
        }
    }

    if (read_len < img_len) {
        len_ir_bg = *(int *) (img_data + read_len);
        read_len += 4;
        LOGV("len_ir_bg:%d, img_len:%d, read_len:%d", len_ir_bg, img_len, read_len);
        if (len_ir_bg > 0 && (len_ir_bg + read_len) <= img_len) {
            val_ir_bg = (char *) calloc(len_ir_bg, sizeof(char));
            memcpy(val_ir_bg, img_data + read_len, len_ir_bg);
            LOGV("len_ir_bg:%d,  val_ir_bg :%s,  read_len:%d", len_ir_bg, val_ir_bg, read_len);
            read_len += len_ir_bg;
            if (env != nullptr) {
                j_val_ir_bg = env->NewDirectByteBuffer(val_ir_bg, len_ir_bg);
            }
        } else {
            LOGE("len_ir_bg:%d,  read_len:%d,  img_len:%d", len_ir_bg, read_len, img_len);
        }
    }

    if (read_len != img_len) {
        LOGE("%s %d:read_len not equal tlvs->length ,read_len: %d,tlvs->length:%d.", __FUNCTION__, __LINE__, read_len, img_len);
    }
    if (this->m_listener != nullptr && this->m_dataCallback_fields.onPhotoData != nullptr) {
        env->CallVoidMethod(this->m_listener, this->m_dataCallback_fields.onPhotoData, j_val_track_id, j_val_recognize_id, j_val_rgb_head, j_val_rgb_bg,
                            j_val_ir_bg);
        env->ExceptionClear();
        env->DeleteLocalRef(j_val_recognize_id);
    }
//    LOGD("%s %d: id",__FUNCTION__,__LINE__);
    if (j_val_rgb_head) {
        free(val_rgb_head);
        if (env != nullptr) {
            env->DeleteLocalRef(j_val_rgb_head);
        }
//        LOGD("%s %d: head",__FUNCTION__,__LINE__);
    }
    if (j_val_rgb_bg) {
        free(val_rgb_bg);
        if (env != nullptr) {
            env->DeleteLocalRef(j_val_rgb_bg);
        }
//        LOGD("%s %d: bg",__FUNCTION__,__LINE__);
    }
    if (j_val_ir_bg) {
        free(val_ir_bg);
        if (env != nullptr) {
            env->DeleteLocalRef(j_val_ir_bg);
        }
//        LOGD("%s %d: ir",__FUNCTION__,__LINE__);
    }
//    this->m_javavm->DetachCurrentThread();
    free(val_recognize_id);
    memset(this->m_dataCallback_fields.img_data, 0, MTC_RECEIVER_IMG_MAX_BUF);
    this->m_dataCallback_fields.img_len = 0;
}

void MTC::nativeFeature(unsigned char *data, JNIEnv *env) {
    if (env == nullptr || this->m_listener == nullptr || this->m_dataCallback_fields.onPhotoData == nullptr) {
        free(data);
        return;
    }
//    JNIEnv *env = nullptr;
//    if (this->m_javavm != nullptr) {
//        int ret = this->m_javavm->GetEnv((void **) &env, JNI_VERSION_1_6);
//        ret = this->m_javavm->AttachCurrentThread(&env, NULL);
//    }

    MTC_TLVS *tlvs = this->coreParseTLVS(data);
    int len_trace_id = *(int *) (tlvs->value);
    char *val_trace_id;
    jstring j_trace_id;
    jobject j_feature;
    if (len_trace_id > 0 && (len_trace_id + 4) <= tlvs->length) {
        val_trace_id = (char *) calloc(len_trace_id + 1, sizeof(char));
        memcpy(val_trace_id, tlvs->value + 4, len_trace_id);
        if (env != nullptr) {
            j_trace_id = env->NewStringUTF(val_trace_id);
        }
        free(val_trace_id);
    }
    int len_feature = *(int *) (tlvs->value + 4 + len_trace_id);
    char *val_feature;
    if (len_feature > 0 && len_feature <= (tlvs->length - 4 - len_trace_id - 4)) {
        val_feature = (char *) calloc(len_feature, sizeof(char));
        memcpy(val_feature, tlvs->value + 4 + len_trace_id + 4, len_feature);
        if (env != nullptr) {
            j_feature = env->NewDirectByteBuffer(val_feature, len_feature);
        }
        free(val_feature);
    }
    this->coreFreeTLVS(tlvs);
//    this->m_javavm->DetachCurrentThread();
    if (this->m_listener != nullptr && this->m_dataCallback_fields.onFeatureData != nullptr) {
        env->CallVoidMethod(this->m_listener, this->m_dataCallback_fields.onFeatureData, j_trace_id, j_feature);
        env->ExceptionClear();
        env->DeleteLocalRef(j_trace_id);
        env->DeleteLocalRef(j_feature);
    }
}

void MTC::nativeQrCode(unsigned char *data, JNIEnv *env) {
    if (env == nullptr || this->m_listener == nullptr || this->m_dataCallback_fields.onQrCodeData == nullptr) {
        free(data);
        return;
    }

    MTC_TLVS *tlvs = this->coreParseTLVS(data);
    int val_length = *(int *) (tlvs->value);
    unsigned int surplus_len = tlvs->length - 4;
    char *val_data;
    LOGV("val_length:%d,   tlvs->length:%d, ", val_length, tlvs->length);
    if (val_length > 0 && (val_length + 4) <= tlvs->length) {
        val_data = (char *) calloc(val_length + 1, sizeof(char));
        memset(val_data, 0x0, val_length + 1);
        memcpy(val_data, tlvs->value + 4, val_length);
    } else {
        return;
    }
    LOGV("qrcode's values:%s", val_data);
    this->coreFreeTLVS(tlvs);
    jstring jval = env->NewStringUTF(val_data);
    if (this->m_listener != nullptr && this->m_dataCallback_fields.onQrCodeData != nullptr) {
        env->CallVoidMethod(this->m_listener, this->m_dataCallback_fields.onQrCodeData, jval);
        env->ExceptionClear();
        env->DeleteLocalRef(jval);
    }
    free(val_data);
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


