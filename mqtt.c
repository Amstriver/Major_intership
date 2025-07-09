#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <pthread.h>

// 阿里云连接参数（请替换为你的实际参数）
#define BROKER_IP "k1yyawzwj2G.iot-as-mqtt.cn-shanghai.aliyuncs.com"
#define BROKER_PORT "1883"
#define CLIENT_ID "0001|securemode=3,signmethod=hmacsha1|"
#define USERNAME "lijiang&k1yyawzwj2G"
#define PASSWORD "85A79F99FD391584D83E82ED69DBEB9F7C9A00DA"

#define	MQTT_PUBLISH_TOPIC "/sys/k1yyawzwj2G/lijiang/thing/event/property/post"   // 发布
#define MQTT_SUBSCRIBE_TOPIC "/sys/k1yyawzwj2G/lijiang/thing/service/property/set"  // 订阅
#define QOS_LEVEL 1  // QoS1等级

// 创建TCP连接
int create_socket() 
{
    struct addrinfo hints, *result, *rp;
    int sockfd = -1;

    // 设置DNS查询参数
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    // 允许IPv4或IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP socket

    // 执行DNS解析
    int ret = getaddrinfo(BROKER_IP, BROKER_PORT, &hints, &result);
    if (ret != 0) {
        fprintf(stderr, "DNS解析失败: %s\n", gai_strerror(ret));
        return -1;
    }

    // 遍历所有可能的地址直到连接成功
    for (rp = result; rp != NULL; rp = rp->ai_next) 
    {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1) continue;

        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1) 
        {
            break; // 连接成功
        }

        close(sockfd); // 连接失败，关闭当前socket
        sockfd = -1;
    }

    freeaddrinfo(result); // 释放DNS结果

    if (rp == NULL) { // 所有地址尝试失败
        fprintf(stderr, "无法连接到服务器\n");
        return -1;
    }

    return sockfd;
}

// 构造CONNECT报文
unsigned char* build_connect_packet(int* packet_len) 
{
    // 协议名 MQTT (UTF-8 encoded)
    unsigned char protocol_name[] = {0x00,0x04,'M','Q','T','T'};
    
    // 协议级别 4 (0x04)
    unsigned char protocol_level = 0x04;
    
    // 连接标志 (Clean Session=1, 用户名/密码=1)
    unsigned char connect_flags = 0xC2; // 11000010
    
    // Keep Alive 60秒
    unsigned char keep_alive[] = {0x00, 0x3C};
    
    // 构造载荷
    unsigned char client_id[2 + strlen(CLIENT_ID)];
    client_id[0] = 0x00;
    client_id[1] = strlen(CLIENT_ID);
    memcpy(client_id+2, CLIENT_ID, strlen(CLIENT_ID));
    
    unsigned char username[2 + strlen(USERNAME)];
    username[0] = 0x00;
    username[1] = strlen(USERNAME);
    memcpy(username+2, USERNAME, strlen(USERNAME));
    
    unsigned char password[2 + strlen(PASSWORD)];
    password[0] = 0x00;
    password[1] = strlen(PASSWORD);
    memcpy(password+2, PASSWORD, strlen(PASSWORD));
    
    // 计算总长度
    int payload_len = sizeof(client_id) + sizeof(username) + sizeof(password);
    int variable_header_len = 10; // protocol(6)+level(1)+flags(1)+keepalive(2)
    int remaining_length = variable_header_len + payload_len;
    
    // 构造报文
    *packet_len = 1 + (remaining_length > 127 ? 2 : 1) + remaining_length;
    unsigned char* packet = malloc(*packet_len);
    
    // 固定头
    packet[0] = 0x10; // CONNECT type
    packet[1] = remaining_length; // 假设长度小于128
    
    // 可变头
    memcpy(packet+2, protocol_name, 6);
    packet[8] = protocol_level;
    packet[9] = connect_flags;
    memcpy(packet+10, keep_alive, 2);
    
    // 载荷
    int pos = 12;
    memcpy(packet+pos, client_id, sizeof(client_id));
    pos += sizeof(client_id);
    memcpy(packet+pos, username, sizeof(username));
    pos += sizeof(username);
    memcpy(packet+pos, password, sizeof(password));
    
    return packet;
}

// 发布数据报文
unsigned char* build_publish_packet(const char* topic, const char* data, int qos, int* packet_len) 
{
    // 1. 构造主题长度（UTF-8编码，大端序）
    uint16_t topic_len = strlen(topic);
    unsigned char topic_len_bytes[2];
    topic_len_bytes[0] = (topic_len >> 8) & 0xFF; // 高位字节
    topic_len_bytes[1] = topic_len & 0xFF;        // 低位字节

    // 2. 计算剩余长度（可变字节编码）
    int remaining_len = 2 + topic_len + strlen(data); // QoS 0
    if(qos > 0) remaining_len += 2; // 添加报文ID字段
    
    // 3. 构造固定头
    unsigned char fixed_header[5];
    int header_size = 1;
    fixed_header[0] = 0x30 | ((qos & 0x03) << 1); // PUBLISH + QoS
    
    // 剩余长度编码（支持最大127长度简化版）
    if(remaining_len <= 127) {
        fixed_header[1] = remaining_len;
        header_size = 2;
    } else {
        // 处理多字节编码（此处简化，建议实现完整编码）
        fixed_header[1] = 0x80 | (remaining_len & 0x7F);
        fixed_header[2] = remaining_len >> 7;
        header_size = 3;
    }

    // 4. 分配内存并组装报文
    *packet_len = header_size + 2 + topic_len + strlen(data);
    unsigned char* packet = malloc(*packet_len);
    
    // 拷贝固定头
    memcpy(packet, fixed_header, header_size);
    int pos = header_size;
    
    // 拷贝主题长度和主题
    memcpy(packet+pos, topic_len_bytes, 2);
    pos += 2;
    memcpy(packet+pos, topic, topic_len);
    pos += topic_len;
    
    // 拷贝载荷数据
    memcpy(packet+pos, data, strlen(data));
    
    return packet;
}

// 示例使用方式（在成功连接后添加）：
void publish_sensor_data(int sockfd)
{
    // 构造符合阿里云物模型的数据（JSON格式）
    const char* payload = 
        "{"
        "\"id\":\"123456\","     // 消息ID
        "\"version\":\"1.0\","   // 协议版本
        "\"params\":{"           // 设备参数
            "\"temperature\":60,"
            "\"Humidity\":56" //
        "},"
        "\"method\":\"thing.event.property.post\""  // 指定方法
        "}";
    
    const char* topic = MQTT_PUBLISH_TOPIC;
    
    int packet_len;
    unsigned char* packet = build_publish_packet(topic, payload, 0, &packet_len);
    
    if(send(sockfd, packet, packet_len, 0) != packet_len) {
        perror("Publish failed");
    } else {
        printf("数据上报成功!\n");
    }
    
    free(packet);
}

// 心跳包发送函数
void send_heartbeat(int sockfd) 
{
    // PINGREQ报文格式：0xC0 0x00
    const unsigned char pingreq_packet[] = {0xC0, 0x00};
    
    ssize_t sent = send(sockfd, pingreq_packet, sizeof(pingreq_packet), 0);
    if(sent != sizeof(pingreq_packet)) {
        perror("Heartbeat send failed");
        // 添加重连逻辑
    } else {
        printf("==> Sent PINGREQ heartbeat\n");
    }
}

// 订阅报文构造函数 
unsigned char* build_subscribe_packet(const char* topic, uint16_t packet_id, int* packet_len) 
{
    // 1. 准备报文标识符
    unsigned char packet_id_bytes[2] = {packet_id >> 8, packet_id & 0xFF};
    
    // 2. 计算主题长度
    uint16_t topic_len = strlen(topic);
    unsigned char topic_len_bytes[2] = {topic_len >> 8, topic_len & 0xFF};
    
    // 3. 计算载荷长度
    int payload_len = 2 + topic_len + 1;  // 主题长度字段(2) + 主题内容 + QoS字节(1)
    
    // 4. 计算剩余长度 (可变头2字节 + 载荷)
    int remaining_len = 2 + payload_len;
    
    // 5. 构造固定头 (SUBSCRIBE类型 + QoS1)
    unsigned char fixed_header[2] = {
        0x82,  // SUBSCRIBE (0x80) | QoS1 (0x02)
        (unsigned char)remaining_len  // 剩余长度 < 128
    };
    
    // 6. 分配内存并组装报文
    *packet_len = 2 + remaining_len;
    unsigned char* packet = malloc(*packet_len);
    if (!packet) return NULL;
    
    int pos = 0;
    memcpy(packet + pos, fixed_header, 2);
    pos += 2;
    
    // 可变头 (报文标识符)
    memcpy(packet + pos, packet_id_bytes, 2);
    pos += 2;
    
    // 载荷 - 主题部分
    memcpy(packet + pos, topic_len_bytes, 2);
    pos += 2;
    memcpy(packet + pos, topic, topic_len);
    pos += topic_len;
    
    // QoS级别 (QoS1)
    packet[pos] = QOS_LEVEL;
    
    return packet;
}
// 订阅主题并等待确认
int mqtt_subscribe(int sockfd, const char* topic) 
{
    // 生成随机的报文标识符 (0-65535)
    uint16_t packet_id = rand() % 65535;
    
    // 1. 构建订阅报文
    int packet_len;
    unsigned char* packet = build_subscribe_packet(topic, packet_id, &packet_len);
    if (!packet) {
        perror("订阅报文构建失败");
        return -1;
    }
    
    // 2. 发送订阅请求
    if (send(sockfd, packet, packet_len, 0) != packet_len) {
        perror("订阅请求发送失败");
        free(packet);
        return -1;
    }
    free(packet);
    printf("订阅请求已发送: %s (报文ID: %d)\n", topic, packet_id);
    
    // 3. 等待并解析SUBACK响应
    unsigned char response[5]; // SUBACK固定5字节
    ssize_t bytes_read = recv(sockfd, response, sizeof(response), 0);
    
    if (bytes_read < 5) {
        perror("订阅响应接收失败");
        return -1;
    }
    
    // 4. 检查响应类型 (0x90 = SUBACK)
    if (response[0] != 0x90) {
        fprintf(stderr, "无效响应类型: 0x%02X\n", response[0]);
        return -1;
    }
    
    // 5. 检查报文ID是否匹配
    uint16_t received_id = (response[2] << 8) | response[3]; // 修正索引位置
    
    if (received_id != packet_id) {
        fprintf(stderr, "报文ID不匹配: 发送 %d, 收到 %d\n", packet_id, received_id);
        return -1;
    }
    
    // 6. 检查返回码 (0-2表示成功, 0x80表示失败)
    if (response[4] >= 0x80) {
        fprintf(stderr, "订阅失败: 错误码 0x%02X\n", response[4]);
        return -1;
    }
    
    printf("订阅成功! (QoS: %d)\n", response[4]);
    return 0;
}
// 处理PUBLISH消息并发送PUBACK
void handle_publish_message(int sockfd, unsigned char* buffer, int length) 
{
    int pos = 1; // 跳过固定头
    
    // 解析剩余长度 (变长编码)
    int multiplier = 1;
    int rem_len = 0;
    unsigned char digit;
    do {
        digit = buffer[pos++];
        rem_len += (digit & 127) * multiplier;
        multiplier *= 128;
    } while ((digit & 128) != 0);
    
    // 解析主题长度
    uint16_t topic_len = (buffer[pos] << 8) | buffer[pos+1];
    pos += 2;
    
    // 提取主题
    char topic[topic_len + 1];
    memcpy(topic, buffer + pos, topic_len);
    topic[topic_len] = '\0';
    pos += topic_len;
    
    // 提取QoS级别
    int qos = (buffer[0] & 0x06) >> 1;
    uint16_t packet_id = 0;
    
    // 如果是QoS1，获取报文ID
    if (qos > 0) {
        packet_id = (buffer[pos] << 8) | buffer[pos+1];
        pos += 2;
    }
    
    // 提取消息内容
    int payload_len = rem_len - (pos - 1); // 减去已经解析的长度
    char payload[payload_len + 1];
    memcpy(payload, buffer + pos, payload_len);
    payload[payload_len] = '\0';
    
    printf("\n==== 收到下发消息 ====\n");
    printf("主题: %s\n", topic);
    printf("QoS: %d\n", qos);
    if (qos > 0) printf("报文ID: %d\n", packet_id);
    printf("内容: %s\n", payload);
    printf("======================\n");
    
    // 如果是QoS1消息，发送PUBACK确认
    if (qos == 1) {
        unsigned char puback[4] = {
            0x40, // PUBACK类型 (0x40)
            0x02, // 剩余长度
            (unsigned char)(packet_id >> 8), // 报文ID高字节
            (unsigned char)(packet_id & 0xFF) // 报文ID低字节
        };
        
        if (send(sockfd, puback, sizeof(puback), 0) != sizeof(puback)) {
            perror("PUBACK发送失败");
        } else {
            printf("已发送PUBACK确认 (ID: %d)\n", packet_id);
        }
    }
}

void* mqtt_listen(void *arg)
{
    int sockfd = *((int*)arg);
    unsigned char buffer[1024];
    printf("启动MQTT消息监听线程...\n");
    while (1) {
        // 1. 读取固定头
        ssize_t bytes_read = recv(sockfd, buffer, 1, 0);
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                printf("服务器关闭了连接\n");
            } else {
                perror("接收错误");
            }
            close(sockfd);
            exit(1);
        }
        // 2. 解析报文类型
        unsigned char packet_type = buffer[0] & 0xF0;
        // 3. 读取剩余长度 (变长编码)
        int multiplier = 1;
        int rem_len = 0;
        unsigned char digit;
        int header_size = 1; // 已读1字节
        
        do {
            bytes_read = recv(sockfd, &digit, 1, 0);
            if (bytes_read <= 0) {
                perror("读取剩余长度失败");
                close(sockfd);
                exit(1);
            }
            header_size++;
            
            buffer[header_size - 1] = digit;
            rem_len += (digit & 127) * multiplier;
            multiplier *= 128;
        } while ((digit & 128) != 0);
        
        // 4. 读取剩余报文
        int total_len = header_size + rem_len;
        if (total_len > sizeof(buffer)) {
            fprintf(stderr, "消息过长 (%d > %zu)\n", total_len, sizeof(buffer));
            // 跳过超长消息
            while (rem_len > 0) {
                bytes_read = recv(sockfd, buffer, rem_len < sizeof(buffer) ? rem_len : sizeof(buffer), 0);
                if (bytes_read <= 0) break;
                rem_len -= bytes_read;
            }
            continue;
        }
        // 读取剩余数据
        bytes_read = recv(sockfd, buffer + header_size, rem_len, 0);
        if (bytes_read != rem_len) {
            perror("读取消息体失败");
            continue;
        }
        // 5. 根据报文类型处理
        switch (packet_type) {
            case 0x30: // PUBLISH 消息
                handle_publish_message(sockfd, buffer, total_len);
                break;
            case 0x90: // SUBACK 消息 (已在同步订阅中处理)
                printf("<== 收到SUBACK确认\n");
                break;
            case 0xD0: // PINGRESP 心跳响应
                printf("<== 收到心跳响应\n");
                break;
            case 0x40: // PUBACK (发布确认)
                printf("<== 收到PUBACK确认\n");
                break;
            default:
                printf("<== 收到未知消息类型: 0x%02X\n", packet_type);
                break;
        }
    }
    return NULL;
}
int main() 
{
    // 1. 建立TCP连接
    int sockfd = create_socket();
    if(sockfd < 0) 
    {
        printf("创建套接字，建立链接失败\n");
        return 1;
    }    
    // 2. 构造MQTT CONNECT报文
    int packet_len;
    unsigned char* packet = build_connect_packet(&packet_len); 
    // 3. 发送报文
    if(send(sockfd, packet, packet_len, 0) != packet_len) 
    {
        perror("发生失败");
        free(packet);
        close(sockfd);
        return 1;
    }
    free(packet);   
    // 4. 接收CONNACK响应
    unsigned char response[4];
    ssize_t len = recv(sockfd, response, sizeof(response), 0);
    printf("%x %x %x %x\n", response[0], response[1], response[2], response[3]);
    if(len < 4 || response[0] != 0x20 || response[3] != 0x00) {
        printf("连接失败! Response code: 0x%02x\n", response[3]);
        close(sockfd);
        return 1;
    }
    printf("连接物联网平台成功!\n");
    
    // 订阅主题并检查结果
    if (mqtt_subscribe(sockfd, MQTT_SUBSCRIBE_TOPIC) != 0)
    {
        printf("订阅主题失败\n");
        return -1;
    }
    printf("订阅主题成功\n");

    pthread_t listen_thread;
    pthread_create(&listen_thread, NULL, (void*)mqtt_listen, (void*)&sockfd);
    while (1)
    {
        // 5、上报数据
        publish_sensor_data(sockfd);
        send_heartbeat(sockfd);
        sleep(1);
    }
    
    // 7. 关闭连接
    // close(sockfd);

    return 0;
}