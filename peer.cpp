#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>

const char *ADDR = "127.0.0.1";
const uint16_t PORT = 9999;
const size_t BUF_LEN = 1024;

class Client {
  int sock;
  struct sockaddr_in addr;

public:
  Client()
      : addr{
            .sin_family = AF_INET,
            .sin_port = htons(PORT),
        } {
    if (0 > (sock = socket(AF_INET, SOCK_STREAM, 0))) {
      throw std::runtime_error("failed to create socket");
    }
    if (0 >= inet_pton(AF_INET, ADDR, &addr.sin_addr)) {
      throw std::runtime_error("failed to set ip address");
    }
  }

  ~Client() { close(sock); }

  void send_hello() {
    if (0 > connect(sock, (sockaddr *)&addr, sizeof(addr))) {
      throw std::runtime_error("failed to connect");
    }
    send(sock, "Hello!", strlen("Hello!"), 0);
  }
};

int main() {
  try {
    Client client;
    client.send_hello();
    std::cout << "sent hello" << std::endl;
  } catch (std::runtime_error const &e) {
    std::cout << "error: " << e.what() << std::endl;
  }

  return 0;
}