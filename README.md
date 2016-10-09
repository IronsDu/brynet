Accumulation-dev
=======

cross platform and high performance TCP network library using C++ 11.

## Features
* cross platform (Linux | Windows)
* High performance and safety use.
* none depend
* multi-threaded
* SSL support
* support HTTP、HTTPS、WebSocket protocol
* support ipv6

## Benchamrk
   under localhost, use centos 6.5 virtual mahcine(host machine is win10 i5)
* PingPong

  benchamrk's server and client both only use one thread, and packet size is 4k

  ![PingPong](image/pingpong.png "PingPong")

* Broadcast
  server use 4 network thread, client use 4 network thread and one logic thread. one packet size is 46 bytes.
  server broadcast packet to all client when recv one packet from any client.
  client send one packet when recv packet from server and it's id equal self.

  ![Broadcast](image/broadcast.png "Broadcast")

## About session safety
  this library use three layer ident one session.
  * first, use raw pointer named [DataSocket](https://github.com/IronsDu/accumulation-dev/blob/master/src/net/DataSocket.h#L30)
  * second, use int64_t number ident one session used some callback in [TCPService](https://github.com/IronsDu/accumulation-dev/blob/master/src/net/TCPService.h#L53) wrapper DataSocket of first layer
  * thrid, use smart pointer named [TCPSession::PTR](https://github.com/IronsDu/accumulation-dev/blob/master/src/net/WrapTCPService.h#L13), you can control session by TCPSession::PTR

i suggest you use the above second or thrid way

## Usage
  please see [examples](https://github.com/IronsDu/accumulation-dev/tree/master/examples)

## Users
* [redis proxy](https://github.com/IronsDu/DBProxy)
* [distributed game server framework](https://github.com/IronsDu/DServerFramework)
* [Joynet - lua network library](https://github.com/IronsDu/Joynet)
