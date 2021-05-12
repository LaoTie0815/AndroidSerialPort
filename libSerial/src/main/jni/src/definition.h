#ifndef DEFINITION_H
#define DEFINITION_H

#include "../mtc_log.h"
//error definition.
#define SUCCESS                            (0)

#define ERR_NOT_INIT                            (-1)
#define ERR_ALREADY_INIT                        (-2)
#define ERR_CANNOT_OPEN                         (-3)
#define ERR_CANNOT_FCNTL                        (-4)
#define ERR_CANNOT_SET_ATTR                     (-5)
#define ERR_INVALID_FD                          (-6)
#define ERR_WRITE_RETRY_FAILED                  (-7)
#define ERR_INVALID_DATA                        (-8)
#define ERR_INVALID_ALLOC_MEM                   (-10)
#define ERR_INVALID_PROTOCOL                    (-11)
#define ERR_RECEIVER_INVALID_STAT               (-12)
#define ERR_WAIT_RECV_TIMEOUT                   (-13)



//serial
#define UART_MAX_PATH                           256

//mtc & tlv
#define MTC_RECEIVER_BUFFER_CHUNK_SIZE          0x1000 //2^12 ,4K
#define MTC_RECEIVER_BUFFER_DATA_MAX_LENGTH     (1024*1024*64)   //64MB
#define MTC_RECEIVER_MODE_AT                    0x0001 //AT
#define MTC_RECEIVER_MODE_TLV                   0x0002 //Data
#define MTC_RECEIVER_STATE_DIE                  0x0000 //die
#define MTC_RECEIVER_STATE_ALIVE                0x0001 //alive
#define MTC_RECEIVER_IMG_MAX_BUF                (1024 * 300) //300KB
#define MTC_RECEIVER_MAX_TIMEOUT                5000 //msecond ,模组内指令调用最大超时上限2900ms
#define MTC_MODE_UART_CLIENT                    0x0002
#define MTC_MODE_SOCKET_CLIENT                  0x0004


#define MTC_IMAGE_ID_MAX_LENGTH                 32
#define MTC_UPGRADE_SEGMENT_MAX_LENGTH          (16384-16) //16KB

//mtc protocol tag
#define MTC_TAG_ROUSE_SERIAL                        0x999
#define MTC_TAG_PING_TEST                        0x1000
#define MTC_TAG_SHELL_CMD                        0x1001
#define MTC_TAG_MODE_AT                          0x1002

#define MTC_TAG_AI_CONTROL_UPLOAD_INFO           0x0018
#define MTC_TAG_AI_CONTROL_FRAME_PRIVATE         0x0019


#define MTC_TAG_AI_TRACK_UPLOAD                  0x0030
#define MTC_TAG_AI_VERIFY_UPLOAD                 0x0031
#define MTC_TAG_QR_CODE_RECOGNIZE                 0x0032
#define MTC_TAG_AI_IMAGE_UPLOAD                  0x0033
#define MTC_TAG_AI_FEATURE_UPLOAD                0x0034


#endif // DEFINITION_H
