# README

Ubiquiti home assignment for Embedded Networking Engineer position.

AI was only used to understand how to work with boost::asio and C++ syntax explanations, since this is my first time programming in C++.

# System Monitor Daemon

A Linux daemon that provides system monitoring information (CPU load and memory usage) over TCP connections. Supports multiple concurrent clients using asynchronous I/O.

## Requirements

- Linux system (tested on Ubuntu 24.04.2)
- C++11
- Boost.Asio library
- Make

## Installation

Install Boost development libraries:
```bash
sudo apt-get install libboost-all-dev
```
