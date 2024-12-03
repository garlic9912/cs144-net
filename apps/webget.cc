#include "socket.hh"

#include <cstdlib>
#include <iostream>
#include <span>
#include <string>

#include "tcp_minnow_socket.hh"

using namespace std;

void get_URL( const string& host, const string& path )
{
  // host: cs144.keithw.org
  // path: /hello
  CS144TCPSocket sock;
  sock.connect(Address(host, "http"));

  // 构造 HTTP 请求
  string request = "GET " + path + " HTTP/1.1\r\n"
                        "Host: " + host + "\r\n"
                        "Connection: close\r\n\r\n"; 

  // 这一行代码的作用是关闭Socket的写端
  // 表示你不再向服务器发送数据，但仍然可以接收来自服务器的数据
  sock.write(request);
  sock.shutdown(SHUT_WR);
  string buf;
  while(!sock.eof()){
      sock.read(buf);
      cout << buf;
  }
  sock.close();
  return;  
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort(); // For sticklers: don't try to access argv[0] if argc <= 0.
    }

    // auto 由编译器用于自动推导变量的类型
    // std::span 可以看作是一个智能指针，但它不管理内存的生命周期，只是提供了一个安全的方式来访问已存在的数据
    // 创建了一个span类型，用来随机存取数据
    auto args = span( argv, argc );

    // The program takes two command-line arguments: the hostname and "path" part of the URL.
    // Print the usage message unless there are these two arguments (plus the program name
    // itself, so arg count = 3 in total).
    if ( argc != 3 ) {
      cerr << "Usage: " << args.front() << " HOST PATH\n";
      cerr << "\tExample: " << args.front() << " stanford.edu /class/cs144\n";
      return EXIT_FAILURE;
    }

    // Get the command-line arguments.
    const string host { args[1] };
    const string path { args[2] };

    // Call the student-written function.
    get_URL( host, path );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}