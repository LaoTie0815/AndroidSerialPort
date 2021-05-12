#ifndef TLV_H
#define TLV_H

#define MTC_PACKAGE_PROTOCOL_SERVER		0xA1
#define MTC_PACKAGE_PROTOCOL_CLIENT		0x1A

#define MTC_PACKAGE_PROTOCOL_LENGTH		1
#define MTC_PACKAGE_LENGTH_LENGTH		4
#define MTC_PACKAGE_CRC32_LENGTH		4
#define MTC_PACKAGE_TLV_T_LENGTH		2
#define MTC_PACKAGE_TLV_L_LENGTH		4

class TLV{
private:

    unsigned int package_calc_crc32(unsigned char *data, unsigned int length);

public:
    unsigned short tag = 0;
    unsigned int length = 0;
    unsigned char value[1] = {0};

    TLV()=default;

    static TLV *package_get_first_tlv(unsigned char* data) noexcept;
    static TLV *package_get_next_tlv(unsigned char* data,TLV* tlv) noexcept;
    unsigned int package_get_crc32(unsigned char* data);
    static unsigned int package_get_length(unsigned char* data) noexcept ;
    constexpr unsigned char package_get_protocol(unsigned char* data) noexcept ;
    int package_validity_check(unsigned char* data);
    int package_fill_protocol(unsigned char *data, unsigned char protocol);
    int package_fill_length(unsigned char *data, unsigned int length);
    int package_fill_tlv(unsigned char *data, TLV *tlv);
    int package_fill_crc32(unsigned char *data);
}__attribute__((packed));



#endif // TLV_H
