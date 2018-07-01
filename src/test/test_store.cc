#include <pthread.h>

#include <chrono>
#include <functional>
#include <random>
#include <thread>
#include <utility>
#include <vector>

#include <boost/asio.hpp>
#include "gtest/gtest.h"

#include "haystack.hh"
#include "needle.hh"
#include "store.hh"

#ifndef PREFIX
 #error Need to define PREFIX with file path
#endif

namespace {

struct StoreTest : public ::testing::Test
{
    static constexpr size_t kTotalFiles = 10;
    static constexpr size_t kBuffLimit = 4096;
    static constexpr size_t kVolumes = 5;
    std::string ipAddr{"0.0.0.0"};
    unsigned serverPort = 5000;
    char buf[kBuffLimit];
    // List of pairs of (needle ID, haystack ID)
    std::vector<std::pair<size_t, size_t>> ids;
    std::vector<std::vector<char>> fileData;
    Store store{ipAddr, serverPort, PREFIX};

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
            ids.emplace_back(i, i % kVolumes);
            auto size = genSize();
            std::vector<char> buffer(size);
            for (auto &c : buffer)
                c = genByte();
            fileData.emplace_back(std::move(buffer));
        }
    }
};

TEST_F(StoreTest, PutGetAndDeleteWork)
{
    std::thread thr(&Store::Run, &store);
    // Give the Store some time to setup before we request connection.
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    auto port = std::to_string(serverPort);

    // put all the needles into haystack
    for (size_t i = 0; i < kTotalFiles; ++i) {
        boost::asio::ip::tcp::iostream conn(ipAddr, port);
        auto needleId = ids[i].first;
        auto haystackId = ids[i].second;
        auto &bytes = fileData[i];
        conn << "put " << haystackId << ' ' << needleId << ' ' << bytes.size() << '\n';
        conn.write(bytes.data(), bytes.size());
        std::string response;
        conn >> response;
        ASSERT_EQ("ok", response)
            << "ERROR: needleId=" << needleId
            << " haystackId=" << haystackId;
    }

    // now try to retrieve all of them
    for (size_t i = 0; i < kTotalFiles; ++i) {
        boost::asio::ip::tcp::iostream conn(ipAddr, port);
        auto needleId = ids[i].first;
        auto &bytes = fileData[i];
        conn << "get " << needleId << '\n';
        size_t size;
        std::string response;
        std::getline(conn, response);
        std::istringstream iss(response);
        iss >> response >> size;
        ASSERT_EQ("ok", response);
        ASSERT_EQ(bytes.size(), size);
        conn.read(buf, size);
        EXPECT_TRUE(std::equal(bytes.begin(), bytes.end(), buf));
    }

    size_t deleteIndex[] = {1, 3, 7};

    // Now delete the following files
    for (auto i : deleteIndex) {
        boost::asio::ip::tcp::iostream conn(ipAddr, port);
        auto needleId = ids[i].first;
        conn << "delete " << needleId << '\n';
        std::string response;
        conn >> response;
        ASSERT_EQ("ok", response);
    }

    // Should not be able to fetch needles that have beend deleted.
    for (auto i : deleteIndex) {
        boost::asio::ip::tcp::iostream conn(ipAddr, port);
        auto needleId = ids[i].first;
        conn << "get " << needleId << '\n';
        std::string response;
        std::getline(conn, response);
        ASSERT_EQ("err BadNeedle", response);
    }

    pthread_cancel(thr.native_handle());
    thr.join();
}

} // namespace
