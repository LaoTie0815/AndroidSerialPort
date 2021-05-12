//
// Created by huangtiebing_vendor on 2020/6/20.
//

#ifndef DEV_UTILS_H
#define DEV_UTILS_H

static int hex_table[] = {0, 0, 0, 0, 0, 0, 0, 0, 0,
                          0, 0, 0, 0, 0, 0, 0, 0, 0,
                          0, 0, 0, 0, 0, 0, 0, 0, 0,
                          0, 0, 0, 0, 0, 0, 0, 0, 0,
                          0, 0, 0, 0, 0, 0, 0, 0, 0,
                          0, 0, 0, 0, 1, 2, 3, 4, 5, 6,
                          7, 8, 9, 0, 0, 0, 0, 0, 0,
                          0, 10, 11, 12, 13, 14, 15, 0, 0,
                          0, 0, 0, 0, 0, 0, 0, 0, 0,
                          0, 0, 0, 0, 0, 0, 0, 0, 0,
                          0, 0, 0, 0, 0, 0, 10,
                          11, 12, 13, 14, 15};

class Utils {



public:

    static int hex_to_decimal(unsigned char hex_str);

    static int hex_char_value(unsigned char c);

    static int getIndexOfSigns(char ch);

    static unsigned char hexToChar(unsigned char bChar);

    static unsigned char charToHex(unsigned char bHex);

};


#endif //DEV_UTILS_H
