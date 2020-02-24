#include "shared.hpp"

static fileDownload_t cl_fileDL;

//TODO: Set port as an argv argument?
int main(int argc, char** argv) {

  if(ShouldPrintHelp(argc, argv)) {
    printf("Requests file from server, and saves it on this machine.");
    printf("\n\n");
    printf("%s <hostname> <port> [-debug=x] [-output <filename>]\n", argv[0]);
    printf("\n");
    printf("output   Determines what file from the server will get returned.\n");
    printf("\n");
    printf("debug    Determines whether debug info gets written to the the terminal.\n");
    printf("         By default, this number is 0. Allowed range: [0,1].\n");

    return 0;
  }

  // fileDownloadSource_t source = { "Test", "test.au.dk", "10.0.0.1", 8000 }; Used for client applications
  // could be an array of sources.
  
  Socket sock(cl_fileDL);
  if(sock.Create()) {;
    
    if(!sock.ListenBegin()) {
 
    }

    while(1) {
      if(sock.AcceptNextConnection()) {
        
      }
    }

  }
    
  return 0;
}
