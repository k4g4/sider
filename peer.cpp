#include "shared.h"
#include <arpa/inet.h>
#include <iostream>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

const char *ADDR = "127.0.0.1";

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
      close(sock);
      throw std::runtime_error("failed to set ip address");
    }
  }

  ~Client() { close(sock); }

  void handle_conn() {
    if (0 > connect(sock, (sockaddr *)&addr, sizeof(addr))) {
      throw std::runtime_error("failed to connect");
    }

    auto msg = "Hello!";
    if (0 > send(sock, msg, strlen(msg), 0)) {
      throw std::runtime_error("failed to send data");
    }

    int bytes_read;
    buffer buf{};
    while (0 < (bytes_read = read(sock, buf.data(), buf.size()))) {
      std::cout << "read: " << buf.data() << std::endl;

      auto msg = "Hello!";
      std::cout << "writing: " << msg << std::endl;

      if (0 > send(sock, msg, strlen(msg), 0)) {
        throw std::runtime_error("failed to send data");
      }
    }
  }
};

int main() {
  try {
    Client client;
    client.handle_conn();
  } catch (std::runtime_error const &e) {
    std::cout << "error: " << e.what() << std::endl;
  }

  return 0;
}