/***
 * @Date: 2022-5-28 10:45:50
 * @LastEditors  : fanqiangwei
 * @LastEditTime : 2022-05-28 14:49:31
 * @FilePath     : /app/dal/yd-dal-base/utils/pingcheck/YPingCheck.cpp
 */
#include "YPingCheck.h"
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>

#define LOG_TAG "YPingCheck"
#define PACKET_SIZE 4096

DAL_BASE_BEGIN_NAMESPACE
namespace Internal {

class YPingCheckPrivate
{
public:
    YPingCheckPrivate();
    ~YPingCheckPrivate();

    // ping
    int ping(char *ips, int timeout);

private:
    unsigned short cal_chksum(unsigned short *addr, int len);
};

} // namespace Internal

using namespace Internal;

YPingCheckPrivate::YPingCheckPrivate()
{
}

YPingCheckPrivate::~YPingCheckPrivate()
{
}

unsigned short YPingCheckPrivate::cal_chksum(unsigned short *addr, int len)
{
    int nleft = len;
    int sum = 0;
    unsigned short *w = addr;
    unsigned short answer = 0;

    while (nleft > 1) {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1) {
        *(unsigned char *)(&answer) = *(unsigned char *)w;
        sum += answer;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = ~sum;

    return answer;
}
// ping函数
int YPingCheckPrivate::ping(char *ips, int timeout)
{
    struct timeval *tval;
    int maxfds = 0;
    fd_set readfds;

    struct sockaddr_in addr;
    struct sockaddr_in from;
    // 设定Ip信息
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;

    addr.sin_addr.s_addr = inet_addr(ips);

#if 1
    int sockfd;
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        YLOGE(LOG_TAG) << "socket error ip : " << ips << std::endl;
        return PINGERROR;
    }

    struct timeval timeo;
    // 设定TimeOut时间
    timeo.tv_sec = timeout / 1000;
    timeo.tv_usec = timeout % 1000;

    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeo, sizeof(timeo)) == -1) {
        YLOGE(LOG_TAG) << "setsockopt error ip : " << ips << std::endl;
        return PINGERROR;
    }

    char sendpacket[PACKET_SIZE];
    char recvpacket[PACKET_SIZE];
    // 设定Ping包
    memset(sendpacket, 0, sizeof(sendpacket));

    pid_t pid;
    // 取得PID，作为Ping的Sequence ID
    pid = getpid();

    struct ip *iph;
    struct icmp *icmp;

    icmp = (struct icmp *)sendpacket;
    icmp->icmp_type = ICMP_ECHO; //回显请求
    icmp->icmp_code = 0;
    icmp->icmp_cksum = 0;
    icmp->icmp_seq = 0;
    icmp->icmp_id = pid;
    tval = (struct timeval *)icmp->icmp_data;
    gettimeofday(tval, NULL);
    icmp->icmp_cksum = cal_chksum((unsigned short *)icmp, sizeof(struct icmp)); //校验

    int n;
    // 发包 。可以把这个发包挪到循环里面去。
    n = sendto(sockfd, (char *)&sendpacket, sizeof(struct icmp), 0, (struct sockaddr *)&addr, sizeof(addr));
    if (n < 1) {
        YLOGE(LOG_TAG) << "sendto error ip : " << ips << std::endl;
        return PINGERROR;
    }

    // 接受
    // 由于可能接受到其他Ping的应答消息，所以这里要用循环
    while (1) {
        // 设定TimeOut时间，这次才是真正起作用的
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        maxfds = sockfd + 1;
        n = select(maxfds, &readfds, NULL, NULL, &timeo);
        if (n <= 0) {
            YLOGE(LOG_TAG) << "Time out error ip : " << ips << std::endl;
            close(sockfd);
            return PINGERROR;
        }

        // 接受
        memset(recvpacket, 0, sizeof(recvpacket));
        int fromlen = sizeof(from);
        n = recvfrom(sockfd, recvpacket, sizeof(recvpacket), 0, (struct sockaddr *)&from, (socklen_t *)&fromlen);
        YLOGD(LOG_TAG) << "recvfrom Len : " << n << std::endl;
        if (n < 1) {
            return PINGERROR;
        }

        char *from_ip = (char *)inet_ntoa(from.sin_addr);
        // 判断是否是自己Ping的回复
        if (strcmp(from_ip, ips) != 0) {
            YLOGE(LOG_TAG) << "NowPingip : " << ips
                           << ", Fromip : " << from_ip
                           << ", NowPingip is not same to Fromip,so ping wrong!"
                           << std::endl;
            return PINGERROR;
        }

        iph = (struct ip *)recvpacket;

        icmp = (struct icmp *)(recvpacket + (iph->ip_hl << 2));

        YLOGD(LOG_TAG) << "ip : " << ips
                       << ", icmp->icmp_type : " << icmp->icmp_type
                       << ", icmp->icmp_id: " << icmp->icmp_id << std::endl;
        // 判断Ping回复包的状态
        if (icmp->icmp_type == ICMP_ECHOREPLY && icmp->icmp_id == pid) //ICMP_ECHOREPLY回显应答
        {
            // 正常就退出循环
            YLOGD(LOG_TAG) << "icmp succecss ............." << std::endl;
            break;
        } else {
            // 否则继续等
            continue;
        }
    }
#endif
    return PINGSUCCESS;
}

YSINGLETON_CREATE_FUN(YPingCheck)

YPingCheck::YPingCheck()
{
    d = new YPingCheckPrivate();
}

YPingCheck::~YPingCheck()
{
    if (d) {
        delete d;
        d = NULL;
    }

    YLOGD(LOG_TAG) << "YPingCheck::~YPingCheck()" << std::endl;
}

int YPingCheck::ping(char *ips, int timeout)
{
    return d->ping(ips, timeout);
}

DAL_BASE_END_NAMESPACE