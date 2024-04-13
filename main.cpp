#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>

const uint16_t PORT = 9999;
const size_t BUF_LEN = 1024;

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
    if (listen(sock, 1)) {
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
      handle_conn(client_sock);
      close(client_sock);
    }
  }

  void handle_conn(int client_sock) {
    int bytes_read;
    std::string buf(BUF_LEN, '\0');
    while (true) {
      if (0 > (bytes_read = read(client_sock, &buf.front(), BUF_LEN))) {
        close(client_sock);
        throw std::runtime_error("failed to read from connection");
      }
      if (!bytes_read) {
        return;
      }
      std::cout << "read " << bytes_read << " bytes: " << buf << std::endl;
      buf.assign(BUF_LEN, '\0');
    }
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