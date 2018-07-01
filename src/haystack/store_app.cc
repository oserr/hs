#include <cstdlib>
#include <iostream>
#include <string>

#include "store.hh"

constexpr int kIpAddr = 1;
constexpr int kPort = 2;
constexpr int kPrefixDir = 3;
constexpr int kArgs = 4;

int
main(int argc, char *argv[])
{
    if (argc != kArgs) {
        std::cerr << "Error: unexpected number of arguments\n";
        std::cerr << "Usage: ./" << argv[0] << "<ipAddr> <port> <prefixDir>\n";
        exit(EXIT_FAILURE);
    }
    Store store(argv[kIpAddr], std::stoi(argv[kPort]), argv[kPrefixDir]);
    store.Run();
    exit(EXIT_SUCCESS);
}
