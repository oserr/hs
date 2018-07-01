#include <functional>
#include <random>
#include <vector>
#include <utility>

#include "gtest/gtest.h"

#include "haystack.hh"
#include "needle.hh"

#ifndef PREFIX
 #error Need to define PREFIX with file path
#endif

namespace {

constexpr uint64_t kPadding = sizeof(NeedleFlags);

struct HaystackTest : public ::testing::Test
{
    static constexpr int kSamples = 20;
    static constexpr int kBuffLimit = 1024;
    uint64_t totalSize;
    char buff[kBuffLimit];
    std::vector<Needle> needles;
    std::vector<std::vector<char>> fileData;

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

        uint64_t offset = 0;
        for (int i = 0; i < kSamples; ++i) {
            auto size = genSize();
            needles.emplace_back(0, offset, i, size);
            std::vector<char> buffer(size);
            for (auto &c : buffer)
                c = genByte();
            fileData.emplace_back(std::move(buffer));
            offset += kPadding + size;
        }
        totalSize = offset;
    }
};

constexpr int HaystackTest::kBuffLimit;

TEST(Haystack, CtorWorks)
{
    Haystack hs(0, PREFIX, 500);
    EXPECT_EQ(500u, hs.FreeCount())
        << "Prefix=" PREFIX;
}

TEST_F(HaystackTest, CanWriteNeedleAndThenReadItAfterClosingFile)
{
    auto &needle = needles[0];
    auto &bytes = fileData[0];
    EXPECT_LE(static_cast<size_t>(kBuffLimit >> 1), bytes.size());
    // Create a needle in the file and then close the file.
    auto eFreeBytes = totalSize+1-bytes.size()-kPadding;
    {
        Haystack hs(0, PREFIX, totalSize+1);
        auto result = hs.Write(needle.flags.id, bytes.data(), bytes.size());
        EXPECT_EQ(eFreeBytes, hs.FreeCount());
        EXPECT_EQ(needle, result);
    }

    // Now reopen the file and re-read the data.
    Haystack hs(0, PREFIX, totalSize+1, true);
    EXPECT_EQ(eFreeBytes, hs.FreeCount());
    hs.Read(needle, buff);
    EXPECT_TRUE(std::equal(bytes.begin(), bytes.end(), buff));
}

TEST_F(HaystackTest, CanReadAndWriteNeedlesRandomly)
{
    // Create a needle in the file and then close the file.
    {
        Haystack hs(0, PREFIX, totalSize+1);
        for (size_t i = 0, last = kSamples >> 1; i < last; ++i) {
            auto &needle = needles[i];
            auto &bytes = fileData[i];
            auto result = hs.Write(needle.flags.id, bytes.data(), bytes.size());
            EXPECT_EQ(needle, result);
        }
        for (auto i : {6, 3, 8, 5, 0, 2, 1}) {
            auto &needle = needles[i];
            auto &bytes = fileData[i];
            hs.Read(needle, buff);
            EXPECT_TRUE(std::equal(bytes.begin(), bytes.end(), buff));
        }
    }

    // Now reopen the file and re-read the data.
    Haystack hs(0, PREFIX, totalSize+1, true);
    // Should be able to read the same needles
    for (auto i : {6, 3, 8, 5, 0, 2, 1}) {
        auto &needle = needles[i];
        auto &bytes = fileData[i];
        hs.Read(needle, buff);
        EXPECT_TRUE(std::equal(bytes.begin(), bytes.end(), buff));
    }

    // Write the rest of the data
    for (size_t i = kSamples >> 1; i < kSamples; ++i) {
        auto &needle = needles[i];
        auto &bytes = fileData[i];
        auto result = hs.Write(needle.flags.id, bytes.data(), bytes.size());
        EXPECT_EQ(needle, result);
    }

    // Should be able to read any of the needles we've inserted
    for (auto i : {6, 12, 19, 15, 1, 17, 9}) {
        auto &needle = needles[i];
        auto &bytes = fileData[i];
        hs.Read(needle, buff);
        EXPECT_TRUE(std::equal(bytes.begin(), bytes.end(), buff));
    }
}

TEST_F(HaystackTest, NeedlesReturnsTheCorrectSetOfNeedles)
{
    // Create a needle in the file and then close the file.
    Haystack hs(0, PREFIX, totalSize+1);
    auto last = kSamples >> 1;
    for (int i = 0; i < last; ++i) {
        auto &needle = needles[i];
        auto &bytes = fileData[i];
        hs.Write(needle.flags.id, bytes.data(), bytes.size());
    }

    auto results = hs.Needles();
    EXPECT_EQ(static_cast<size_t>(last), results.size());
    for (size_t i = 0; i < results.size(); ++i)
        EXPECT_EQ(needles[i], results[i]);

    for (auto i = last; i < kSamples; ++i) {
        auto &needle = needles[i];
        auto &bytes = fileData[i];
        hs.Write(needle.flags.id, bytes.data(), bytes.size());
    }

    results = hs.Needles();
    EXPECT_EQ(needles.size(), results.size());
    for (size_t i = 0; i < results.size(); ++i)
        EXPECT_EQ(needles[i], results[i]);
}

TEST_F(HaystackTest, DeleteMarksNeedlesAsDeletedCorrectly)
{
    // Create a needle in the file and then close the file.
    Haystack hs(0, PREFIX, totalSize+1);
    for (int i = 0; i < kSamples; ++i) {
        auto &needle = needles[i];
        auto &bytes = fileData[i];
        hs.Write(needle.flags.id, bytes.data(), bytes.size());
    }

    // Entries to be deleted.
    int indexes[] = {0, 5, 10, 15};

    // Calling delete should mark the needle as deleted
    for (auto i : indexes) {
        auto &needle = needles[i];
        hs.Delete(needle);
        EXPECT_TRUE(needle.flags.isDeleted);
    }

    // The needles should show that they are deleted.
    auto results = hs.Needles();
    EXPECT_TRUE(needles == results);

    // Trying to ask for needles that have been deleted should result in error.
    for (auto i : indexes) {
        auto &needle = needles[i];
        EXPECT_THROW(hs.Read(needle, buff), HaystackErr);
    }
}

} // namespace
