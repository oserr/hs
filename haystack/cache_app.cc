#include <cstdlib>
#include <iostream>
#include <string>

#include "cache.hh"

constexpr int kCacheIpAddr = 1;
constexpr int kCachePort = 2;
constexpr int kRedisIpAddr = 3;
constexpr int kRedisPort = 4;
constexpr int kStoreIpAddr = 5;
constexpr int kStorePort = 6;
constexpr int kArgs = 7;

int
main(int argc, char *argv[])
{
    if (argc != kArgs) {
        std::cerr << "Error: unexpected number of arguments\n";
        std::cerr << "Usage: ./" << argv[0]
                  << "<cacheIpAddrr> <cachePort> "
                  << "<redisIpAddr> <redisPort> "
                  << "<storeIpAddr> <storePort>\n";
        exit(EXIT_FAILURE);
    }
    Cache cache(
        argv[kCacheIpAddr],
        std::stoi(argv[kCachePort]),
        argv[kRedisIpAddr],
        std::stoi(argv[kRedisPort]),
        argv[kStoreIpAddr],
        argv[kStorePort]);
    cache.Run();
    exit(EXIT_SUCCESS);
}
