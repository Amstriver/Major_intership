/*
Author: @XiaoShuai
Date: 2025-7-8
Description:
        通过按动触摸屏，在屏幕上切换照片

    tftp传输命令：tftp 192.168.1.10 -g -r 文件名
*/

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <linux/input.h>

int screen_fd = -1;
int bmp_fds[3] = {-1, -1, -1};
int *p = NULL;

void display(int single) {
    if (p == NULL) {
        p = mmap(NULL, 800 * 480 * 4, PROT_READ | PROT_WRITE, MAP_SHARED, screen_fd, 0);
        if (p == MAP_FAILED) {
            perror("mmap failed");
            return;
        }
    }

    if (bmp_fds[single] < 0) {
        char filenames[3][20] = {"./test.bmp", "./test1.bmp", "./test2.bmp"};
        bmp_fds[single] = open(filenames[single], O_RDWR);
        if (bmp_fds[single] < 0) {
            perror("bmp open failed");
            return;
        }
    }

    // 读取图片数据
    lseek(bmp_fds[single], 54, SEEK_SET);
    char buf[800 * 480 * 3];
    if (read(bmp_fds[single], buf, sizeof(buf)) != sizeof(buf)) {
        perror("read bmp failed");
        return;
    }

    // 转换为RGB并写入显存
    int i = 0;
    for (int y = 0; y < 480; y++) {
        for (int x = 0; x < 800; x++) {
            char b = buf[i++];
            char g = buf[i++];
            char r = buf[i++];
            *(p + (479 - y) * 800 + x) = (r << 16) | (g << 8) | b;
        }
    }
}

int main() {

    int input_fd = open("/dev/input/event0", O_RDWR);
    screen_fd = open("/dev/fb0", O_RDWR);
    if (input_fd < 0 || screen_fd < 0) {
        perror("open device failed");
        return -1;
    }

    struct input_event ev;
    int ret = 0;

    while (1) {
        if (read(input_fd, &ev, sizeof(ev)) != sizeof(ev)) {
            perror("read input failed");
            break;
        }

        // 检测触摸事件
        if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
            if (ev.value == 1) { // 按下事件
                display(ret);
                ret = (ret + 1) % 3; // 循环切换
            }
        }
    }

    for (int i = 0; i < 3; i++) {
        if (bmp_fds[i] >= 0) close(bmp_fds[i]);
    }
    if (screen_fd >= 0) close(screen_fd);
    if (input_fd >= 0) close(input_fd);

    return 0;
}