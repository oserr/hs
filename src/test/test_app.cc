#include <pthread.h>

#include <chrono>
#include <cstdlib>
#include <functional>
#include <random>
#include <set>
#include <thread>
#include <utility>
#include <vector>

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include "gtest/gtest.h"

#include "directory.hh"
#include "needle.hh"
#include "store.hh"
#include "cache.hh"

#ifndef PREFIX
 #error Need to define PREFIX with file path
#endif

namespace {

struct AppTest : public ::testing::Test
{
    using TcpStream = Directory::TcpStream;
    static constexpr size_t kTotalFiles = 10;
    static constexpr size_t kBuffLimit = 4096;
    static constexpr size_t kVolumes = 5;

    std::string cacheIpAddr{"0.0.0.0"};
    unsigned cachePort = 5000;
    std::string storeIpAddr{"0.0.0.0"};
    unsigned storePort = 5001;
    std::string redisIpAddr{"0.0.0.0"};
    unsigned redisPort = 5002;
    std::string dirIpAddr{"0.0.0.0"};
    unsigned dirPort = 5003;
    std::string mongoUri{"mongodb://localhost:27017"};

    char buf[kBuffLimit];
    std::vector<size_t> ids;
    std::vector<std::vector<char>> fileData;

    Store store{storeIpAddr, storePort, PREFIX};
    Cache cache{cacheIpAddr, cachePort, redisIpAddr,
        redisPort, storeIpAddr, std::to_string(storePort)};
    Directory directory{dirIpAddr, dirPort, mongoUri,
        storeIpAddr, std::to_string(storePort)};

    std::thread thrStore, thrDir, thrCache;

    virtual void
    SetUp() override
    {
        // Random generator for size of buffer. Since we don't vary the seed,
        // behavior is actually deterministic across different runs.
        auto genSize = std::bind(
            std::uniform_int_distribution<>(kBuffLimit >> 1, kBuffLimit),
            std::default_random_engine());

        // Random generator for bytes.
        auto genByte = std::bind(
            std::uniform_int_distribution<char>(),
            std::default_random_engine());

        for (size_t i = 0; i < kTotalFiles; ++i) {
            auto size = genSize();
            std::vector<char> buffer(size);
            for (auto &c : buffer)
                c = genByte();
            fileData.emplace_back(std::move(buffer));
        }

        boost::filesystem::create_directories("/tmp/mongo/data/db");
        std::system("mongod --dbpath /tmp/mongo/data/db &> /dev/null &");
        //std::system("mongod --dbpath /tmp/mongo/data/db");
        std::system("redis-server --port 5002 &> /dev/null &");
        thrStore = std::thread(&Store::Run, &store);
        thrDir = std::thread(&Directory::Run, &directory);
        thrCache = std::thread(&Cache::Run, &cache);
    }

    virtual void
    TearDown() override
    {
        std::system("kill $(ps | grep mongod | awk '{print $1}')");
        std::system("kill $(ps | grep redis-server | awk '{print $1}')");
        std::system("rm -fr /tmp/mongo");
        pthread_cancel(thrStore.native_handle());
        pthread_cancel(thrDir.native_handle());
        pthread_cancel(thrCache.native_handle());
        thrStore.join();
        thrDir.join();
        thrCache.join();
    }
};

TEST_F(AppTest, TheDirectoryCacheAndStoreWorkCorrectlyTogether)
{
    // Give servers time to setup
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    auto cachePortStr = std::to_string(cachePort);
    auto dirPortStr = std::to_string(dirPort);
    auto storePortStr = std::to_string(storePort);

    // Add needles.
    for (size_t i = 0; i < kTotalFiles; ++i) {
        TcpStream conn(dirIpAddr, dirPortStr);
        auto &bytes = fileData[i];
        conn << "upload " << bytes.size() << '\n';
        conn.write(bytes.data(), bytes.size());
        std::string line, response;
        std::getline(conn, line);
        std::istringstream iss(line);
        size_t id;
        iss >> response >> id;
        ASSERT_EQ("ok", response)
            << "line=" << line
            << " id=" << id;
        ids.push_back(id);
    }

    // Now fetch the data directly from the store.
    for (size_t i = 0; i < kTotalFiles; ++i) {
        TcpStream conn(storeIpAddr, storePortStr);
        auto needleId = ids[i];
        auto &bytes = fileData[i];
        conn << "get " << needleId << '\n';
        size_t size;
        std::string line, response;
        std::getline(conn, line);
        std::istringstream iss(line);
        iss >> response >> size;
        ASSERT_EQ("ok", response)
            << "line=" << line
            << " size=" << size;
        ASSERT_EQ(bytes.size(), size);
        conn.read(buf, size);
        EXPECT_TRUE(std::equal(bytes.begin(), bytes.end(), buf));
    }

    // Now fetch the data directly from the cache.
    for (size_t i = 0; i < kTotalFiles; ++i) {
        TcpStream conn(cacheIpAddr, cachePortStr);
        auto needleId = ids[i];
        auto &bytes = fileData[i];
        conn << "get " << needleId << '\n';
        size_t size;
        std::string line, response;
        std::getline(conn, line);
        std::istringstream iss(line);
        iss >> response >> size;
        ASSERT_EQ("ok", response)
            << "line=" << line
            << " size=" << size;
        ASSERT_EQ(bytes.size(), size);
        conn.read(buf, size);
        EXPECT_TRUE(std::equal(bytes.begin(), bytes.end(), buf));
    }

    // Now get the list of IDs, which should match the ones we received.
    std::set<size_t> idSet;
    {
        TcpStream conn(dirIpAddr, dirPortStr);
        conn << "list\n";
        std::string line, response;
        std::getline(conn, line);
        std::istringstream iss(line);
        size_t size;
        iss >> response >> size;
        ASSERT_EQ("ok", response)
            << "line=" << line
            << " size=" << size;
        size_t needleId;
        while (conn >> needleId)
            idSet.insert(needleId);
        ASSERT_EQ(ids.size(), idSet.size());
        auto last = idSet.end();
        for (auto id : ids) {
            auto it = idSet.find(id);
            ASSERT_TRUE(last != it);
        }
    }

    // Now delete all of the IDs
    for (auto &id : ids) {
        TcpStream conn(dirIpAddr, dirPortStr);
        conn << "delete " << id << '\n';
        std::string line, response;
        std::getline(conn, line);
        std::istringstream iss(line);
        iss >> response;
        ASSERT_EQ("ok", response) << "line=" << line;
    }

    // All fetches from the store should now result in error
    for (auto &id : ids) {
        TcpStream conn(storeIpAddr, storePortStr);
        conn << "get " << id << '\n';
        std::string response;
        conn >> response;
        ASSERT_EQ("err", response);
    }

    // List should not return anything now.
    {
        TcpStream conn(dirIpAddr, dirPortStr);
        conn << "list\n";
        std::string line, response, stuff;
        std::getline(conn, line);
        std::istringstream iss(line);
        size_t size = 0;
        iss >> response >> size;
        if (size and size <= kBuffLimit) {
            conn.read(buf, size);
            stuff.assign(buf, buf+size);
        }
        ASSERT_EQ("ok", response)
            << "line=" << line
            << " size=" << size
            << " stuff=" << stuff;
        ASSERT_EQ(0u, size)
            << "line=" << line
            << " stuff=" << stuff;
    }

    // But we should still be able to fetch from the cache.
    for (size_t i = 0; i < kTotalFiles; ++i) {
        TcpStream conn(cacheIpAddr, cachePortStr);
        auto needleId = ids[i];
        auto &bytes = fileData[i];
        conn << "get " << needleId << '\n';
        size_t size;
        std::string line, response;
        std::getline(conn, line);
        std::istringstream iss(line);
        iss >> response >> size;
        ASSERT_EQ("ok", response)
            << "line=" << line
            << " size=" << size;
        ASSERT_EQ(bytes.size(), size);
        conn.read(buf, size);
        EXPECT_TRUE(std::equal(bytes.begin(), bytes.end(), buf));
    }

    // Now delete everything from the cache
    for (auto id : ids) {
        TcpStream conn(cacheIpAddr, cachePortStr);
        conn << "delete " << id << '\n';
        std::string response;
        conn >> response;
        ASSERT_EQ("ok", response);
    }

    // Now fetching from cache should result in error
    for (auto id : ids) {
        TcpStream conn(cacheIpAddr, cachePortStr);
        conn << "get " << id << '\n';
        std::string line;
        conn >> line;
        ASSERT_EQ("err", line);
    }
}

} // namespace
