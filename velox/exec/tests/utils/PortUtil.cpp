/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "velox/exec/tests/utils/PortUtil.h"
#ifndef _WIN32
#include <netinet/in.h>
#include <sys/socket.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#endif
#include <memory>
#include "velox/common/base/Exceptions.h"

namespace facebook::velox::exec::test {
namespace {

void getFreePortsImpl(int numPorts, int* ports) {
#ifdef _WIN32
  WSADATA wsaData;
  WSAStartup(MAKEWORD(2, 2), &wsaData);
  const std::unique_ptr<SOCKET[]> sockets(new SOCKET[numPorts]);
#else
  const std::unique_ptr<int[]> sockets(new int[numPorts]);
#endif
  for (int i = 0; i < numPorts; i++) {
#ifdef _WIN32
    SOCKET sock = socket(PF_INET, SOCK_STREAM, 0);
    VELOX_CHECK(sock != INVALID_SOCKET, "Error while creating socket: {}", WSAGetLastError());
#else
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    VELOX_CHECK_NE(sock, -1, "Error while creating socket: {}", errno);
#endif
    sockets[i] = sock;

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = INADDR_ANY;

    socklen_t len = sizeof(addr);
    int result = ::bind(sock, reinterpret_cast<sockaddr*>(&addr), len);
#ifdef _WIN32
    VELOX_CHECK_NE(result, SOCKET_ERROR, "Error while binding socket: {}", WSAGetLastError());
#else
    VELOX_CHECK_NE(result, -1, "Error while binding socket: {}", errno);
#endif

    result = getsockname(sock, reinterpret_cast<sockaddr*>(&addr), &len);
#ifdef _WIN32
    VELOX_CHECK_NE(result, SOCKET_ERROR, "Error on getsockname: {}", WSAGetLastError());
#else
    VELOX_CHECK_NE(result, -1, "Error on getsockname: {}", errno);
#endif

    ports[i] = ntohs(addr.sin_port);
  }

  for (int i = 0; i < numPorts; i++) {
#ifdef _WIN32
    closesocket(sockets[i]);
#else
    close(sockets[i]);
#endif
  }
#ifdef _WIN32
  WSACleanup();
#endif
}

} // namespace

std::vector<int> getFreePorts(std::size_t numPorts) {
  std::vector<int> ports;
  ports.resize(numPorts);
  getFreePortsImpl(numPorts, &ports[0]);
  return ports;
}

int getFreePort() {
  int port;
  getFreePortsImpl(1, &port);
  return port;
}

} // namespace facebook::velox::exec::test
