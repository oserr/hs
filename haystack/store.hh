#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <boost/asio.hpp>

#include "asyncmap.hh"
#include "haystack.hh"

class Store
{
    // Total number of files used by store. Ideally, this would be configurable,
    // but hardcoding to simplify matters.
    static constexpr unsigned kVolumes = 5;

    // The maximum file size allowed per needle (1 MiB).
    static constexpr uint64_t kMaxFileSize = 1<<20;

    // The maximum volume size allowed for the file containing all the needles
    // for a given volume (1 GiB).
    static constexpr uint64_t kMaxVolumeSize = kMaxFileSize<<10;

    // The IP address and port where Store will listen for requests.
    unsigned port;
    std::string ipAddr;

    // A map of IDs to needles.
    AsyncMap<uint64_t, Needle> needles;

    // The prefix path where the haystack files are located, or to be created.
    std::string hayDir;

    // List of Hastack instances.
    std::vector<std::shared_ptr<Haystack>> hayStacks;

    void HandleConnection(boost::asio::ip::tcp::iostream *conn);
    void Put(uint64_t volumeId, uint64_t needleId, char *buf, uint64_t size);
    uint64_t Get(uint64_t needleId, char *buf) const;
    void Remove(uint64_t needleId);

public:
    Store(
        const std::string &ipAddr,
        unsigned port,
        const std::string &hayDir);

    // Listens for requests on a loop.
    void Run();
};
