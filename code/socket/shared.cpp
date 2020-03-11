#include "shared.hpp"

static pthread_t threads[NUM_THREADS];
static unsigned int logicalThreadIndex;

static void FormatQueryLocal(char* query, int querySize, const char* fileName) {
  printf(query, querySize, "file?n=%s", fileName);
}

static const fileDownloadSource_t fileDLSources[1] = {
 {"Local", "localhost", "0.0.0.0", 27015, &FormatQueryLocal}
};

// TODO
#if defined (_WIN32)
DWORD WINAPI AcceptSocket(void* data) {
  sockDownload_t* dl = (sockDownload_t*)data;

  const int ec = recv(dl->socket, dl->recBuffer, sizeof(dl->recBuffer) - 1, 0);
  
  free(data);
  return 0;
}
#else
//@TODO:
// We need to make the server/client follow our own custom
// protocol. -bjarke, 23rd February 2020
void* AcceptSocket(void* data) {

  fileDownload_t dl;
  dl.socket = *((SOCKET*)data);

  Buffer buf;

  while(1) {
    const int ec = recv(dl.socket, dl.recBuffer, sizeof(dl.recBuffer), 0);
    if(ec > 0) {

      char* token;
      token = strtok(dl.recBuffer, " ");
        
      if(strncmp(token, "QUIT", strlen(token) - 1) == 0) {
	break;
      }

      if(strncmp(token, "RETR", strlen(token) - 1) == 0) {
	token = strtok(NULL, ""); // Get the next token
	token[strcspn(token, "\n")] = 0;
	
	memcpy(dl.fileName, token, strlen(token));
	if(FindLocalFile(&dl)) {
	  if(ReadEntireFile(buf, dl.tempPath)) {

	    //char fileSize[MAX_PATH];
	    //snprintf(fileSize, MAX_PATH, "%d", (int)buf.length);
	    //write(dl.socket, fileSize, MAX_PATH);

	    //@TODO: Error when doing it in chunks right now
	    //for(int i = 0; i < buf.length; i += CHUNK_SIZE) {
	    //write(dl.socket, buf.buffer, buf.length);	
	    //}	    
	    for(int i = 0; i < buf.length; i += CHUNK_SIZE) {
	      if((i + CHUNK_SIZE) > buf.length) {
		// If we dont do this we write out of memory
		write(dl.socket, buf.buffer + i, buf.length - i);
	      } else {
		write(dl.socket, buf.buffer + i, CHUNK_SIZE);
	      }
	    }   
	  }
	}

	memset(&dl.recBuffer, 0, sizeof(dl.recBuffer));
	memset(&dl.tempPath, 0, sizeof(dl.tempPath));
	memset(&dl.fileName, 0, sizeof(dl.fileName));
	memset(buf.buffer, 0, sizeof(buf.buffer));
	buf.length = 0;
      }

      if(strncmp(token, "PWD", strlen(token) - 1) == 0) {
	char cwd[MAX_PATH];
	if (getcwd(cwd, sizeof(cwd)) != NULL) {
	  write(dl.socket, &cwd, strlen(cwd));
	} else {
	  write(dl.socket, "failed getting current directory", 33);
	}
      }

      //@TODO:
      //Special cases for LIST.
      if(strncmp(token, "LIST", strlen(token) - 1) == 0) {

	DIR* dir;
	struct dirent* ent;
	char list[MAX_PATH];
	strcpy(list, "List of files: \n\n");

	if((dir = opendir("./")) != NULL) {
	  while((ent = readdir(dir)) != NULL) {	     
	    if(ent->d_type == DT_REG) {
	      strncat(list, strcat(ent->d_name, "\n"), ent->d_reclen + 1);
	    }
	  }
	} else {
	  PrintError(&dl, "LIST (%d)", errno);
	}

	write(dl.socket, list, sizeof(list));
	closedir(dir);
      }
      
    } else {
      //PrintError(dl, "recv", ec);
    }
  }

  closesocket(dl.socket);
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

Socket::Socket(fileDownload_t dl_) : dl(dl_) {}

Socket::~Socket() {
}

bool Socket::SetSocketBlocking(SOCKET socket, bool blocking) {
#if defined(_WIN32)
	unsigned long option = blocking ? 0 : 1;
	return ioctlsocket(socket, FIONBIO, &option) != SOCKET_ERROR;
#else
	int flags = fcntl(socket, F_GETFL, 0);
	if (flags == -1)
		return false;
	int newFlags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
	return fcntl(socket, F_SETFL, newFlags) != SOCKET_ERROR;
#endif
}

bool Socket::SetSocketOption(int option, const void* data, size_t dataLength) {
#if defined(_WIN32)
  if(setsockopt(dl.socket, SOL_SOCKET, option, (const char*)data, (int)dataLength) == SOCKET_ERROR) {
#else
  if(setsockopt(dl.socket, SOL_SOCKET, option, data, (socklen_t)dataLength) == SOCKET_ERROR) {
#endif
    PrintSocketError("setsockoption");
    return false;
    
  }

  return true;
}

bool Socket::Create() {
  if(dl.socket != INVALID_SOCKET) {
    closesocket(dl.socket);
    dl.socket = INVALID_SOCKET;
  }

  dl.socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP); // Create the socket.
  if(dl.socket == INVALID_SOCKET) {
    PrintSocketError("createsocket");
    return false;
  }

  if(!SetSocketBlocking(dl.socket, false)) {
    PrintSocketError("ioctlsocket/fnctl");
    return false;
  }

#if defined(_WIN32)
  DWORD timeout   = 1;
#else
  timeval timeout;
  timeout.tv_sec  = 0;
  timeout.tv_usec = 1000;
#endif

  if(!SetSocketOption(SO_RCVTIMEO, &timeout, sizeof(timeout)))
    return false;
  
#if defined(_WIN32)
  timeout = MAX_TIMEOUT_MS;
#else
  timeout.tv_sec  = MAX_TIMEOUT_MS / 1000;
  timeout.tv_usec = MAX_TIMEOUT_MS * 1000 - timeout.tv_sec * 1000000;
#endif
  if(!SetSocketOption(SO_SNDTIMEO, &timeout, sizeof(timeout)))
    return false;

  return true;
}

void DownloadClear(fileDownload_t* dl) {
  *dl->tempPath = '\0';
  *dl->fileName = '\0';
  *dl->errorMessage = '\0';
  //dl->socket = INVALID_SOCKET;
  dl->file = NULL;
  dl->addressIndex = 0;
  dl->addressCount = 0;
  //dl->startTimeMS = INT_MIN;
  //dl->crc32 = 0;
  //dl->bytesHeader = 0;
  dl->bytesTotal = 0;
  dl->bytesDownloaded = 0;
  //dl->headerParsed = false;
  //dl->badResponse = false;
  //dl->timeOutStartTimeMS = INT_MIN;
  //dl->lastErrorTimeOut = false;
  //dl->sourceIndex = 0;
  dl->exactMatch = false;
  dl->cleared = true;
  dl->connecting = false;
}

//TODO
bool Socket::AcceptNextConnection() {

  SOCKET connSocket = INVALID_SOCKET;
  fileDownload_t clientdl;
  socklen_t clilen = sizeof(dl.client_addr);
  connSocket = accept(dl.socket, (sockaddr*)&dl.client_addr, &clilen);
  if(connSocket != INVALID_SOCKET) {
#if defined (_WIN32)
    HANDLE thread = CreateThread(NULL, 0, AcceptSocket, &clientdl, 0, NULL);

#else
    SOCKET l = connSocket;
    if(pthread_create(&threads[logicalThreadIndex], NULL, AcceptSocket, &l)) {
      // Error couldn't create thread.
      return false;
    }

    if(pthread_detach(threads[logicalThreadIndex++])) {
      // Error couldn't join thread.
      return false;
    }
#endif
  } else {
    return false;
  }
  
  return true;
}

bool Socket::ListenBegin() {
  
  addrInfo_t hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_PASSIVE;

  addrInfo_t* addresses;
  
  const int ec = getaddrinfo(NULL, DEFAULT_PORT, &hints, &addresses);
  if(ec == 0) {
    
    addrInfo_t* a = addresses;

    while(a != NULL) {
      if(a->ai_family == AF_INET &&
	 a->ai_socktype == SOCK_STREAM &&
	 a->ai_protocol == IPPROTO_TCP) {
	
	sockaddr_in&  address = dl.serv_addr;
	memset(&address, 0, sizeof(sockaddr_in));
	dl.serv_addr.sin_family = AF_INET;
	dl.serv_addr.sin_addr.s_addr = INADDR_ANY;
	dl.serv_addr.sin_port = htons(27015);
	//memcpy(&address, a->ai_addr, sizeof(*a->ai_addr));
	
	break;
      }
      a = a->ai_next;
    }
    
    if(bind(dl.socket, (struct sockaddr *)&dl.serv_addr, sizeof(dl.serv_addr)) == SOCKET_ERROR) {
      // Binding failed
      PrintSocketError("listenbegin");
      return false;
    }
   
    freeaddrinfo(addresses);
  } else {
    // Couldn't get addr info
    return false;
  }

  if(listen(dl.socket, SOMAXCONN) == SOCKET_ERROR) {
    return false;
  }

  return true;
}
 
bool Socket::DownloadBegin(int sourceIndex) {

  DownloadClear(&dl);
  dl.sourceIndex = sourceIndex;

  if(!Create())
    return false;
  
  addrInfo_t hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family   = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  const fileDownloadSource_t& source = fileDLSources[dl.sourceIndex];
  addrInfo_t* addresses;
  char service[256];
  snprintf(service, sizeof(service), "%d", source.port);
  
  const int ec = getaddrinfo(source.hostName, service, &hints, &addresses);
  if(ec == 0) {
    addrInfo_t* a = addresses;

    while(a != NULL && dl.addressCount < ARRAY_LEN(dl.addresses)) {
      sockaddr_in& address = dl.addresses[dl.addressCount++];
      memset(&address, 0, sizeof(sockaddr_in));
      memcpy(&address, a->ai_addr, sizeof(*a->ai_addr));
      a = a->ai_next;
    }

    freeaddrinfo(addresses);
  } else {
    PrintSocketError("getaddrinfo", ec);
  }

  if(dl.addressCount < ARRAY_LEN(dl.addresses)) {
    sockaddr_in& address = dl.addresses[dl.addressCount++];
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(source.port);
    inet_pton(AF_INET, source.numericHostName, &address.sin_addr.s_addr);
  }

  for(;;) {
    const connState_t result = OpenNextConnection();
    if(result == CS_OPENING) {
      dl.connecting = true;
      return true;
    }
    if(result == CS_OPEN) {
      dl.timeOutStartTimeMS = INT_MIN;
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

//@TODO
connState_t Socket::FinishCurrentConnection() {
  connState_t temp;
  return temp;
}
 
connState_t Socket::OpenNextConnection() {
  if(dl.addressIndex >= dl.addressCount) {
    return CS_LIST_EXHAUSTED;
  }

  if(dl.addressIndex >= 1) {
    printf("Attempting to download again at address #%d...\n", dl.addressIndex + 1);

    if(!Create())
      return CS_CLOSED;
  }

  const sockaddr_in& address = dl.addresses[dl.addressIndex];
  if(connect(dl.socket, (const sockaddr*)&address, sizeof(sockaddr_in)) != SOCKET_ERROR) {
    const connState_t result = SendFileRequest() ? CS_OPEN : CS_CLOSED;
    ++dl.addressIndex;
    return result;
  }

  ++dl.addressIndex;
  if(!IsConnectionInProgressError()) {
    PrintSocketError("openconnection");
    return CS_CLOSED;
  }
  
#if defined(_WIN32)
  SYSTEMTIME st;
  GetSystemTime(&st);
  dl.timeOutStartTimeMS = st.wMilliseconds;
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  dl.timeOutStartTimeMS = tv.tv_sec*1000LL + tv.tv_usec/1000;
#endif
  
  return CS_OPENING;
}

bool FindLocalFile(fileDownload_t* dl) {
  DIR* dir;
  struct dirent* ent;

  if((dir = opendir ("./")) != NULL) { // current working directory
    while((ent = readdir(dir)) != NULL) {
      if(strcmp(ent->d_name, dl->fileName) == 0) {
#if defined (_WIN32)
	
#else	
	if(realpath(ent->d_name, dl->tempPath) != NULL) {
	  printf("%s\n", dl->tempPath);
	} else {
	  PrintError(dl, "findlocalfile (%s)", dl->tempPath);
	  return false;
	}
#endif
      }
    }
    
  } else {
    PrintError(dl, "findlocalfile (%s)", dl->tempPath);
    return false;
  }

  
 
  return true;
}

bool Socket::SendFileRequest() {
  const fileDownloadSource_t& source = fileDLSources[dl.sourceIndex];
  const int port = source.port;
  const char* const hostName = dl.addressIndex >= dl.addressCount ? source.numericHostName : source.hostName;
  const char* const query = dl.query;

  char request[256];
  snprintf(request, sizeof(request),"xxx"); //@TODO: Format request
  const int requestLength = strlen(request);
  if(send(dl.socket, request, requestLength, 0) != requestLength) {
    PrintSocketError("sendfilereq");
    return false;
  }

  char tempPath[MAX_PATH];

#if defined(_WIN32)
  if(GetModuleFileNameA(NULL, tempPath, sizeof(tempPath)) == 0) {
      // Error couldn't get current path (WIN32)
      return false;
  }
  
  if(GetTempFileNameA(tempPath, "", 0, dl.tempPath) == 0) {
    // Print error creating a file name
    return false;
  }
  dl.file = fopen(dl.tempPath, "wb");
#else
  if(getcwd(tempPath, sizeof(tempPath)) == NULL) {
    // Error couldn't get current path
    return false;
  }

  // Build the current path to the file download.
  snprintf(dl.tempPath, sizeof(dl.tempPath), "%s/XXXXXX.tmp", tempPath);
  const int fd = mkstemps(dl.tempPath, 4);
  dl.file = fd != -1 ? fdopen(fd, "wb") : NULL;
#endif
  if(dl.file == NULL) {
    // Error opening file
    const int error = errno;
    char* const errorString = strerror(error);

    return false;
  }

  // Get the start time in milliseconds.
#if defined(_WIN32)
  SYSTEMTIME st;
  GetSystemTime(&st);
  dl.startTimeMS = st.wMilliseconds;
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  dl.startTimeMS = tv.tv_sec*1000LL + tv.tv_usec/1000;
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

void PrintError(fileDownload_t* dl, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(dl->tempMessage, sizeof(dl->tempMessage), fmt, ap);
  va_end(ap);

  if(dl->realFileName && dl->fileName[0] != '\0') {
    snprintf(dl->errorMessage, sizeof(dl->errorMessage), "File download failed: file '%s' - %s", dl->fileName, dl->tempMessage);
  } else {
    snprintf(dl->errorMessage, sizeof(dl->errorMessage), "File download failed: %s", dl->tempMessage);
  }
  
  const int l = strlen(dl->errorMessage);
  if(l > 0 && dl->errorMessage[l - 1] != '\n')
    strcat(dl->errorMessage, "\n");
  
  printf(dl->errorMessage);
}

void Socket::PrintSocketError(const char* functionName, int ec) {

  const char* const errorMsg = strerror_r(ec, dl.tempMessage2, sizeof(dl.tempMessage2));
  if(errorMsg == NULL) {
    PrintError(&dl, "%s failed with error %d", functionName, ec);
    return;
  }
  
  PrintError(&dl, "%s failed with error %d (%s)", functionName, ec, errorMsg);
}

void Socket::PrintSocketError(const char* functionName) {
  PrintSocketError(functionName, GetSocketError());
}

bool Socket::FileDownload_Active() {
  return dl.cleared && dl.socket != INVALID_SOCKET;
}

bool Socket::FileDownload_CheckActive() {
  if(!FileDownload_Active())
    return false;

  PrintError(&dl, "Download already in progress for file %s", dl.fileName);
  return true;
}

bool Socket::FileDownload_StartImpl(const char* fileName, int sourceIndex, bool exactMatch, bool realFileName) {
  const fileDownloadSource_t& source = fileDLSources[sourceIndex];

  printf("Attempting download from the %s file server...\n", source.name);
  strncpy(dl.fileName, fileName, sizeof(dl.fileName));
  dl.realFileName = realFileName;

  const bool success = DownloadBegin(sourceIndex);  
}

bool Socket::FileDownload_Start(const char* fileName) {
  if(FileDownload_CheckActive())
    return false;

  (*fileDLSources[0].formatQuery)(dl.query, sizeof(dl.query), fileName);
  if(FileDownload_StartImpl(fileName, 0, false, true))
    return true;

  return false; //@TODO!
}


bool Socket::FileDownload_CleanUp(bool rename) {
  if(!dl.cleared)
    return false;

  if(dl.socket != INVALID_SOCKET) {
    closesocket(dl.socket);
    dl.socket = INVALID_SOCKET;
  }

  if(dl.file != NULL) {
    fclose(dl.file);
    dl.file = NULL;
  }

  bool success = true;
  if(rename) {

    //TODO
  }

  if(unlink(dl.tempPath) != 0 && errno != ENOENT) {
    PrintError(&dl, "Failed to delete file '%s'", dl.tempPath);
  }

  return success;
}

int Socket::FileDownload_Continue() {
  if(dl.socket == INVALID_SOCKET)
    return FDLS_NOTHING;

  if(dl.connecting) {
    connState_t cs = FinishCurrentConnection();
    if(cs == CS_CLOSED) {
      for(;;) {
	cs = OpenNextConnection();
	if(cs == CS_OPENING)
	  return FDLS_IN_PROGRESS;
	if(cs == CS_OPEN) {
	  dl.connecting = false;
	  dl.timeOutStartTimeMS = INT_MIN;
	  dl.lastErrorTimeOut = false;
	  return FDLS_IN_PROGRESS;
	}
	if(cs == CS_LIST_EXHAUSTED) {
	  //@TODO: Cleanup the download.
	  return FDLS_ERROR;
	}
      }
    }
    if(cs == CS_OPENING)
      return FDLS_IN_PROGRESS;

    dl.connecting = false;
    dl.timeOutStartTimeMS = INT_MIN;
    dl.lastErrorTimeOut = false;
    return FDLS_IN_PROGRESS;
  }

  // Receive data.
  const int ec = recv(dl.socket, dl.recBuffer, sizeof(dl.recBuffer), 0);
  if(ec < 0) { // Some error happened

  }

  if(ec == 0) {
    if(dl.bytesDownloaded == dl.bytesTotal) // All data has been sent
      return FileDownload_CleanUp(true) ? FDLS_SUCCESS : FDLS_ERROR;

    if(dl.badResponse) {
      // Print Error: Some response error from the server.
    } else {
      // Print Error: Connection closed
    }

    FileDownload_CleanUp(false);
    return FDLS_ERROR;
  }

  dl.bytesDownloaded += ec;
  const u32 bytesToWrite = ec;
  if(bytesToWrite > 0) {
    if(fwrite(dl.recBuffer, bytesToWrite, 1, dl.file) != 1) {
      const int err = errno;
      char* const errorString = strerror(err);
      PrintError(&dl, "Failed to write %d bytes to '%s' : %s (%d)",
		 (int)ec, dl.tempPath, errorString ? errorString : "?", err);
      FileDownload_CleanUp(false);
      return FDLS_ERROR;
    }
  }

  if(dl.bytesDownloaded == dl.bytesTotal) {
    return FileDownload_CleanUp(true) ? FDLS_SUCCESS : FDLS_ERROR;
  }
    
  dl.timeOutStartTimeMS = INT_MIN;
  dl.lastErrorTimeOut = false;

  return FDLS_IN_PROGRESS;
}
