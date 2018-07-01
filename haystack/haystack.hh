#pragma once

#include <cstdint>
#include <exception>
#include <fstream>
#include <mutex>
#include <vector>

#include "needle.hh"

enum class HsErr
{
    BadNeedle,
    NoFit
};

class HaystackErr : std::exception
{
    HsErr err;
public:
    HaystackErr(HsErr err)
        : exception(), err(err) {}

    const char* what() const noexcept override
    {
        switch (err) {
        case HsErr::BadNeedle:
            return "HaystackErr(BadNeedle)";
        case HsErr::NoFit:
            return "HaystackErr(NoFit)";
        default:
            return "HaystackErr(Unknown)";
        }
    }

    HsErr reason() noexcept
    {
        return err;
    }
};

class Haystack
{
    mutable std::mutex mtx;  // To protect writing/reading to the file.
    mutable std::fstream file;  // The file object.
    std::string fname;  // The name of the file.
    uint64_t maxSize;  // The maximum size of the file.
    uint64_t currentSize;  // The current size of the file.
    unsigned id;  // The id of the object.
    bool isReadOnly;  // Read-only status flag.

public:
    Haystack(unsigned id,
             const std::string &path,
             uint64_t maxSize,
             bool fromFile = false);
    Haystack(const Haystack &hs) = delete;
    Haystack(Haystack&& hs) = delete;
    Haystack& operator=(const Haystack &hs) = delete;
    Haystack& operator=(Haystack&& hs) = delete;
    ~Haystack();

    uint64_t Id() const noexcept { return id; }
    uint64_t FreeCount() const noexcept;
    void Read(const Needle &needle, char *buff) const;
    Needle Write(uint64_t id, char *buff, uint64_t size);
    void Delete(Needle &needle);
    std::vector<Needle> Needles();
};
