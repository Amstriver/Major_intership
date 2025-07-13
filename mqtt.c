/*
    终端执行：arm-linux-gcc mqtt.c -L ~/paho.mqtt.c-1.3.13/build/output/ -lpaho-mqtt3c -I ~/paho.mqtt.c-1.3.13/src/

    udhcpc
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "MQTTClient.h"

#define BROKER_IP "mqtts.heclouds.com"
#define BROKER_PORT "1883"
#define CLIENT_ID "major_test"
#define USERNAME "b135zUaXpg"
#define PASSWORD "version=2018-10-31&res=products%2Fb135zUaXpg%2Fdevices%2Fmajor_test&et=8888888888&method=md5&sign=CVaVwvCrF0B8c1kkeHRTcA%3D%3D"

#define MQTT_PUBLISH_TOPIC "$sys/b135zUaXpg/major_test/thing/property/post"   // 发布
#define MQTT_SUBSCRIBE_TOPIC "$sys/b135zUaXpg/major_test/thing/property/post/reply"  // 订阅
#define QOS_LEVEL 1  // QoS1等级

// 失去连接回调函数
void connect_lost(void *context, char *cause)
{
    printf("失去连接,The reason: %s \n",cause);
}
// 收到主题信息回调函数
int message_arrived(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    printf("Receive topic: %s, message data: \n", topicName);
    printf("%.*s\n", message->payloadlen, (char*)message->payload);
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

// 主题发布成功回调函数
void delivery_complete(void *context, MQTTClient_deliveryToken dt)
{
    printf("主题发布成功,token = %d \n", dt);
}

int main(int argc, char const *argv[])
{
    // 1、定义一个MQTT客户端结构体指针
    MQTTClient client;
    // 2、创建一个MQTT客户端
    int rc;
    if ((rc = MQTTClient_create(&client, BROKER_IP, CLIENT_ID, MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to create client, return code %d\n", rc);
        return -1;
    }

    // 3、创建一个MQTT连接配置结构体，并配置其参数
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.username = USERNAME;          // 用户名 
    conn_opts.password = PASSWORD;          // 用户名对应的密码
    conn_opts.keepAliveInterval = 60;       // 心跳时间
    conn_opts.cleansession = 1;             // 清除会话
    
    // 4、设置MQTT连接时的回调函数
    if ((rc = MQTTClient_setCallbacks(client, NULL, connect_lost, message_arrived, delivery_complete)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to set callbacks, return code %d\n", rc);
        rc = EXIT_FAILURE;
        return -1;
    }
    // 5、开始连接到MQTT服务器
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to connect, return code %d\n", rc);
        exit(EXIT_FAILURE);
        return -1;
    }
    printf("连接成功\n");
    
    // 6、订阅主题
    if ((rc = MQTTClient_subscribe(client, MQTT_SUBSCRIBE_TOPIC, 1)) != MQTTCLIENT_SUCCESS)
    {
        printf("订阅主题失败, return code %d\n", rc);
        rc = EXIT_FAILURE;
        return -1;
    }

    // 7、定义一个主题消息存储结构体
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    char mag_data[500]; // 确保足够大的缓冲区
    srand(time(NULL));
    int x, y;
    while (1)
    {
        x = rand()%100;
        y = rand()%100;
        sprintf(mag_data, 
        "{\n"
        "    \"id\": \"123\",\n"
        "    \"version\": \"1.0\",\n"
        "    \"params\":{\n"
        "        \"Hum\":{\n"
        "            \"value\":%d\n"
        "        },\n"
        "        \"Tem\":{\n"
        "            \"value\":%d\n"
        "        }\n"
        "    }\n"
        "}"
        ,60, 10);
        pubmsg.payload = mag_data;
        pubmsg.payloadlen = (int)strlen(mag_data);
        pubmsg.qos = 1;                 // qos等级为1 
        pubmsg.retained = 0;            // 服务器不保留消息
        MQTTClient_deliveryToken token; // 标记MQTT消息的值，用来检查消息是否发送成功
        printf("%s\n", mag_data);
        // 7、发布主题信息
        if ((rc = MQTTClient_publishMessage(client, MQTT_PUBLISH_TOPIC, &pubmsg, &token)) != MQTTCLIENT_SUCCESS)
        {
            printf("Failed to publish message, return code %d\n", rc);
            exit(EXIT_FAILURE);
        }
        MQTTClient_yield();  // 保持网络连接
        sleep(3);
    }
    
    return 0;
}