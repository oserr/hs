#include <cstdlib>
#include <iostream>
#include <string>

#include "directory.hh"

constexpr int kDirIpAddr = 1;
constexpr int kDirPort = 2;
constexpr int kMongoUri = 3;
constexpr int kStoreIpAddr = 4;
constexpr int kStorePort = 5;
constexpr int kArgs = 6;

int
main(int argc, char *argv[])
{
    if (argc != kArgs) {
        std::cerr << "Error: unexpected number of arguments\n";
        std::cerr << "Usage: ./" << argv[0]
                  << "<dirIpAddrr> <dirPort> "
                  << "<mongoUri> "
                  << "<storeIpAddr> <storePort>\n";
        exit(EXIT_FAILURE);
    }
    Directory dir(
        argv[kDirIpAddr],
        std::stoi(argv[kDirPort]),
        argv[kMongoUri],
        argv[kStoreIpAddr],
        argv[kStorePort]);
    dir.Run();
    exit(EXIT_SUCCESS);
}
