#pragma once

#include <atomic>
#include <memory>
#include <string>

#include <boost/asio.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/instance.hpp>

class Directory
{
public:
    using TcpStream = boost::asio::ip::tcp::iostream;

private:
    static constexpr char const *kDbName = "HAYSTACK";
    static constexpr char const *kDbCollectionName = "NEEDLES";
    static constexpr unsigned kVolumes = 5;
    static constexpr uint64_t kMaxFileSize = 1 << 20;

    std::string dirIpAddr;
    unsigned dirPort;
    std::string mongoUri;
    std::string storeIpAddr;
    std::string storePort;

    std::atomic_int volumeCounter;
    std::atomic_llong idCounter;

    mongocxx::instance mongoInstance;

    void HandleConnection(std::unique_ptr<TcpStream> conn);
    void List(std::unique_ptr<TcpStream> conn);
    void Upload(std::unique_ptr<TcpStream> conn, uint64_t size);
    void Remove(std::unique_ptr<TcpStream> conn, uint64_t needleId);

public:
    Directory(
        const std::string &dirIpAddr,
        unsigned dirPort,
        const std::string &mongoUri,
        const std::string &storeIpAddr,
        const std::string &storePort);

    // Listens for requests on a loop.
    void Run();
};
