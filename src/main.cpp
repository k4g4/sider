#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <thread>

#include "protocol.h"
#include "shared.h"
#include "storage.h"

const size_t BACKLOG = 10;

class Server {
  int sock;
  sockaddr_in addr;
  Storage storage;

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
    if (listen(sock, BACKLOG)) {
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
      // using threads, could optimize with an event loop
      std::thread(&Server::handle_conn, this, client_sock).detach();
    }
  }

  void handle_conn(int client_sock) {
    buffer in{};
    std::string out;

    while (0 < read(client_sock, in.data(), in.size())) {
      std::cout << "read:\n" << in.data() << std::endl;
      out.clear();
      server_transact(storage, in, out);
      std::cout << "writing:\n" << out.data() << std::endl;

      if (0 > send(client_sock, out.data(), out.size(), 0)) {
        close(client_sock);
        throw std::runtime_error("failed to send data to client");
      }
    }

    close(client_sock);
  }
};

int main() {
  try {
    Server server;
    std::cout << "listening..." << std::endl;
    server.accept_loop();
  } catch (std::runtime_error const &e) {
    std::cerr << "error: " << e.what() << std::endl;
  }

  return 0;
}