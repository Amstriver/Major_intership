#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "MQTTClient.h"
#include "font.h"

#define SERIAL_PORT "/dev/ttySAC1"
#define BROKER_IP "mqtts.heclouds.com"
#define CLIENT_ID "major_test"
#define USERNAME "b135zUaXpg"
#define PASSWORD "version=2018-10-31&res=products%2Fb135zUaXpg%2Fdevices%2Fmajor_test&et=8888888888&method=md5&sign=CVaVwvCrF0B8c1kkeHRTcA%3D%3D"

#define MQTT_PUBLISH_TOPIC "$sys/b135zUaXpg/major_test/thing/property/post"
#define MQTT_SUBSCRIBE_TOPIC "$sys/b135zUaXpg/major_test/thing/property/post/reply"

MQTTClient client;

int Serial_Init(int fd) {
    struct termios new_cfg, old_cfg;
    if (tcgetattr(fd, &old_cfg) != 0) {
        perror("tcgetattr failed");
        return -1;
    }
    new_cfg = old_cfg;
    new_cfg.c_cflag |= CLOCAL | CREAD;
    cfsetispeed(&new_cfg, B115200);
    cfsetospeed(&new_cfg, B115200);
    new_cfg.c_cflag &= ~CSIZE;
    new_cfg.c_cflag |= CS8;
    new_cfg.c_cflag &= ~PARENB;
    new_cfg.c_cflag &= ~CSTOPB;
    new_cfg.c_cc[VTIME] = 0;
    new_cfg.c_cc[VMIN] = 1;
    tcflush(fd, TCIFLUSH);
    if (tcsetattr(fd, TCSANOW, &new_cfg) != 0) {
        perror("tcsetattr");
        return -1;
    }
    return 0;
}

void connect_lost(void *context, char *cause) {
    printf("失去连接，原因: %s\n", cause);
}

int message_arrived(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    printf("接收到主题：%s\n", topicName);
    printf("内容：%.*s\n", message->payloadlen, (char*)message->payload);
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

void delivery_complete(void *context, MQTTClient_deliveryToken dt) {
    printf("主题发布成功，token = %d\n", dt);
}

// 解析串口数据并发送MQTT和显示在屏幕
void parse_and_publish_and_display(const char *buf) {
    const char *temp_ptr = strstr(buf, "Temperature:");
    const char *humi_ptr = strstr(buf, "Humidity:");
    const char *light_ptr = strstr(buf, "Light:");

    if (temp_ptr && humi_ptr && light_ptr) {
        int temperature = atoi(temp_ptr + strlen("Temperature:"));
        int humidity = atoi(humi_ptr + strlen("Humidity:"));
        int light = atoi(light_ptr + strlen("Light:"));

        // 显示在屏幕上
        char display_buf[64];
        Clean_Area(0, 0, 800, 150, 0xffffff);  // 清空上一次显示区域

        snprintf(display_buf, sizeof(display_buf), "Temperature: %d C", temperature);
        Display_characterX(50, 20, display_buf, 0xff0000, 2);

        snprintf(display_buf, sizeof(display_buf), "Humidity: %d %%", humidity);
        Display_characterX(50, 60, display_buf, 0x00aa00, 2);

        snprintf(display_buf, sizeof(display_buf), "Light: %d / 100", light);
        Display_characterX(50, 100, display_buf, 0x0000ff, 2);

        // MQTT JSON 构造
        char json_payload[512];
        snprintf(json_payload, sizeof(json_payload),
            "{\n"
            "  \"id\": \"123\",\n"
            "  \"version\": \"1.0\",\n"
            "  \"params\": {\n"
            "    \"Hum\": { \"value\": %d },\n"
            "    \"Tem\": { \"value\": %d },\n"
            "    \"Light\": { \"value\": %d }\n"
            "  }\n"
            "}", humidity, temperature, light);

        MQTTClient_message pubmsg = MQTTClient_message_initializer;
        pubmsg.payload = json_payload;
        pubmsg.payloadlen = (int)strlen(json_payload);
        pubmsg.qos = 1;
        pubmsg.retained = 0;
        MQTTClient_deliveryToken token;

        printf("发布MQTT数据：\n%s\n", json_payload);
        int rc = MQTTClient_publishMessage(client, MQTT_PUBLISH_TOPIC, &pubmsg, &token);
        if (rc != MQTTCLIENT_SUCCESS) {
            printf("发布失败，返回码：%d\n", rc);
        }
        MQTTClient_yield();
    } else {
        printf("接收到格式错误的数据：%s\n", buf);
    }
}

int main() {
    Init_Font();
    Clean_Area(0, 0, 800, 480, 0xffffff);

    // 初始化串口
    int serial_fd = open(SERIAL_PORT, O_RDWR | O_NOCTTY);
    if (serial_fd < 0) {
        perror("打开串口失败");
        return -1;
    }
    if (Serial_Init(serial_fd) != 0) {
        close(serial_fd);
        return -1;
    }

    // 初始化MQTT
    int rc = MQTTClient_create(&client, BROKER_IP, CLIENT_ID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (rc != MQTTCLIENT_SUCCESS) {
        printf("创建MQTT客户端失败，返回码：%d\n", rc);
        return -1;
    }

    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.username = USERNAME;
    conn_opts.password = PASSWORD;
    conn_opts.keepAliveInterval = 60;
    conn_opts.cleansession = 1;

    MQTTClient_setCallbacks(client, NULL, connect_lost, message_arrived, delivery_complete);

    rc = MQTTClient_connect(client, &conn_opts);
    if (rc != MQTTCLIENT_SUCCESS) {
        printf("MQTT连接失败，返回码：%d\n", rc);
        return -1;
    }
    printf("MQTT连接成功\n");

    MQTTClient_subscribe(client, MQTT_SUBSCRIBE_TOPIC, 1);

    // 读取串口数据并处理
    char buf[128] = {0};
    int i = 0;
    char ch;
    while (1) {
        if (read(serial_fd, &ch, 1) > 0) {
            if (ch == '\n' || i >= sizeof(buf) - 1) {
                buf[i] = '\0';
                parse_and_publish_and_display(buf);
                i = 0;
                memset(buf, 0, sizeof(buf));
            } else {
                buf[i++] = ch;
            }
        }
    }

    close(serial_fd);
    MQTTClient_disconnect(client, 1000);
    MQTTClient_destroy(&client);
    return 0;
}
