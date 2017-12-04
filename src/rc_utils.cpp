#include "rc_utils.h"
#include <arpa/inet.h>
//#include <ifaddrs.h>
#include <netinet/in.h>
#include <sstream>
#include <sys/select.h>
#include <netdb.h>
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>
#include <cstdlib>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <algorithm>

using namespace milive;

using namespace std;

namespace helper {
inline unsigned long long htonllex( unsigned long long data ) {
    union {
        uint64_t ull;
        uint32_t ul[2];
    } u;

    u.ull = 0x01;
    if ( u.ul[1] == 0x01 ) { //bigendine
        return data;
    } else {
        u.ul[0] = ( uint32_t ) ( data >> 32 );
        u.ul[1] = ( uint32_t ) ( data & 0xffffffff );
        u.ul[0] = htonl( u.ul[0] );
        u.ul[1] = htonl( u.ul[1] );
        return u.ull;
    }
}

inline unsigned long long ntohllex( unsigned long long src ) {
    return htonllex( src );
}
}

#define ACK_RANDLEN 52
void RCUtils::rand_char( int rand_len, string& rand_str ) {
    char celem[ACK_RANDLEN] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'};
    char szstr[32] = {0};
    if ( rand_len > 31 ) {
        rand_len = 31;
    }
    int irand = 0;
    for ( int i = 0; i < rand_len; ++i ) {
        irand = rand() % ACK_RANDLEN;
        szstr[i] = celem[irand];
    }
    rand_str = szstr;
}

long long RCUtils::hostonet( const unsigned long long val ) {
    return helper::htonllex( val );
}
//is i =  2 or 2*2....
int RCUtils::ppowerof2( int i ) {
    return ( ( i > 0 ) && ( ( i & ( i - 1 ) ) == 0 ) );
}

int RCUtils::get_peeraddr_byfd( int sock, int& ip, unsigned short& port ) {
    struct sockaddr_in sa;
    socklen_t len = sizeof( sa );
    if ( ::getpeername( sock, ( struct sockaddr* )&sa, &len ) == 0 ) {
        ip = sa.sin_addr.s_addr;
        port = ntohs( sa.sin_port );
        return 0;
    }else{
        return 1;
    }
    
}
int RCUtils::get_addr_byfd( int sock, int& ip, unsigned short& port ) {
    sockaddr_in addr;
    socklen_t addr_len = sizeof( addr );

    if ( ::getsockname( sock, ( sockaddr* ) &addr, &addr_len ) < 0 ) {
        return 1;
    } else {
        ip = addr.sin_addr.s_addr;
        port = ntohs( addr.sin_port );

        //printf("%s read ip : %s port=%d\n",__func__, ::inet_ntoa(addr.sin_addr), ntohs(port));
        return 0;
    }
}

int RCUtils::ip_s2u( const string& sip, unsigned& uip ) {
    sockaddr_in sa;

    int ret = inet_pton( AF_INET, sip.c_str(), &sa.sin_addr );
    if ( ret <= 0 ) {
        return -1;
    } else {
        uip = ( unsigned )sa.sin_addr.s_addr;
        return 0;
    }
}

string RCUtils::ip_u2s( unsigned ip ) {
    char ip_buf[20] = {0};

    struct in_addr addr;
    addr.s_addr = ip;

    return inet_ntop( AF_INET, &addr, ip_buf, sizeof( ip_buf ) );
}

string RCUtils::numToString( unsigned num ) {
    stringstream ss;
    ss << num;
    string s = ss.str();
    return s;
}

unsigned RCUtils::stringToNum( const string& str ) {
    stringstream ss;
    ss << str;
    unsigned i;
    ss >> i;
    return i;
}

std::string RCUtils::DnsReslove(const string& domain)
{
    struct hostent * result = gethostbyname(domain.c_str());
    if (result == NULL)
    {
        return "";
    }
    char str[32] = { 0 };
    char **pptr = result->h_addr_list;
    
    switch (result->h_addrtype)
    {
        case AF_INET:
        case AF_INET6:
            inet_ntop(result->h_addrtype, *pptr, str, sizeof(str));
            break;
        default:
            printf("unknown address type\n");
            break;
    }
    if (strlen(str) > 0)
    {
        return string(str);
    }
    else
    {
        return "";
    }
}
bool RCUtils::isHost(const string& host){
    return (inet_addr(host.c_str()) == INADDR_NONE);
}
void RCUtils::mi_sleep( int secs, int usecs ) {
    struct timeval to;
    to.tv_sec = secs;
    to.tv_usec = usecs;

    select( 0, NULL, NULL, NULL, &to );
}

bool RCUtils::startWith( const string& str, const string& strStart ) {
    if ( str.empty() || strStart.empty() ) {
        return false;
    }

    return str.compare( 0, strStart.size(), strStart ) == 0 ? true : false;
}

bool RCUtils::endWith( const string& str, const string& strEnd) {
    if ( str.empty() || strEnd.empty() ) {
        return false;
    }
    
    return str.compare( str.size() - strEnd.size(), strEnd.size(), strEnd ) == 0 ? true : false;
}

int RCUtils::get_in_addrstr(sockaddr *sa, string& addr)
{
    char buf[INET6_ADDRSTRLEN];
    if (sa->sa_family == AF_INET)
    {
        addr = inet_ntop(AF_INET, &(((sockaddr_in*)sa)->sin_addr), buf, INET_ADDRSTRLEN);
    }else{
        addr = inet_ntop(AF_INET6, &(((sockaddr_in6*)sa)->sin6_addr), buf, INET6_ADDRSTRLEN);
    }
    return 0;
}

string RCUtils::get_in_addrstr(sockaddr *sa)
{
    string addr;
    char buf[INET6_ADDRSTRLEN];
    if (sa->sa_family == AF_INET)
    {
        addr = inet_ntop(AF_INET, &(((sockaddr_in*)sa)->sin_addr), buf, INET_ADDRSTRLEN);
    }else{
        addr = inet_ntop(AF_INET6, &(((sockaddr_in6*)sa)->sin6_addr), buf, INET6_ADDRSTRLEN);
    }
    return addr;
}

unsigned short RCUtils::get_in_addrport(sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return ntohs(((sockaddr_in*)sa)->sin_port);
    }else{
        return ntohs(((sockaddr_in6*)sa)->sin6_port);
    }
}

string RCUtils::getCurrentTimeStr(){
    time_t now = time(NULL);
    struct tm tmm;
    localtime_r(&now, &tmm);
    
    char s[50];                                // 定义总日期时间char*变量。
    pthread_t tid = pthread_self();
    sprintf(s, "[%04d-%02d-%02d %02d:%02d:%02d][%-5d]", tmm.tm_year + 1900, tmm.tm_mon + 1, tmm.tm_mday, tmm.tm_hour, tmm.tm_min, tmm.tm_sec, tid);// 将年月日时分秒合并。    
    string str(s);                             // 定义string变量，并将总日期时间char*变量作为构造函数的参数传入。
    return str;                                // 返回转换日期时间后的string变量。
}

unsigned int RCUtils::getCurrentTime()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

unsigned milive::RCUtils::GetCurrentTimeSecond()
{
    return time(NULL);
}

void RCUtils::split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;

    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
}

int RCUtils::getFileSize(const std::string path){
    struct stat sb;
    if(stat(path.c_str(), &sb) != -1){
        return sb.st_size;
    }
    return 0;
}

int RCUtils::createDocDir(std::string dir){
    int flag = 1;
    if (access(dir.c_str(), 0) == -1)
    {
       cout<<dir<<" is not existing ; so now make it"<<endl;
        flag=mkdir(dir.c_str(), 0777);
        
       if (flag == 0)
       {
           cout<<"make successfully"<<endl;
       } else {
           cout<<"make errorly"<<endl;
       }
    }
    return flag;
}
int RCUtils::removeFile(std::string fileName){
    return ::remove(fileName.c_str());
}

void RCUtils::strToLower(std::string& s)
{
    transform(s.begin(), s.end(), s.begin(), ::tolower);
}


