#ifndef __rc_utils__
#define __rc_utils__

#include <string>
#include <stdio.h>
#include <sys/socket.h>
#include <vector>

#define RANM_MAX_LEN    10

namespace milive {

using namespace std;


class RCUtils {
  public:
    static int get_addr_byfd( int sock, int& ip, unsigned short& port );
    static long long hostonet( const unsigned long long val );
    static int ip_s2u( const string& sip, unsigned& uip );
    static string ip_u2s( unsigned uip );
//      static void getLocalIp(string& localIp);
    static string numToString( unsigned num );
    static unsigned stringToNum( const string& str );
    static void mi_sleep( int secs, int usecs );
    static bool startWith( const string& str, const string& strStart );
    static int ppowerof2( int i );

    static string get_in_addrstr(sockaddr *sa);
    static int get_in_addrstr(sockaddr *sa, string& addr);
    static unsigned short get_in_addrport(sockaddr *sa);
    
    static std::string DnsReslove(const string& domain);

    static unsigned int getCurrentTime();
    static unsigned GetCurrentTimeSecond();
    static int get_peeraddr_byfd( int sock, int& ip, unsigned short& port );
    static bool isHost(const string& host);
    static void rand_char( int rand_len, string& rand_str ) ;
    static void split(const std::string &s, char delim, std::vector<std::string> &elems);
    static int getFileSize(const std::string path);
    static string getCurrentTimeStr();
    static int createDocDir(std::string dir);
    static bool endWith( const string& str, const string& strEnd);
    
    static int removeFile(std::string fileName);
    static void strToLower(std::string& s);
};
}

#endif
