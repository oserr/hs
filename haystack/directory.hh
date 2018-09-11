#pragma once

#include <atomic>
#include <memory>
#include <string>

#include <boost/asio.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/instance.hpp>

/**
 * The directory component.
 *
 * Maps needle IDs to stores. It can receive commands to upload blobs, list the
 * IDs of the blobs in the stores, and to remove a blob. When it receives an
 * upload request, it allocates a needle ID for the blob, chooses a store where
 * to put the blob, and forwards the upload command to the chosen store. On a
 * list command, it simply responds with all the needle IDs in the stores. On a
 * remove command, the blob is not removed from the store, but the needle ID is
 * no longer included with the other needle IDs when a needle ID list is
 * provided.
 *
 * The directory uses MongoDB to store the IDs.
 */
class Directory
{
public:
    using TcpStream = boost::asio::ip::tcp::iostream;

private:
    // The MongoDB database and collection names.
    static constexpr char const *kDbName = "HAYSTACK";
    static constexpr char const *kDbCollectionName = "NEEDLES";

    // The number of volumes in the store.
    static constexpr unsigned kVolumes = 5;
    static constexpr uint64_t kMaxFileSize = 1 << 20;

    // The IP address and port where Directory listens for requests.
    std::string dirIpAddr;
    unsigned dirPort;

    // The MongoDB URI.
    std::string mongoUri;

    // The IP address and port where Store listens for requests.
    std::string storeIpAddr;
    std::string storePort;

    // Numbers to keep track of the volume number and ID for the next needle.
    std::atomic_int volumeCounter;
    std::atomic_llong idCounter;

    // The MongoDB instance.
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
