#include <cstdint>
#include <iostream>
#include <sstream>
#include <thread>

#include <boost/asio.hpp>

#include "store.hh"

// Define them here to avoid link errors
constexpr unsigned Store::kVolumes;
constexpr uint64_t Store::kMaxFileSize;
constexpr uint64_t Store::kMaxVolumeSize;

/**
 * Initializes a Store with an address to listen for connections, and the
 * prefix path of the directory whre the haystack files are located, or to
 * be created.
 *
 * @param ipAddr The IP address where it will listen for requests.
 * @param port The port number where it will listen for requests.
 * @param hayDir The prefix path were haystack files are located,
 *  or where they will be created.
 *
 * @details The ctor does not open files or open a listening socket until
 *  Run is executed.
 */
Store::Store(
    const std::string &ipAddr,
    unsigned port,
    const std::string &hayDir)
    : port(port),
      ipAddr(ipAddr),
      hayDir(hayDir)
 //     ioService(),
 //     acceptor(ioService, boost::asio::ip::tcp::endpoint(
 //              boost::asio::ip::address::from_string(ipAddr), port))
{}

/**
 * Sets up the Store to listen for requests to store and fetch files.
 *
 * @details Initializes the store by creating a set of haystack files, creating
 *  a listening socket, and spinning on a loop as it listens for requests.
 */
void
Store::Run()
{
    // Create the haystacks
    for (size_t i = 0; i < kVolumes; ++i) {
        auto p = std::make_shared<Haystack>(i, hayDir, kMaxVolumeSize);
        hayStacks.push_back(std::move(p));
    }

    using namespace boost::asio;
    io_service io_service;
    auto ipaddr = ip::address::from_string(ipAddr);
    ip::tcp::acceptor acceptor(io_service, ip::tcp::endpoint(ipaddr, port));

    for (;;) {
        try {
            auto conn = new ip::tcp::iostream;
            acceptor.accept(*conn->rdbuf());
            std::thread thr(&Store::HandleConnection, this, conn);
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
 * @param conn A pointer to a connection.
 * @details Responds to three commands: get, put, and delete. In each case, the
 *  handler responds with an ok message on success, or an err message on
 *  failure. If there is a failure, then it also replies with a brief
 *  description of the error message. The requests are expected to have the
 *  following format:
 *  - PUT: |put <needleId> <haystackId> <size><newline><message...>|
 *  - GET: |get <needleId>|
 *  - DELETE: |delete <needleId>|
 */
void
Store::HandleConnection(boost::asio::ip::tcp::iostream *conn)
{
    char buf[kMaxFileSize];
    std::string command, line;
    uint64_t needleId, volumeId, nBytes;
    conn->exceptions(std::ios::badbit);

    try {
        std::getline(*conn, line);
        std::istringstream iss(line);
        iss.exceptions(std::ios::badbit);

        iss >> command;
        if (command == "get") {
            iss >> needleId;
            auto nBytes = Get(needleId, buf);
            *conn << "ok " << nBytes << '\n';
            conn->write(buf, nBytes);
        }
        else if (command == "put") {
            iss >> volumeId >> needleId >> nBytes;
            if (volumeId >= kVolumes)
                *conn << "err BadHaystackId\n";
            else if (nBytes > kMaxFileSize)
                *conn << "err TooManyBytes\n";
            else {
                //conn->readsome(buf, nBytes);
                conn->read(buf, nBytes);
                nBytes = conn->gcount();
                // Ignore that we may get less, but store correct number of
                // bytes.
                Put(volumeId, needleId, buf, nBytes);
                *conn << "ok\n";
            }
        }
        else if (command == "delete") {
            iss >> needleId;
            Remove(needleId);
            *conn << "ok\n";
        }
        else
            *conn << "err BadCommand\n";
        conn->flush();
        delete conn;
    }
    catch (HaystackErr &err) {
        const char *msg;
        switch (err.reason()) {
        case HsErr::BadNeedle:
            msg = "err BadNeedle";
            break;
        case HsErr::NoFit:
            msg = "err NoFit";
            break;
        default:
            msg = "err Unknown";
            break;
        }
        try {
            *conn << msg << '\n';
            conn->flush();
        }
        catch (std::exception) {
            // Nothing to do
        }
        delete conn;
    }
    catch (std::exception) {
        delete conn;
    }
}

/**
 * Creats a new Needle in a Haystack.
 *
 * @param volumeId The Haystack ID.
 * @param needleId The ID to associate with the Needle.
 * @param buf The buffer containing the contents to be put in the Haystack.
 * @param size The number of bytes in the buffer.
 * @throw HaystackErr if there is a problem writing to the Haystack or inserting
 *  the Needle into the map of needles.
 */
void
Store::Put(uint64_t volumeId, uint64_t needleId, char *buf, uint64_t size)
{
    auto hs = hayStacks[volumeId];
    auto needle = hs->Write(needleId, buf, size);
    if (not needles.Put(needleId, needle)) {
        hs->Delete(needle);
        throw HaystackErr(HsErr::NoFit);
    }
}

/**
 * Gets a Needle content from a Haystack.
 *
 * @param needleId The Needle ID.
 * @param buf The buffer where the contents are to be copied.
 * @return The number of bytes copied into the buffer.
 * @throw HaystackErr if Needle is not found.
 */
uint64_t
Store::Get(uint64_t needleId, char *buf) const
{
    Needle needle;
    if (not needles.Get(needleId, needle))
        throw HaystackErr(HsErr::BadNeedle);
    auto hs = hayStacks[needle.haystackId];
    hs->Read(needle, buf);
    return needle.flags.size;
}

/**
 * Removes a needle from a Haystack.
 *
 * @param needleId The ID of the needle to remove.
 * @throw HaystackErr if Needle is not found.
 */
void
Store::Remove(uint64_t needleId)
{
    Needle needle;
    if (not needles.Get(needleId, needle))
        throw HaystackErr(HsErr::BadNeedle);
    auto hs = hayStacks[needle.haystackId];
    hs->Delete(needle);
}
