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

    buffer in{}, out{};

    strncpy(out.data(), "Hello!", out.size());
    std::cout << "writing: " << out.data() << std::endl;
    if (0 > send(sock, out.data(), out.size(), 0)) {
      throw std::runtime_error("failed to send data");
    }

    int bytes_read;
    while (0 < (bytes_read = read(sock, in.data(), in.size()))) {
      std::cout << "read: " << in.data() << std::endl;
      if (0 == strncmp("Bye!", in.data(), in.size())) {
        break;
      }

      strncpy(out.data(), "Goodbye!", out.size());
      std::cout << "writing: " << out.data() << std::endl;

      if (0 > send(sock, out.data(), out.size(), 0)) {
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
    std::cerr << "error: " << e.what() << std::endl;
  }

  return 0;
}