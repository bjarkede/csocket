#include "shared.hpp"

void error(const char* msg) {
  perror(msg);
  exit(0);
}

//@TODO: Enhance the socket so it can handle multiple connections.
int main(int argc, char** argv) {

  if(ShouldPrintHelp(argc, argv)) {
    printf("Requests file from server, and saves it on this machine.\n");
    printf("\n");
    printf("<hostname> <port> <filename>\n");

    return 0;
  }

  const char* hostname = argv[1];
  const s32   port     = atoi(argv[2]);
  const char* filename = argv[3];

  Socket server;
  File file;
  
  if(!server.Open(port)) {
    printf("Error: Couldn't open port: [%i].\n", port);
    return 0;
  }

  if(!server.Connect(hostname)) {
    printf("Error: Couldn't find host: %s@%i\n", hostname, port); 
    return 0;
  }

  // Requesting the file
  server.Write(filename, sizeof(filename));
  
  // Getting the file
  Buffer buffer;
  size_t bytes;

  void* data; 

  server.Read(data, 1000);
  bytes = (s64)data;
  
  if(AllocBuffer(buffer, bytes)) {
    for(int i = 0; i < bytes; i + 1000) {
    if(!server.Read(buffer.buffer, 1000)) {
	printf("Error: Couldn't read from socket.\n");
	return 0;
      }
    }
	
    file.Open("C:/temp/file", "w");
    file.Write(buffer.buffer, buffer.length);
    file.Close();
  }

  printf("File transfer complete.\n");
  
  return 0;
}
