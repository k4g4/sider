#include "shared.h"
#include <algorithm>
#include <iostream>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

const size_t PEERS_LEN = 10;

class Server {
  int sock;
  sockaddr_in addr;

public:
  Server()
      : addr{
            .sin_family = AF_INET,
            .sin_port = htons(PORT),
            .sin_addr = {.s_addr = INADDR_ANY},
        } {
    if (0 > (sock = socket(AF_INET, SOCK_STREAM, 0))) {
      throw std::runtime_error("failed to create socket");
    }
    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                   sizeof(opt))) {
      throw std::runtime_error("failed to set socket options");
    }
    if (bind(sock, (sockaddr *)&addr, sizeof(addr))) {
      throw std::runtime_error("failed to bind socket");
    }
    if (listen(sock, PEERS_LEN)) {
      throw std::runtime_error("failed to prepare socket");
    }
  }

  ~Server() { close(sock); }

  void accept_loop() {
    int client_sock;

    while (true) {
      if (0 > (client_sock = accept(sock, nullptr, nullptr))) {
        throw std::runtime_error("failed to accept connection");
      }
      std::thread(&Server::handle_conn, this, client_sock).detach();
    }
  }

  void handle_conn(int client_sock) {
    int bytes_read;
    buffer buf{};

    while (0 < (bytes_read = read(client_sock, buf.data(), buf.size()))) {
      std::cout << "read: " << buf.data() << std::endl;
      transact(buf);
      std::cout << "writing: " << buf.data() << std::endl;

      if (0 > send(client_sock, buf.data(), buf.size(), 0)) {
        close(client_sock);
        throw std::runtime_error("failed to send data to client");
      }
    }

    close(client_sock);
  }

  void transact(buffer &buf) {
    std::reverse(buf.begin(), buf.begin() + strlen(buf.data()));
  }
};

int main() {
  try {
    Server server;
    std::cout << "listening..." << std::endl;
    server.accept_loop();
  } catch (std::runtime_error const &e) {
    std::cout << "error: " << e.what() << std::endl;
  }

  return 0;
}