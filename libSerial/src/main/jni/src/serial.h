#ifndef SERIAL_H
#define SERIAL_H

#include <cstddef>
#include <sys/select.h>
#include "definition.h"
#ifdef _WIN32
#include <WinSock2.h>
#include <windows.h>
#endif

typedef void(serialCallbackFn)(class Serial* serail,void* priv_data);

typedef struct UART{
#ifdef _WIN32
    int fd[16] = {0};
#else
    int fd;
    fd_set fds;
#endif
    int pipe_fd[2];
    int speed;
    int stopBits;
    int dataBits;
    int parity;
    char path[UART_MAX_PATH];
}UART;

class Serial{
public:
    Serial()=default;
//    Serial(const char* device,const int &speed,const int &stopBits,const int &dataBits,const int &parity);
    ~Serial();

    int init(const char* device,const int &speed,const int &stop_bits,const int &data_bits,const int &parity);
    void destroy() noexcept;
    int cleanSeriaBuffer() noexcept ;
    void uartReadData(serialCallbackFn *cb, void *priv_data);
    int uartWriteData(const unsigned char *data, size_t length);
    int getFD() noexcept ;

private:
    UART m_uart = {0};
    volatile bool m_is_init = false;
    int uartOpen(const char *device);
    int uartClose();
    int uartSetAttr();
};
#endif // SERIAL_H
