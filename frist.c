/*
Author: @XiaoShuai
Date: 2025-7-7
Description:
        使用open, close, read, write函数实现文件复制
        也可以实现视频、音频、图片的复制

*/

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{   
    char buf[1024];  // 修改缓冲区大小可以减少系统调用次数，从而减少时间
    ssize_t bytes_read;

    int fd = open("test.txt", O_RDWR, 0777);
    int fdcp = open("test_copy.txt", O_RDWR | O_CREAT, 0777);
    if (fd == -1 || fdcp == -1) {
        perror("open failed");
        return -1;
    }

    while ((bytes_read = read(fd, buf, sizeof(buf))) > 0) {
        write(fdcp, buf, bytes_read);
    }

    close(fdcp);
    close(fd);
    return 0;
}