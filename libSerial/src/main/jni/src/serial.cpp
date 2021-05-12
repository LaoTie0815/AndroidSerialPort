#include "serial.h"
#include "mtc.h"
#include <fcntl.h>
#include <cstdio>
#include <termios.h>
#include <cstring>
#include <cerrno>
#include <sys/select.h>
//#include <sys/stat.h>
//#include <sys/types.h>
#include <unistd.h>
#include <malloc.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>

/******************************************************************************
Function   : getBaudrate
Description: get serial port transfer baudrate.
Input      : speed
Output     : none
Return     : baudrate
******************************************************************************/
static speed_t getBaudrate(int speed) {
    switch (speed) {
        case 0:
            return B0;
        case 50:
            return B50;
        case 75:
            return B75;
        case 110:
            return B110;
        case 134:
            return B134;
        case 150:
            return B150;
        case 200:
            return B200;
        case 300:
            return B300;
        case 600:
            return B600;
        case 1200:
            return B1200;
        case 1800:
            return B1800;
        case 2400:
            return B2400;
        case 4800:
            return B4800;
        case 9600:
            return B9600;
        case 19200:
            return B19200;
        case 38400:
            return B38400;
        case 57600:
            return B57600;
        case 115200:
            return B115200;
        case 1500000:
            return B1500000;
        case 2000000:
            return B2000000;
        case 3000000:
            return B3000000;
        case 3500000:
            return B3500000;
        case 4000000:
            return B4000000;
        default:
            return 0;//-1
    }
}

/******************************************************************************
Function   : ~Serial
Description: Destructor.
Input      : none
Output     : none
Return     : none
******************************************************************************/
Serial::~Serial() {
    this->destroy();
}

/******************************************************************************
Function   : init
Description: initialize config and data.
Input      : device: the dev path.
             speed:  the uart transfer baudrate.
             stopBits: the uart transfer data stop bit.
             dataBits: the uart transfer data width bit.
             parity: does the transmitted data need parity? 0 is none, 1 is odd，2 is even.
Output     : none
Return     : success: 0
             failure: uart_open() failure return value ,or uart_set_attr() failure return value.
******************************************************************************/
int Serial::init(const char *device, const int &speed, const int &stop_bits, const int &data_bits, const int &parity) {

    this->m_uart.speed = speed;
    this->m_uart.stopBits = stop_bits;
    this->m_uart.dataBits = data_bits;
    this->m_uart.parity = parity;
    strcpy(this->m_uart.path, device);
    if(this->m_is_init) this->destroy();
    int ret = this->uartOpen(this->m_uart.path);
    LOGI("%s %d: The Serial init result: %d", __FUNCTION__, __LINE__, ret);
    if(ret) return ret;
    if((ret = pipe(this->m_uart.pipe_fd)) != 0) return ret;
    if((ret = this->uartSetAttr()) != 0) return ret;
    LOGI("%s %d: The Serial set attr result: %d", __FUNCTION__, __LINE__, ret);
    this->m_is_init = true;
    return 0;
}

/******************************************************************************
Function   : destroy
Description: destroy all the data in the heap.
Input      : none
Output     : none
Return     : none
******************************************************************************/
void Serial::destroy() noexcept {
    if(this->m_is_init)
    {
        LOGV("destroy.");
        if(write(this->m_uart.pipe_fd[1],"0",1)>=0){
            int try_count = 10;
            do{
                usleep(100);
                --try_count;
                if(!try_count) break;
            }while (this->m_is_init);
        }
        this->m_is_init = false;
        this->uartClose();
    }
}

/******************************************************************************
Function   : cleanSeriaBuffer
Description: clean all the data in serial buffer.
Input      : none
Output     : none
Return     : 0 is success, other is failed.
******************************************************************************/
int Serial::cleanSeriaBuffer() noexcept {
    if (!this->m_is_init)
        return -1;
    return tcflush(this->m_uart.fd,TCIOFLUSH);
}

/******************************************************************************
Function   : uartReadData
Description: read data from uart.
Input      : cb: callback function.
             privData: user's private data.
Output     : none
Return     : none
******************************************************************************/
void Serial::uartReadData(serialCallbackFn *cb, void *priv_data) {
    if (!this->m_is_init)
        return;

    fd_set total_fds;
    fd_set lst_fds;
    int max_fd = 0;

    while (this->m_is_init) {
        FD_ZERO(&total_fds);
        FD_SET(this->m_uart.fd,&total_fds);
        FD_SET(this->m_uart.pipe_fd[0],&total_fds);
        max_fd = this->m_uart.fd > this->m_uart.pipe_fd[0] ? this->m_uart.fd + 1 : this->m_uart.pipe_fd[0] + 1;
        int try_count = 15;
        while(true){
            memcpy(&lst_fds,&total_fds,sizeof(fd_set));
            int ret_fd = select(max_fd, &lst_fds, nullptr, nullptr, nullptr);
            LOGV("event fd: %d",ret_fd);
            if (ret_fd <= 0) {
                int err = errno;
                LOGE("errno:%d, errstr: %s", err, strerror(err));
                if (EAGAIN == err|| EINTR == err) {
                    usleep(1000);
                    --try_count;
                    if(EAGAIN == err && !try_count){
                        this->destroy();
                        cb(this,priv_data);
                        break;
                    }
                    continue;
                }
                break; //串口未知的异常状态,重新打开串口.
            }
            if(FD_ISSET(this->m_uart.fd,&lst_fds))
            {
                cb(this, priv_data);
            }else{
                LOGW("get close notify.");
                int read_len = 0;
                const int buf_len = 8;
                char buf[buf_len] = {0};
                do{
                    read_len = read(this->m_uart.pipe_fd[0],buf,buf_len);
                }while (read_len == buf_len);
                this->m_is_init = false;
                break;//return;
            }
        }
        // if init is true. need reopen uart
        if(!this->m_is_init)
            break;
        this->uartClose();
        this->uartOpen(this->m_uart.path);
        this->uartSetAttr();
    }
    LOGW("uartReadData exit.");
}


/******************************************************************************
Function   : uartWriteData
Description: write data to uart.
Input      : data: raw data.
             length: raw data's length.
Output     : none
Return     : success: 0.
             failure: ERR_WRITE_RETRY_OUT: retry failed.
                      ERR_INVALID_FD: invalid file descriptor.
******************************************************************************/
int Serial::uartWriteData(const unsigned char *data, size_t length) {
    if (!this->m_is_init)
        return false;
    ssize_t write_length = 0;
    size_t write_pos = 0;
    int retry_count = 10;
    while (retry_count > 0) {
        write_length = write(this->m_uart.fd, data + write_pos, length - write_pos);
        LOGV("write_length: %lu",write_length);
        if (write_length <= 0) {
            if (errno == EAGAIN || errno == EINTR) {
                usleep(10000);
                retry_count--;
                continue;
            }
            return ERR_INVALID_FD;
        }
        write_pos += size_t(write_length);
        LOGV("write: m_uart.fd:%d,write_pos: %lu, length: %lu\n",this->m_uart.fd,write_pos,length);
        if (write_pos == length)
            return 0;
    }
    return ERR_WRITE_RETRY_FAILED;
}

/******************************************************************************
Function   : getFD
Description: get file descripter
Input      : none
Output     : none
Return     : success: fd>0.
             failure: ERR_NOT_INIT : failed,it's mean not initialize.
******************************************************************************/
int Serial::getFD() noexcept {
    if (this->m_is_init)
        return this->m_uart.fd;
    return ERR_NOT_INIT;
}

/******************************************************************************
Function   : uartOpen
Description: open the uart
Input      : device: dev path.
Output     : none
Return     : success: 0
             failure: ERR_ALREADY_INIT: already init.
                      ERR_CANNOT_OPEN: can't open serial port,may not have permission.
                      ERR_CANNOT_FCNTL: catn't set fcntl.
******************************************************************************/
int Serial::uartOpen(const char *device) {
#ifdef _WIN32
    HANDLE pCom = CreateFileA(device,
                              GENERIC_READ | GENERIC_WRITE, //支持读写
                              0,                //独占方式，串口不支持共享
                              nullptr,          //安全属性指针，默认值为NULL
                              OPEN_EXISTING,    //打开现有的串口文件
                              0,                //0：同步方式，FILE_FLAG_OVERLAPPED：异步方式
                              nullptr);         //用于复制文件句柄，默认值为NULL，对串口而言该参数必须置为NULL);
    if(pCom==HANDLE(-1))
        return ERR_CANNOT_OPEN;
    if(!SetupComm(pCom,4096,4096))
        return ERR_CANNOT_OPEN;
#else
//    if(this->m_is_connect)
//        return ERR_ALREADY_INIT; //already init.
    if(this->m_is_init)
        return ERR_ALREADY_INIT;
    int fd;
    fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    LOGD("device:%s, fd:%d", device, fd);
//    fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
    if (-1 == fd) {
        perror("Can't OpenSerial Port");
        return ERR_CANNOT_OPEN;
    }
    if (fcntl(fd, F_SETFL, 0) < 0) {//设为阻塞状态
        perror("fcntl setfl failed!\n");
        return ERR_CANNOT_FCNTL;
    } else {
        printf("fcntl=%d\n", fcntl(fd, F_SETFL, 0));
    }
    this->m_uart.fd = fd;
#endif
    return 0;
}

int Serial::uartClose() {
    if(this->m_uart.fd > 0){
        close(this->m_uart.fd);
        this->m_uart.fd = 0;
    }
    return 0;
}

/******************************************************************************
Function   : uartSetAttr
Description: set the IO fd's attr of uart.
Input      : none
Output     : none
Return     : success: 0
             failure: ERR_CANNOT_SET_ATTR: failed.
******************************************************************************/
int Serial::uartSetAttr() {
    struct termios ios;
    if (tcgetattr(this->m_uart.fd, &ios) != 0) {
        perror("can't setup serial device.");
        this->uartClose();
        return ERR_CANNOT_SET_ATTR;
    }
    speed_t speed = getBaudrate(this->m_uart.speed);
    cfmakeraw(&ios);
    //设置速率
    cfsetispeed(&ios, speed);
    cfsetospeed(&ios, speed);
    //设置字符
    ios.c_cflag |= CLOCAL | CREAD;
    ios.c_cflag &= ~CSIZE;

    //设置数据位
    switch (this->m_uart.dataBits) {
        case 5:
            ios.c_cflag |= CS5;
            break;
        case 6:
            ios.c_cflag |= CS6;
            break;
        case 7:
            ios.c_cflag |= CS7;
            break;
        case 8:
            ios.c_cflag |= CS8;
            break;
        default:
            ios.c_cflag |= CS8;
            break;
    }

    //设置奇偶校验
    switch (this->m_uart.parity) {
        case 0:
            ios.c_cflag &= ~PARENB;
            break;
        case 1:
            ios.c_cflag |= PARENB;
            ios.c_cflag |= PARODD;
            ios.c_iflag |= (INPCK | ISTRIP);
            break;
        case 2:
            ios.c_iflag |= (INPCK | ISTRIP);
            ios.c_cflag |= PARENB;
            ios.c_cflag &= ~PARODD;
            break;
        default:
            ios.c_cflag &= ~PARENB;
            break;
    }

    //设置停止位
    switch (this->m_uart.stopBits) {
        case 1:
            ios.c_cflag &= ~CSTOPB;
            break;
        case 2:
            ios.c_cflag |= CSTOPB;
            break;
        default:
            ios.c_cflag &= ~CSTOPB;
            break;
    }

    if ((tcsetattr(this->m_uart.fd, TCSANOW, &ios)) != 0) {
        perror("serial set error.");
        return ERR_CANNOT_SET_ATTR;
    }
    return tcflush(this->m_uart.fd,TCIOFLUSH);
}
