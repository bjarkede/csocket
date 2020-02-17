#include "shared.hpp"

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
  bool result = fwrite(data, sizeof(char), bytes, (FILE*)file_buffer) == 1;
  //fflush((FILE*)file_buffer);
  return result;
}

Socket::Socket() {}

Socket::~Socket() {
  close(sockfd);
}

bool Socket::Open(const s32 port) {
  portno = port;
  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if(sockfd < 0) return false;

  return true;
}

bool Socket::Close() {
  close(sockfd);
}

bool Socket::Connect(const char* hostname) {
  server = gethostbyname(hostname);
  if(server == NULL) return false;

  bzero((char*)&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char*)server->h_addr, (char*)&serv_addr.sin_addr.s_addr, server->h_length);

  serv_addr.sin_port = htons(portno);

  if(connect(sockfd,(struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) return false;

  return true;
}

bool Socket::Write(const void* data, size_t bytes) {  
  if(write(sockfd, data, bytes) < 0) return false;
  return true;
}

bool Socket::Read(void* data, size_t bytes) {
  if(read(sockfd, data, bytes) < 0) return false;
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
