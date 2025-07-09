/*
Author: @XiaoShuai
Date: 2025-7-8
Description:
        使用open, close, read, write函数实现
        1. 打开/dev/fb0设备文件
        2. 将屏幕的像素数据映射到内存中
        3. 修改内存中的像素数据
        4. 将修改后的数据写回到屏幕上
        5. 关闭设备文件

    tftp传输命令：tftp 192.168.1.10 -g -r 文件名
*/

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>


int main(int argc, char const *argv[])
{
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0)
    {
        perror("lcd open false");
        return -1;
    }

    // 分配内存
    int *p = mmap(NULL, 800*480*4, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == NULL)
    {
        perror("mmap false");
        return -1;
    }

    // 读取图片
    int bmp_fd = open("./test.bmp", O_RDWR);
    if (bmp_fd < 0)
    {
        perror("bmp open false");
        return -1;
    }

    // 读取54字节信息头
    char head[54];
    read(bmp_fd, head, 54);

    // 读取图片数据
    char buf[800*480*3];
    read(bmp_fd, buf, sizeof(buf));

    int i = 0, value = 0;
    for(int y = 0; y < 480; y++)
    {
        for(int x = 0; x < 800; x++)
        {
            char b = buf[i++];
            char g = buf[i++];
            char r = buf[i++];

            value = b | (g << 8) | (r << 16);
            *(p + (479 - y) * 800 + x) = value;
        }
    }
    
    // 直接写数据方法
    /*int buf[800*480];
    for(int i = 0; i < 800*480; i++)
    {
        buf[i] = 0xff0000;  // 红色
    }

    write(fd, buf, sizeof(buf));  // 会产生毛刺，不能一时间就填充整个屏幕
    */

    // 映射方法
    /*int *p = mmap(NULL, 800*480*4, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == NULL)
    {
        perror("mmap false");
        return -1;
    }
    // 每120行显示一个颜色，红橙黄绿,显示屏为800*480
    for (int i = 0; i < 480; i++)
    {
        for (int j = 0; j < 800; j++)
        {
            if (i < 120)
            {
                *(p+i*800+j) = 0xff0000;  // 红色
            }
            else if (i < 240){
                *(p+i*800+j) = 0xffa500;  // 橙色
            }
            else if (i < 360){
                *(p+i*800+j) = 0xffff00;  // 黄色
            }
            else{
                *(p+i*800+j) = 0x00ff00;  // 绿色
            }
        }
    }
    */
    close(bmp_fd);
    close(fd);

    return 0;
}