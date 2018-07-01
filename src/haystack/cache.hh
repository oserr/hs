#pragma once

#include <memory>
#include <string>

#include <boost/asio.hpp>
#include <hiredis.h>

class Cache
{
public:
    using TcpStream = boost::asio::ip::tcp::iostream;

private:
    static constexpr uint64_t kBuffSize = 1 << 20;
    std::string cacheIpAddr;
    unsigned cachePort;
    std::string redisIpAddr;
    unsigned redisPort;
    std::string storeIpAddr;
    std::string storePort;

    void HandleConnection(std::unique_ptr<TcpStream> conn);
    void Get(std::unique_ptr<TcpStream> conn, const std::string &needleId);
    void Remove(std::unique_ptr<TcpStream> conn, const std::string &needleId);
    redisContext* ConnectToRedis();

public:
    Cache(
        const std::string &cacheIpAddr,
        unsigned cachePort,
        const std::string &redisIpAddr,
        unsigned redisPort,
        const std::string &storeIpAddr,
        const std::string &storePort);

    // Listens for requests on a loop.
    void Run();
};
