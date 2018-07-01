#include "gtest/gtest.h"

#include "asyncmap.hh"
#include "needle.hh"

namespace {

using NeedleMap = AsyncMap<uint64_t, Needle>;

TEST(AsyncMap, DefaultCtorWorks)
{
    ASSERT_NO_THROW(NeedleMap{});
}

TEST(AsyncMap, CtorWithSizeWorks)
{
    ASSERT_NO_THROW(NeedleMap{10000});
}

TEST(AsyncMap, PutInsertsNewElements)
{
    Needle needle;
    NeedleMap needleMap;
    EXPECT_TRUE(needleMap.Put(0, needle));
    EXPECT_TRUE(needleMap.Put(1, needle));
}

TEST(AsyncMap, PutDoesNotInsertElementsThatAlreadyExist)
{
    Needle needle;
    NeedleMap needleMap;
    EXPECT_TRUE(needleMap.Put(0, needle));
    EXPECT_FALSE(needleMap.Put(0, needle));
}

TEST(AsyncMap, GetFindsExistingKeys)
{
    Needle needle1(1,1,1,1);
    Needle needle2(2,2,2,2);
    NeedleMap needleMap;
    needleMap.Put(1, needle1);
    needleMap.Put(2, needle2);

    Needle result;
    EXPECT_TRUE(needleMap.Get(1, result));
    EXPECT_EQ(result, needle1);

    EXPECT_TRUE(needleMap.Get(2, result));
    EXPECT_EQ(result, needle2);
}

TEST(AsyncMap, GetDoesNotFindNonExistingKeys)
{
    Needle needle;
    NeedleMap needleMap;
    EXPECT_FALSE(needleMap.Get(1, needle));
}

TEST(AsyncMap, RemoveErasesElementsIfTheyExist)
{
    NeedleMap needleMap;
    needleMap.Put(1, Needle{});
    EXPECT_TRUE(needleMap.Remove(1));
}

TEST(AsyncMap, RemoveDoesNotEraseElementsThatDontExist)
{
    NeedleMap needleMap;
    EXPECT_FALSE(needleMap.Remove(1));
}

} // namespace
