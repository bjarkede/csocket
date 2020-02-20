#include "shared.hpp"

static pthread_t threads[NUM_THREADS];
static unsigned int logicalThreadIndex;

// TODO
#if defined (_WIN32)
DWORD WINAPI AcceptSocket(void* data) {

  return 0;
}
#else
void* AcceptSocket(void* data) {

  sockDownload_t* dl = (sockDownload_t*)data;
  
  const int ec = recv(dl->socket, dl->recBuffer, sizeof(dl->recBuffer) - 1, 0);
  
  pthread_exit(NULL);
}
#endif

File::File() : file_buffer(NULL) {};

File::~File() {
  if (file_buffer != NULL)
    fclose((FILE*)file_buffer);
}

bool File::Open(const char* filePath, const char* mode) {
  file_buffer = fopen(filePath, mode);
  return file_buffer != NULL;
}

bool File::Close() {
  return fclose((FILE*)file_buffer);
}

bool File::IsValid() {
  return file_buffer != NULL;
}

bool File::Read(void* data, size_t bytes) {
  return fread(data, bytes, 1, (FILE*)file_buffer) == 1;
}

bool File::Write(const void* data, size_t bytes) {
  return fwrite(data, sizeof(char), bytes, (FILE*)file_buffer) == 1;
}

Socket::Socket() {}

Socket::~Socket() {
}

bool Socket::SetSocketBlocking(SOCKET socket, bool blocking) {
#if defined(_WIN32)
  u_long option = blocking ? 0 : 1;
  return ioctlsocket(socket, FIONBIO, &option) != SOCKET_ERROR;
#else
  const int flags = fcntl(socket, F_GETFL, 0);
  if(flags == -1)
    return false;
  const int newFlags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
  return fcntl(socket, F_SETFL, newFlags) != SOCKET_ERROR;
#endif
}

bool Socket::SetSocketOption(fileDownload_t* dl, int option, const void* data, size_t dataLength) {
#if defined(_WIN32)
  if(setsockopt(dl->socket, SOL_SOCKET, option, (const char*)data, (int)dataLength) == SOCKET_ERROR) {
#else
  if(setsockopt(dl->socket, SOL_SOCKET, option, data, (socklen_t)dataLength) == SOCKET_ERROR) {
#endif
    // print socket error
    return false;
    
  }

  return true;
}

bool Socket::Create(fileDownload_t* dl) {
  if(dl->socket != INVALID_SOCKET) {
    closesocket(dl->socket);
    dl->socket = INVALID_SOCKET;
  }

  dl->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP); // Create the socket.
  if(dl->socket == INVALID_SOCKET) {
    // print socket error
    return false;
  }

  if(!SetSocketBlocking(dl->socket, false)) {
    // print socket error
    return false;
  }

#if defined(_WIN32)
  DWORD timeout   = 1;
#else
  timeval timeout;
  timeout.tv_sec  = 0;
  timeout.tv_usec = 1000;
#endif

  if(!SetSocketOption(dl, SO_RCVTIMEO, &timeout, sizeof(timeout)))
    return false;
  
#if defined(_WIN32)
  timeout = MAX_TIMEOUT_MS;
#else
  timeout.tv_sec  = MAX_TIMEOUT_MS / 1000;
  timeout.tv_usec = MAX_TIMEOUT_MS * 1000 - timeout.tv_sec * 1000000;
#endif
  if(!SetSocketOption(dl, SO_SNDTIMEO, &timeout, sizeof(timeout)))
    return false;

  return true;
}

void DownloadClear(sockDownload_t* dl) {
  dl->socket = INVALID_SOCKET;
  dl->file   = NULL;
  dl->bytesTotal = 0;
  dl->bytesDownloaded = 0;
}

//TODO
bool Socket::AcceptNextConnection(fileDownload_t* dl) {

  SOCKET connSocket = INVALID_SOCKET;
  connSocket = accept(dl->socket, (struct sockaddr*)&connSocket, (socklen_t*)sizeof(connSocket));
  if(connSocket != SOCKET_ERROR) {
#if defined (_WIN32)

#else
    sockDownload_t dl; 
    
    if(pthread_create(&threads[logicalThreadIndex++], NULL, AcceptSocket, &connSocket)) {
      // Error couldn't create thread.
      return false;
    }

#endif
  } else {
    // Error accepting connection.
    return false;
  }
  
  return true;
}

bool Socket::ListenBegin(fileDownload_t* dl) {
  
  if(!Create(dl))
    return false;

  addrInfo_t hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_PASSIVE;

  addrInfo_t* address;
  const int ec = getaddrinfo(NULL, DEFAULT_PORT, &hints, &address);
  if(ec == 0) {
    if(bind(dl->socket, address->ai_addr, (int)address->ai_addrlen) == SOCKET_ERROR) {
      // Binding failed
      printf("Binding failed with error: %d...\n", GetSocketError());
      freeaddrinfo(address);
      return false;
    }

    freeaddrinfo(address);
  } else {
    // Couldn't get addr info
    return false;
  }

  if(listen(dl->socket, SOMAXCONN) == SOCKET_ERROR) {
    return false;
  }

  return true;
}
 
bool Socket::DownloadBegin(fileDownload_t* dl, fileDownloadSource_t& source) {
  
  addrInfo_t hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family   = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  addrInfo_t* addresses;
  char service[256];
  snprintf(service, sizeof(service), "%d", source.port);
  
  const int ec = getaddrinfo(source.hostName, service, &hints, &addresses);
  if(ec == 0) {
    addrInfo_t* a = addresses;

    while(a != NULL && dl->addressCount < ARRAY_LEN(dl->addresses)) {
      sockaddr_in& address = dl->addresses[dl->addressCount++];
      memset(&address, 0, sizeof(sockaddr_in));
      memcpy(&address, a->ai_addr, sizeof(*a->ai_addr));
      a = a->ai_next;
    }

    freeaddrinfo(addresses);
  } else {
    // getaddrinfo error
  }

  if(dl->addressCount < ARRAY_LEN(dl->addresses)) {
    sockaddr_in& address = dl->addresses[dl->addressCount++];
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(source.port);
    inet_pton(AF_INET, source.numericHostName, &address.sin_addr.s_addr);
  }

  for(;;) {
    const connState_t result = OpenNextConnection(dl, source);
    if(result == CS_OPENING) {
      dl->connecting = true;
      return true;
    }
    if(result == CS_OPEN) {
      dl->timeOutStartTimeMS = INT_MIN;
      return true;
    }
    if(result == CS_LIST_EXHAUSTED)
      return false;
  }
}

int Socket::GetSocketError()
{
#if defined(_WIN32)
	return WSAGetLastError();
#else
	return errno;
#endif
}

bool Socket::IsConnectionInProgressError() {
  const int ec = GetSocketError();
#if defined (_WIN32)
  return ec == WSAEWOULDBLOCK;
#else
  return ec == EINPROGRESS;
#endif
}
 
connState_t Socket::OpenNextConnection(fileDownload_t* dl, fileDownloadSource_t& source) {
  if(dl->addressIndex >= dl->addressCount) {
    return CS_LIST_EXHAUSTED;
  }

  if(dl->addressIndex >= 1) {
    printf("Attempting to download again at address #%d...\n", dl->addressIndex + 1);

    if(!Create(dl))
      return CS_CLOSED;
  }

  const sockaddr_in& address = dl->addresses[dl->addressIndex];
  if(connect(dl->socket, (const sockaddr*)&address, sizeof(sockaddr_in)) != SOCKET_ERROR) {
    const connState_t result = SendFileRequest(dl, source) ? CS_OPEN : CS_CLOSED;
    ++dl->addressIndex;
    return result;
  }

  ++dl->addressIndex;
  if(!IsConnectionInProgressError()) {
    // Print error connection
    return CS_CLOSED;
  }
  
#if defined(_WIN32)
  SYSTEMTIME st;
  GetSystemTime(&st);
  dl->timeOutStartTimeMS = st.wMilliseconds;
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  dl->timeOutStartTimeMS = tv.tv_sec*1000LL + tv.tv_usec/1000;
#endif
  
  return CS_OPENING;
}

bool Socket::SendFileRequest(fileDownload_t* dl, fileDownloadSource_t& source) {
  const int port = source.port;
  const char* const hostName = dl->addressIndex >= dl->addressCount ? source.numericHostName : source.hostName;
  const char* const query = dl->query;

  char request[256];
  snprintf(request, sizeof(request),"xxx"); //@TODO: Format request
  const int requestLength = strlen(request);
  if(send(dl->socket, request, requestLength, 0) != requestLength) {
    // Print socket error, failed to send
    return false;
  }

  char tempPath[MAX_PATH];

#if defined(_WIN32)
  if(GetModuleFileNameA(NULL, tempPath, sizeof(tempPath) == 0) {
      // Error couldn't get current path (WIN32)
      return false;
  }
  
  if(GetTempFileNameA(tempPath, "", 0, dl->tempPath) == 0) {
    // Print error creating a file name
    return false;
  }
  dl->file = fopen(dl->tempPath, "wb");
#else
  if(getcwd(tempPath, sizeof(tempPath)) == NULL) {
    // Error couldn't get current path
    return false;
  }

  // Build the current path to the file download.
  snprintf(dl->tempPath, sizeof(dl->tempPath), "%s/XXXXXX.tmp", tempPath);
  const int fd = mkstemps(dl->tempPath, 4);
  dl->file = fd != -1 ? fdopen(fd, "wb") : NULL;
#endif
  if(dl->file == NULL) {
    // Error opening file
    const int error = errno;
    char* const errorString = strerror(error);

    return false;
  }

  // Get the start time in milliseconds.
#if defined(_WIN32)
  SYSTEMTIME st;
  GetSystemTime(&st);
  dl->startTimeMS = st.wMilliseconds;
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  dl->startTimeMS = tv.tv_sec*1000LL + tv.tv_usec/1000;
#endif
  
  return true;
}

bool ShouldPrintHelp(int argc, char** argv) {
  if (argc == 1)
    return true;

  return strcmp(argv[1], "/?") == 0 ||
         strcmp(argv[1], "/help") == 0 ||
         strcmp(argv[1], "--help") == 0;
}

bool AllocBuffer(Buffer& buffer, uptr bytes)
{
	buffer.buffer = malloc(bytes);
	if(!buffer.buffer)
	{
		return false;
	}

	buffer.length = bytes;
	return true;
}

bool DeallocBuffer(Buffer& buffer) {
  free(buffer.buffer);
  buffer.length = 0;

  if(!buffer.buffer) return true;
  return false;
}

bool ReadEntireFile(Buffer& buffer, const char* filePath) {
	FILE* file = fopen(filePath, "rb");
	if(!file)
	{
		return false;
	}

	fseek(file, 0, SEEK_END);
	const uptr bufferLength = (uptr)ftell(file);
	if(!AllocBuffer(buffer, bufferLength + 1))
	{
		fclose(file);
		return false;
	}

	fseek(file, 0, SEEK_SET);
	if(fread(buffer.buffer, (size_t)bufferLength, 1, file) != 1)
	{
		fclose(file);
		return false;
	}

	fclose(file);
	((char*)buffer.buffer)[bufferLength] = '\0';
	return true;
}

const char* GetExecutableFileName(char* argv0) {
  static char fileName[256];

  char* s = argv0 + strlen(argv0);
  char* end = s;
  char* start = s;
  bool endFound = false;
  bool startFound = false;
  
  while(s >= argv0) {
    const char c = *s;
    if(!startFound && !endFound && c == '.') {
      end = s;
      endFound = true;
    }

    if(!startFound && (c == '\\' || c == '/')) {
      start = s + 1;
      break;
    }

    --s;
  }

  strncpy(fileName, start, (size_t)(end - start));
  fileName[sizeof(fileName) - 1] = '\0';

  return fileName;
}
