#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>
#include <utility>

#include <boost/asio.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/exception/exception.hpp>

#include "directory.hh"

// Define here to avoid link errors
constexpr char const *Directory::kDbName;
constexpr char const *Directory::kDbCollectionName;
constexpr unsigned Directory::kVolumes;
constexpr uint64_t Directory::kMaxFileSize;

/**
 * Initializes a Directory with the address where it will listen for
 * connections, the address of the MongoDB it uses to store needle information,
 * and the address of the Store where needles are stored.
 *
 * @param dirIpAddr The address where it listens for requests.
 * @param dirPort The port number where it listens for requests.
 * @param mongoUri The URI to connect to the MongoDB.
 * @param storeIpAddr The Store's IP address.
 * @param storePort The Store's port number.
 *
 * @details In additiona to initializing the paremters listed above, the
 *  constructor also initializes two atomic counters used to determine the ID for
 *  a new needle and volume where it should be stored, and a MongoDB instance
 *  that needs to be created before creating MongoDB clients.
 */
Directory::Directory(
    const std::string &dirIpAddr,
    unsigned dirPort,
    const std::string &mongoUri,
    const std::string &storeIpAddr,
    const std::string &storePort)
    : dirIpAddr(dirIpAddr),
      dirPort(dirPort),
      mongoUri(mongoUri),
      storeIpAddr(storeIpAddr),
      storePort(storePort),
      volumeCounter(0),
      idCounter(0),
      mongoInstance()
{}

/**
 * Listens for requests in a loop, and launches a thread to handle each
 * connection.
 */
void
Directory::Run()
{
    using namespace boost::asio;
    io_service io_service;
    ip::tcp::acceptor acceptor(
        io_service,
        ip::tcp::endpoint(ip::address::from_string(dirIpAddr), dirPort));

    for (;;) {
        try {
            std::unique_ptr<TcpStream> conn(new TcpStream);
            acceptor.accept(*conn->rdbuf());
            std::thread thr(
                &Directory::HandleConnection, this, std::move(conn));
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
 * @details Responds to two commands: upload, and list:
 *  - upload: |upload <size>|
 *    This uploads a new object to the store. <size> specifies the number of
 *    bytes in the object. The command terminates with a new line, and the
 *    message is expected to follow the new line. Directory will obtain an ID
 *    for the object, and will decide in which volume of the Store to save the
 *    needle. If the command success, it replies with ok or err if there is an
 *    error.
 *  - list: |list|
 *    Replies with |ok<new line><list of IDs>|, where list of IDs is a JSON
 *    list.
 */
void
Directory::HandleConnection(std::unique_ptr<TcpStream> conn)
{
    try {
        conn->exceptions(std::ios::badbit);
        std::string line, command;
        std::getline(*conn, line);
        std::istringstream iss(line);
        iss.exceptions(std::ios::badbit);

        iss >> command;

        if (command == "list")
            List(std::move(conn));
        else if (command == "upload") {
            uint64_t size;
            iss >> size;
            Upload(std::move(conn), size);
        }
        else if (command == "delete") {
            uint64_t needleId;
            iss >> needleId;
            Remove(std::move(conn), needleId);
        }
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
 * Handles a list request.
 *
 * @param conn The pointer to a TCP stream.
 * @details Fetches the list of IDs from the MongoDB and forwards them to the
 *  client on the TCP stream. The list of IDs is sent back is a new line
 *  separated.
 */
void
Directory::List(std::unique_ptr<TcpStream> conn)
{
    try {
        // Access MongoDB for list needle/haystack IDs
        mongocxx::client mongoConn{mongocxx::uri{mongoUri}};
        auto coll = mongoConn[kDbName][kDbCollectionName];
        auto cursor = coll.find({});

        // Create list of newline separated IDs
        std::string msg;
        auto docFirst = cursor.begin();
        auto docLast = cursor.end();
        while (docFirst != docLast) {
            auto el = (*docFirst)["needleId"];
            msg += std::to_string(el.get_int64().value) + '\n';
            ++docFirst;
        }

        // Respond to the client
        *conn << "ok " << msg.size() << '\n' << msg;
        conn->flush();
    }
    catch (mongocxx::exception &err) {
        std::cerr << "mongoerr " << err.what() << std::endl;
        *conn << "err DbErr\n";
        conn->flush();
    }
    catch (std::exception &err) {
        std::cerr << "Error: " << err.what() << std::endl;
        if (not *conn) return;
        *conn << "err Unknown\n";
        conn->flush();
    }
}

/**
 * Uploads a new needle to the Store.
 *
 * @param conn A pointer to TCP stream for the connection.
 * @param size The size of the object to store in the Store.
 * @details Does the following:
 *  - Creates an ID for the needle
 *  - Selects the volume for the needle
 *  - Stores the object in the Store.
 *  - Saves the needle ID and volume info in the database.
 */
void
Directory::Upload(std::unique_ptr<TcpStream> conn, uint64_t size)
{
    char buf[size];

    try {
        conn->read(buf, size);

        // Get IDs
        auto haystackId = volumeCounter++ % kVolumes;
        auto needleId = idCounter++;

        // Save object in store
        TcpStream storeConn(storeIpAddr, storePort);
        storeConn << "put " << haystackId << ' '
                  << needleId << ' ' << size << '\n';
        storeConn.write(buf, size);
        std::string storeResponse;
        std::getline(storeConn, storeResponse);
        if (storeResponse.find("err") != std::string::npos) {
            *conn << storeResponse << '\n';
            conn->flush();
            return;
        }

        // Save needleId and haystackId in MongoDB
        mongocxx::client mongoConn{mongocxx::uri{mongoUri}};
        bsoncxx::builder::stream::document doc;
        doc << "needleId" << static_cast<int64_t>(needleId)
            << "haystackId" << static_cast<int>(haystackId);
        auto coll = mongoConn[kDbName][kDbCollectionName];
        coll.insert_one(doc.view());

        // Respond to client
        *conn << "ok " << needleId << '\n';
        conn->flush();
    }
    catch (mongocxx::exception &err) {
        std::cerr << "ERR MONGO: " << err.what() << std::endl;
        *conn << "err DbErr\n";
        conn->flush();
    }
    catch (std::exception &err) {
        std::cerr << "ERR: " << err.what() << std::endl;
        if (not *conn) return;
        *conn << "err Unknown\n";
        conn->flush();
    }
}

/**
 * Deletes a needle from the directory and the store.
 *
 * @param conn A pointer to TCP stream for the connection.
 * @param needleId The needle ID.
 */
void
Directory::Remove(std::unique_ptr<TcpStream> conn, uint64_t needleId)
{
    try {
        // Delete needle from store
        TcpStream storeConn(storeIpAddr, storePort);
        storeConn << "delete " << needleId << '\n';
        std::string storeResponse;
        std::getline(storeConn, storeResponse);
        if (storeResponse.find("ok") == std::string::npos) {
            *conn << storeResponse << '\n';
            conn->flush();
            return;
        }

        // Delete needleId from MongoDB
        mongocxx::client mongoConn{mongocxx::uri{mongoUri}};
        bsoncxx::builder::stream::document doc;
        doc << "needleId" << static_cast<int64_t>(needleId);
        auto coll = mongoConn[kDbName][kDbCollectionName];
        auto dbResult = coll.delete_one(doc.view());

        // Respond to client
        *conn << (dbResult ? "ok\n" : "err DbErr\n");
        conn->flush();
    }
    catch (mongocxx::exception &err) {
        std::cerr << "MongoErr: " << err.what() << std::endl;
        *conn << "err DbErr\n";
        conn->flush();
    }
    catch (std::exception &err) {
        std::cerr << "Err: " << err.what() << std::endl;
        if (not *conn) return;
        *conn << "err Unknown\n";
        conn->flush();
    }
}
