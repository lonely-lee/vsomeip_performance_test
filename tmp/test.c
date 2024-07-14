#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/sockios.h>

int main() {
    int sockfd;
    struct ifreq ifr;

    // 创建套接字
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(1);
    }

    // 绑定到指定的网络接口
    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ);
    if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
        perror("ioctl");
        close(sockfd);
        exit(1);
    }

    // 检查NIC是否支持传输时间戳
    if (ifr.ifr_data & HWTSTAMP_FILTER_PTP_V2_L4_EVENT) {
        printf("NIC supports transmit timestamp.\n");
    } else {
        printf("NIC does not support transmit timestamp.\n");
    }

    close(sockfd);
    return 0;
}