#pragma once

#include <cstdint>
#include <exception>
#include <fstream>
#include <mutex>
#include <vector>

#include "needle.hh"

// Haystack errors.
enum class HsErr
{
    BadNeedle,
    NoFit
};

// Haystack exception, which will contain an HsErr code.
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

/**
 * The Haystack component.
 *
 * It organizes multiple needles in a single file. The file is organized as an
 * array of needles. Each needle has an ID, a size attribute, a deleted status
 * flag attribute, and a binary blob attribute. The first three are fixed size,
 * the blob is variable, and the size attribute indicates the size of the blob
 * in terms of bytes. Thus, the information in each Haystack file is self
 * contained because everything, such as the number of needles, which are
 * deleted, and so on, can be reconstructed by simply traversing the file.
 */
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
