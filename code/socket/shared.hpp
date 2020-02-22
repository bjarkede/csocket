#include <stdint.h>
#include <stdarg.h>
#include <limits.h>
#if defined(_WIN32)
#include <io.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#if defined(_WIN32)
#include <winsock2.h>
#include <Windows.h>
#include <ws2tcpip.h>
#include <Processthreadsapi.h>
#else
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#endif

#if defined(_WIN32)
typedef ADDRINFOA addrInfo_t;
#define closesocket closesocket
#define unlink _unlink
#else
typedef addrinfo addrInfo_t;
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define MAX_PATH (256)
#define closesocket close
#define unlink unlink
#endif

#define DEFAULT_PORT "27015"
#define MAX_TIMEOUT_MS 2000
#define NUM_THREADS 5
#define MAXPRINTMSG 512
#define CHUNK_SIZE 1000

#define s8 int8_t
#define s16 int16_t
#define s32 int32_t
#define s64 int64_t
#define sptr int64_t

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t
#define uptr uint64_t

#define ARRAY_LEN(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

static const char sockaddr_in_large_enough[sizeof(sockaddr_in) >= sizeof(sockaddr) ? 1 : -1] = { 0 };

struct fileDownload_t {
  SOCKET socket;
  FILE* file;
  char tempMessage[MAXPRINTMSG];  // for PrintError
  char tempMessage2[MAXPRINTMSG]; // for PrintSocketError
  char errorMessage[MAXPRINTMSG];
  char fileName[MAX_PATH];
  int addressIndex;
  int addressCount;
  int startTimeMS;
  int timeOutStartTimeMS;
  bool connecting;
  bool cleared;
  bool exactMatch;
  bool realFileName;
  char query[512];
  sockaddr_in addresses[16];
  char tempPath[MAX_PATH];
  sockaddr_in serv_addr;
  sockaddr_in client_addr;
};

struct sockDownload_t {
  SOCKET socket;
  FILE* file;
  char recBuffer[1 << 20];
  int bytesTotal;
  int bytesDownloaded;
};

enum fileDownloadStatus_t {
			   FDLS_NOTHING,
			   FDLS_ERROR,
			   FDLS_SUCCESS,
			   FDLS_IN_PROGRESS,
			   FDLS_COUNT
};

enum connState_t {
		  CS_OPEN,
		  CS_OPENING,
		  CS_CLOSED,
		  CS_LIST_EXHAUSTED
};

typedef void (*filedlQueryFormatter_t)(char* query, int querySize, const char* fileName);

struct fileDownloadSource_t {
  const char* name;
  const char* hostName;
  const char* numericHostName; // fallback
  int port;
  //filedlQueryFormatter_t formatQuery;
};

struct Buffer {
  void * buffer;
  uptr length;
};

struct File {
  File();
  ~File();

  bool Open(const char* filePath, const char* mode);
  bool Close();
  bool IsValid();
  bool Read(void* data, size_t bytes);
  bool Write(const void* data, size_t bytes);

  void * file_buffer;
};

struct Socket {
  Socket();
  ~Socket();

  bool Create(fileDownload_t* dl);
  bool ListenBegin(fileDownload_t* dl);
  bool SendFileRequest(fileDownload_t* dl, fileDownloadSource_t& source);
  bool DownloadBegin(fileDownload_t* dl, fileDownloadSource_t& source);
  bool AcceptNextConnection(fileDownload_t* dl);
  connState_t OpenNextConnection(fileDownload_t* dl, fileDownloadSource_t& source);

  int GetSocketError();
  bool IsSocketTimeoutError();
  bool IsConnectionInProgressError();
  bool SetSocketBlocking(SOCKET socket, bool blocking);
  bool SetSocketOption(fileDownload_t* dl, int option, const void* data, size_t dataLength);

  void PrintSocketError(fileDownload_t* dl, const char* functionName, int ec);
  void PrintSocketError(fileDownload_t* dl, const char* functionName);
};

bool AllocBuffer(Buffer& buffer, uptr bytes);
bool DeallocBuffer(Buffer& buffer);
bool ReadEntireFile(Buffer& buffer, const char* filePath);
bool ShouldPrintHelp(int argc, char** argv);
const char* GetExecutableFileName(char* argv0);
void DownloadClear(sockDownload_t* dl);

void PrintError(fileDownload_t* dl, const char* fmt, ...);

