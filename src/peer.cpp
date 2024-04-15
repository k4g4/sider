#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>

#include "shared.h"

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

    read_command(out);
    std::cout << "writing: " << out.data() << std::endl;
    if (0 > send(sock, out.data(), out.size(), 0)) {
      throw std::runtime_error("failed to send data");
    }

    while (0 < read(sock, in.data(), in.size())) {
      std::cout << "read:\n" << in.data() << std::endl;
      read_command(out);
      std::cout << "writing:\n" << out.data() << std::endl;

      if (0 > send(sock, out.data(), out.size(), 0)) {
        throw std::runtime_error("failed to send data");
      }
    }
  }

  void read_command(buffer &out) { strncpy(out.data(), "PING", out.size()); }
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