//
// Created by huangtiebing_vendor on 2020/6/20.
//

#include <assert.h>
#include "Utils.h"


int Utils::hex_to_decimal(unsigned char hex_str) {
    char ch;
    int iret = 0;
    while (ch = hex_str++) {
        iret = (iret << 4) | hex_table[ch];
    }
    return iret;
}

int Utils::hex_char_value(unsigned char c)
{
    if(c >= '0' && c <= '9')
        return c - '0';
    else if(c >= 'a' && c <= 'f')
        return (c - 'a' + 10);
    else if(c >= 'A' && c <= 'F')
        return (c - 'A' + 10);
    return 0;
}

int Utils::getIndexOfSigns(char ch)
{
    if(ch >= '0' && ch <= '9')
    {
        return ch - '0';
    }
    if(ch >= 'A' && ch <='F')
    {
        return ch - 'A' + 10;
    }
    if(ch >= 'a' && ch <= 'f')
    {
        return ch - 'a' + 10;
    }
    return -1;
}

/**function: CharToHex()
*** ACSII change to 16 hex
*** input:ACSII
***Return :Hex
**/
unsigned char Utils::charToHex(unsigned char bHex)
{
    if((bHex>=0)&&(bHex<=9))
    {
        bHex += 0x30;
    }
    else if((bHex>=10)&&(bHex<=15))//Capital
    {
        bHex += 0x37;
    }
    else
    {
        bHex = 0xff;
    }
    return bHex;
}

/**function: CharToHex()
*** ACSII change to 16 hex
*** input:ACSII
***Return :Hex
**/
unsigned char Utils::hexToChar(unsigned char bChar)
{
    if((bChar>=0x30)&&(bChar<=0x39))
    {
        bChar -= 0x30;
    }
    else if((bChar>=0x41)&&(bChar<=0x46)) // Capital
    {
        bChar -= 0x37;
    }
    else if((bChar>=0x61)&&(bChar<=0x66)) //littlecase
    {
        bChar -= 0x57;
    }
    else
    {
        bChar = 0xff;
    }
    return bChar;
}
