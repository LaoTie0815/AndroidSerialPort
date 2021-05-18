#include "tlv.h"
#include "definition.h"
#include <cstring>
#include <unistd.h>
#include "../mtc_log.h"
#include <libgen.h>

/******************************************************************************
Function   : package_validity_check
Description: check data  validity.
Input      : data
Output     : none
Return     : 0 is success, other is failed.
******************************************************************************/
int TLV::package_validity_check(unsigned char *data) {
    unsigned char protocol = this->package_get_protocol(data);
    unsigned int length = this->package_get_length(data);
    unsigned int crc32 = this->package_get_crc32(data);

    if (protocol != 0xA1 && protocol != 0x1A) {
        return -1;
    }

    if (this->package_calc_crc32(data, length + MTC_PACKAGE_PROTOCOL_LENGTH + MTC_PACKAGE_LENGTH_LENGTH) != crc32) {
        return -2;
    }

    return 0;
}


/******************************************************************************
Function   : package_get_protocol
Description: get package data protocol.
Input      : data
Output     : none
Return     : protocol,one char size.
******************************************************************************/
constexpr unsigned char TLV::package_get_protocol(unsigned char *data) noexcept {
    return data[0];
}


/******************************************************************************
Function   : package_get_first_tlv
Description: get package first tlv data.
Input      : data
Output     : none
Return     : TLV struct pointer.
******************************************************************************/
TLV *TLV::package_get_first_tlv(unsigned char *data) noexcept {
    TLV *tlv = nullptr;
    if (TLV::package_get_length(data) > 0) {
        tlv = reinterpret_cast<TLV *>(data + MTC_PACKAGE_PROTOCOL_LENGTH + MTC_PACKAGE_LENGTH_LENGTH);
    }
    return tlv;
}


/******************************************************************************
Function   : package_get_next_tlv
Description: get package next tlv data.
Input      : data, prev tlv pointer
Output     : none
Return     : TLV struct pointer.
******************************************************************************/
TLV *TLV::package_get_next_tlv(unsigned char *data, TLV *tlv) noexcept {
    TLV *tlv_next = nullptr;
    unsigned int tlv_length = 0;
    unsigned char *data_pos = reinterpret_cast<unsigned char *>(tlv);

    tlv_length = TLV::package_get_length(data);

    if (data_pos + MTC_PACKAGE_TLV_T_LENGTH + MTC_PACKAGE_TLV_L_LENGTH + tlv->length - data <
        MTC_PACKAGE_PROTOCOL_LENGTH + MTC_PACKAGE_LENGTH_LENGTH + tlv_length) {
        tlv_next = reinterpret_cast<TLV *>(data_pos + MTC_PACKAGE_TLV_T_LENGTH + MTC_PACKAGE_TLV_L_LENGTH + tlv->length);
    }

    return tlv_next;
}


/******************************************************************************
Function   : package_fill_protocol
Description: write protocol type to package data.
Input      : data, protocol
Output     : none
Return     : 0.
******************************************************************************/
int TLV::package_fill_protocol(unsigned char *data, unsigned char protocol) {
    if (protocol == 0xA1 || protocol == 0x1A) {
        data[0] = protocol;
    }
    return 0;
}


/******************************************************************************
Function   : package_fill_length
Description: write data length to package.
Input      : data, length
Output     : none
Return     : 0.
******************************************************************************/
int TLV::package_fill_length(unsigned char *data, unsigned int length) {

    data[1] = (length & 0xFF000000) >> 24;
    data[2] = (length & 0x00FF0000) >> 16;
    data[3] = (length & 0x0000FF00) >> 8;
    data[4] = (length & 0x000000FF);
    return 0;
}

/******************************************************************************
Function   : package_fill_length
Description: write data length to package.
Input      : data, length
Output     : none
Return     : 0.
******************************************************************************/
int TLV::package_fill_tlv(unsigned char *data, TLV *tlv) {
    unsigned int length = this->package_get_length(data);
    TLV *tlv_next = nullptr;

    tlv_next = reinterpret_cast<TLV *>(data + MTC_PACKAGE_PROTOCOL_LENGTH + MTC_PACKAGE_LENGTH_LENGTH + length);

    //only for debug
//    LOGE("tlv_next->tag:%hd, length:%d, value:%s, data:%s, length:%d", tlv_next->tag, tlv_next->length, tlv_next->value, data, length);
//    for (int i = 0; i < sizeof(data); ++i) {
//        LOGE("i:%c", data[i]);
//    }
    //only for debug

    tlv_next->tag = tlv->tag;
    tlv_next->length = tlv->length;
    memcpy(tlv_next->value, tlv->value, tlv->length);

    //only for debug
//    LOGE("tlv_next->tag:%hd, length:%d, value:%s, data:%s, length:%d", tlv_next->tag, tlv_next->length, tlv_next->value, data, length);
//    LOGE("proto: %d", *(char *)data);
//    for (int i = 0; i < sizeof(data); ++i) {
//        LOGE("after i:%c", data[i]);
//    }
    //only for debug

    length += MTC_PACKAGE_TLV_T_LENGTH + MTC_PACKAGE_TLV_L_LENGTH + tlv->length;
    this->package_fill_length(data, length);

    LOGE("length: %d", *(int *)(data+1));

    return 0;
}

/******************************************************************************
Function   : package_fill_crc32
Description: write data length to package.
Input      : data, length
Output     : none
Return     : 0.
******************************************************************************/
int TLV::package_fill_crc32(unsigned char *data) {
    unsigned int length = this->package_get_length(data);
    unsigned int crc32 = this->package_calc_crc32(data, length + MTC_PACKAGE_PROTOCOL_LENGTH + MTC_PACKAGE_LENGTH_LENGTH);
    unsigned char *crc32_data = data + MTC_PACKAGE_PROTOCOL_LENGTH + MTC_PACKAGE_LENGTH_LENGTH + length;

    crc32_data[0] = (crc32 & 0xFF000000) >> 24;
    crc32_data[1] = (crc32 & 0x00FF0000) >> 16;
    crc32_data[2] = (crc32 & 0x0000FF00) >> 8;
    crc32_data[3] = (crc32 & 0x000000FF);

    return 0;
}

/******************************************************************************
Function   : package_get_length
Description: get package length
Input      : data
Output     : none
Return     : length of a package.
******************************************************************************/
unsigned int TLV::package_get_length(unsigned char *data) noexcept {
    //以防大小端问题
    return static_cast<unsigned int>(data[1] << 24 | data[2] << 16 | data[3] << 8 | data[4]);
}

/******************************************************************************
Function   : package_get_crc32
Description: get package crc value
Input      : data
Output     : none
Return     : crc value of a package.
******************************************************************************/
unsigned int TLV::package_get_crc32(unsigned char *data) {
    unsigned char *crc32 = data + MTC_PACKAGE_PROTOCOL_LENGTH + MTC_PACKAGE_LENGTH_LENGTH + this->package_get_length(data);
    return static_cast<unsigned int>(crc32[0] << 24 | crc32[1] << 16 | crc32[2] << 8 | crc32[3]);
}

/******************************************************************************
Function   : package_calc_crc32
Description: calculation package crc value
Input      : data
Output     : none
Return     : crc value of a package.
******************************************************************************/
unsigned int TLV::package_calc_crc32(unsigned char *data, unsigned int length) {
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
