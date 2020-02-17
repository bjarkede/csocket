#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

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
  s32 sockfd, portno, n;
  struct sockaddr_in serv_addr;
  struct hostent* server;
  
  Socket();
  ~Socket();

  bool Open(const s32 port);
  bool Close();
  bool Connect(const char* hostname);
  bool Write(const void* data, size_t bytes);
  bool Read(void* data, size_t bytes);
};

bool AllocBuffer(Buffer& buffer, uptr bytes);
bool DeallocBuffer(Buffer& buffer);
bool ReadEntireFile(Buffer& buffer, const char* filePath);
bool ShouldPrintHelp(int argc, char** argv);
