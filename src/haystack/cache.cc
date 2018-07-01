#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>
#include <utility>

#include <boost/asio.hpp>
#include <hiredis.h>

#include "cache.hh"

// Define here to avoid link errors
constexpr uint64_t Cache::kBuffSize;

/**
 * Initializes a Cache with the addresses of the Redis cache and Store.
 *
 * @param cacheIpAddr The IP address where the Cache listens for requests.
 * @param cachePort The port number where the Cache listens for requests.
 * @param redisIpAddr The Redis IP address.
 * @param redisPort The Redis port number.
 * @param storeIpAddr The Store's IP address.
 * @param storePort The Store's port number.
 */
Cache::Cache(
    const std::string &cacheIpAddr,
    unsigned cachePort,
    const std::string &redisIpAddr,
    unsigned redisPort,
    const std::string &storeIpAddr,
    const std::string &storePort)
    : cacheIpAddr(cacheIpAddr),
      cachePort(cachePort),
      redisIpAddr(redisIpAddr),
      redisPort(redisPort),
      storeIpAddr(storeIpAddr),
      storePort(storePort)
{}

/**
 * Listens for requests in a loop, and launches a thread to handle each
 * connection.
 */
void
Cache::Run()
{
    using namespace boost::asio;
    io_service io_service;
    ip::tcp::acceptor acceptor(
        io_service,
        ip::tcp::endpoint(ip::address::from_string(cacheIpAddr), cachePort));

    for (;;) {
        try {
            std::unique_ptr<TcpStream> conn(new TcpStream);
            acceptor.accept(*conn->rdbuf());
            std::thread thr(
                &Cache::HandleConnection, this, std::move(conn));
            thr.detach();
        }
        catch (std::exception& err) {
            std::cerr << "ERROR: " << err.what() << std::endl;
        }
    }
}

/**
 * Handles a connection request.
 *
 * @param conn A pointer to a TCP stream.
 * @details Responds to two commands: get, and delete:
 *  - get: |get <needleId>|
 *  - delete: |delete <needleId>|
 */
void
Cache::HandleConnection(std::unique_ptr<TcpStream> conn)
{
    try {
        conn->exceptions(std::ios::badbit);
        std::string command, needleId;

        *conn >> command >> needleId;

        if (command == "get")
            Get(std::move(conn), needleId);
        else if (command == "delete")
            Remove(std::move(conn), needleId);
        else {
            *conn << "err BadCommand\n";
            conn->flush();
        }
    }
    catch (std::exception) {
        // Prevent exception form terminating thread
    }
}

/**
 * Gets a neeldeId from the cache if its there. If the needle is not in the
 * cache, then it fetches the needle from the store, puts the needle in the
 * cache, returns the contents of the needle.
 *
 * @param conn A pointer to a TCP stream for the connection.
 * @param needleId The needle ID.
 */
void
Cache::Get(std::unique_ptr<TcpStream> conn, const std::string &needleId)
{
    char buf[kBuffSize];
    redisContext *rc = nullptr;
    redisReply *rp = nullptr;

    try {
        rc = ConnectToRedis();

        // Try a GET
        rp = static_cast<redisReply*>(
            redisCommand(rc,"GET %s", needleId.c_str()));
        if (not rp) {
            std::cerr << "CONNECTION ERROR: " << rc->errstr << std::endl;
            redisFree(rc);
            rc = nullptr;
            *conn << "err RedisErr\n";
            conn->flush();
            return;
        }
        else if (rp->type == REDIS_REPLY_STRING) {
            *conn << "ok " << rp->len << '\n';
            conn->write(rp->str, rp->len);
            conn->flush();
            freeReplyObject(rp);
            redisFree(rc);
            return;
        }
        freeReplyObject(rp);
        rp = nullptr;

        // Fetch object from store
        TcpStream storeConn(storeIpAddr, storePort);
        storeConn << "get " << needleId << '\n';
        std::string line, status;
        size_t nBytes;
        std::getline(storeConn, line);
        std::istringstream iss(line);
        iss >> status >> nBytes;
        if (status != "ok") {
            redisFree(rc);
            rc = nullptr;
            *conn << line << '\n';
            conn->flush();
            return;
        }
        else if (nBytes > kBuffSize) {
            redisFree(rc);
            rc = nullptr;
            *conn << "err TooBig\n";
            conn->flush();
            return;
        }
        storeConn.read(buf, nBytes);
        storeConn.close();
        *conn << "ok " << nBytes << '\n';
        conn->write(buf, nBytes);
        conn->flush();
        conn->close();

        // Cache the object in the Redis cache
        rp = static_cast<redisReply*>(
            redisCommand(rc,"SET %s %b", needleId.c_str(), buf, nBytes));
        if (not rp) {
            std::cerr << "ERROR: redis PUT: " << rc->errstr << std::endl;
            redisFree(rc);
            return;
        }
        freeReplyObject(rp);
        redisFree(rc);
    }
    catch (std::exception &err) {
        if (rc) redisFree(rc);
        if (rp) freeReplyObject(rp);
        std::cerr << "ERROR: " << err.what() << std::endl;
        if (not *conn) return;
        *conn << "err Unknown\n";
        conn->flush();
    }
}

/**
 * Deletes a needle from the Redis cache.
 *
 * @param conn A pointer to TCP stream for the connection.
 * @param needleId The needle ID.
 */
void
Cache::Remove(std::unique_ptr<TcpStream> conn, const std::string &needleId)
{
    redisContext *rc = nullptr;
    redisReply *rp = nullptr;

    try {
        rc = ConnectToRedis();
        rp = static_cast<redisReply*>(
            redisCommand(rc,"DEL %s", needleId.c_str()));
        if (not rp) {
            std::cerr << "ERROR: redis: " << rc->errstr << std::endl;
            redisFree(rc);
            rc = nullptr;
            *conn << "err RedisErr\n";
            conn->flush();
            return;
        }
        freeReplyObject(rp);
        rp = nullptr;
        *conn << "ok\n";
        conn->flush();
    }
    catch (std::exception &err) {
        if (rc) redisFree(rc);
        if (rp) freeReplyObject(rp);
        std::cerr << "ERROR: " << err.what() << std::endl;
        if (not *conn) return;
        *conn << "err Unknown\n";
        conn->flush();
    }
}

/**
 * Establishes a connection with a Redis cache.
 *
 * @return A pointer to the redis cache. If
 */
redisContext*
Cache::ConnectToRedis()
{
    struct timeval timeout = { 1, 500000 }; // 1.5 seconds
    auto rc = redisConnectWithTimeout(redisIpAddr.c_str(), redisPort, timeout);
    if (rc == nullptr or rc->err) {
        if (rc) redisFree(rc);
        std::cerr << "ERROR: redis: cannot connect" << std::endl;
        throw std::exception();
    }
    return rc;
}
