#pragma once

#include <memory>
#include <string>

#include <boost/asio.hpp>
#include <hiredis.h>

/**
 * The cache component.
 *
 * Requests to the store firt hit the cache. If the blob is found in the cache,
 * then the blob in the cache is served, otherwise the cache forwards the
 * request to the store. If the store replies with the blob, then the blob is
 * stored in the cache, and then the cache forwards the blob back to the client.
 * Internally, Cache does not cache anything, but instead relies on the Redis
 * for caching.
 */
class Cache
{
public:
    using TcpStream = boost::asio::ip::tcp::iostream;

private:
    static constexpr uint64_t kBuffSize = 1 << 20;

    // The IP address and port where the cache listens for requests.
    std::string cacheIpAddr;
    unsigned cachePort;

    // The IP address and port where Redis listens for requests.
    std::string redisIpAddr;
    unsigned redisPort;

    // The IP address and port where the store listens for requests.
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
