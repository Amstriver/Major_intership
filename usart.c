#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include<termios.h> 

int Serial_Init(int fd)
{
    /* 1、保留原先串口配置 */
    struct termios new_cfg, old_cfg;
    if (tcgetattr(fd, &old_cfg) != 0)
    {
        perror("tcgetattr failed");
        return -1;
    }
    /* 2、激活选项 */
    new_cfg.c_cflag  |=  CLOCAL | CREAD; 

    /* 3、设置波特率 */
    cfsetispeed(&new_cfg, B115200); 
    cfsetospeed(&new_cfg, B115200);

    /* 4、设置字符大小 */
    new_cfg.c_cflag &= ~CSIZE; // 用数据位掩码清空数据位设置 
    new_cfg.c_cflag |= CS8;

    /* 5、设置校验位 */
    new_cfg.c_cflag &= ~PARENB; // 无校验位

    /* 6、设置停止位 */
    new_cfg.c_cflag &= ~CSTOPB; // 将停止位设置为一个比特

    /* 7、设置最小字符和等待时间 */
    new_cfg.c_cc[VTIME] = 0;
    new_cfg.c_cc[VMIN] = 1; // 最少读取一个字符

    /* 8、清除串口缓存 */
    tcflush(fd, TCIFLUSH); 

    /* 9、激活配置 */
    if (tcsetattr(fd, TCSANOW, &new_cfg) != 0)
    {
        perror("tcsetattr");
        return -1;
    }
}

int main(int argc, char const *argv[])
{
    /* 打开串口 */
    int Serial_fd = open("/dev/ttySAC1", O_RDWR | O_NOCTTY);
    if (Serial_fd < 0)
    {
        perror("Serial_fd open failed");
        return -1;
    }
    /* 配置串口参数 */
    Serial_Init(Serial_fd);

    /* 写入数据，自己写自己读 */
    // write(Serial_fd, "hello", 5);
    // sleep(1);

    /* 延时一秒后读取数据 */
    char buf[50] = {0};
    read(Serial_fd, buf, 50);
    printf("buf = %s\n", buf);

    /* 关闭串口 */
    close(Serial_fd);
    
    return 0;
}