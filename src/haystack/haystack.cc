#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>
#include <utility>

#include <boost/filesystem.hpp>

#include "haystack.hh"

namespace {
namespace fs = boost::filesystem;
using LockGuard = std::lock_guard<std::mutex>;
}

/**
 * Initializes a Haystack object.
 *
 * @param id The ID of the haystack, used to identify the Haystack. The ID is
 *  also used to name the haystack file in the form of haystack_ID.
 * @param path The prefix path where the haystack files are located, or where
 *  they should be created.
 * @param maxSize The maximum size of the file. If the file already exists and
 *  is larger or equal to maxSize, then Haystack configures itself to be in
 *  read-only mode.
 * @param fromFile Boolean flag indicating whether a new Haystack is being
 *  created from scratch, or is being associated with a pre-existing haystack
 *  file.
 */
Haystack::Haystack(
    unsigned id, const std::string &path, uint64_t maxSize, bool fromFile)
    : mtx(),
      file(),
      fname(),
      maxSize(maxSize),
      currentSize(0),
      id(id),
      isReadOnly(false)
{
    auto name = "haystack_" + std::to_string(id);
    if (path.empty())
        fname = std::move(name);
    else if (path.back() == '/')
        fname = path + name;
    else
        fname = path + '/' + name;

    file.exceptions(std::ios::badbit);


    if (not fromFile) {
        // Since we are creating the file from scratch, we assume that the
        // directory may not exist.
        if (not fs::exists(path))
            fs::create_directory(path);
        constexpr auto mode = std::ios::in | std::ios::out
                        | std::ios::trunc | std::ios::binary;
        file.open(fname, mode);
    }
    else {
        constexpr auto mode = std::ios::in | std::ios::out
                            | std::ios::binary;
        // We assume file already exists, and will throw error if it doesn't.
        file.open(fname, mode);
        currentSize = fs::file_size(fname);
        isReadOnly = currentSize >= maxSize;
    }
}

/**
 * Dtor. Flushes and closes the file.
 */
Haystack::~Haystack()
{
    file.flush();
    file.close();
}

/**
 * @return The number of free bytes that can be used to store content.
 */
uint64_t
Haystack::FreeCount() const noexcept
{
    LockGuard lk(mtx);
    return maxSize - currentSize;
}

/**
 * Reads the contents of a Needle.
 *
 * @param needle The Needle which tells Haystack how to find the data.
 * @param buff The buffer where the data is copied. It is assumed that the
 *  buffer is allocated and big enough to accomodate the data associated with
 *  the needle.
 * @throw A HaystackErr if the needle is not for this Haystack, the offset is
 *  larger than the file, or the ID, size, or delete status extracted from the
 *  file do not match the Needle.
 */
void
Haystack::Read(const Needle &needle, char *buff) const
{
    LockGuard lk(mtx);
    constexpr auto kFlagsSize = sizeof(NeedleFlags);

    if (needle.haystackId != id or needle.offset >= currentSize-kFlagsSize)
        throw HaystackErr(HsErr::BadNeedle);

    NeedleFlags nf;
    file.seekg(needle.offset);
    file.read(reinterpret_cast<char*>(&nf), kFlagsSize);

    if (nf.isDeleted or nf.id != needle.flags.id
        or nf.size != needle.flags.size)
        throw HaystackErr(HsErr::BadNeedle);

    file.read(buff, nf.size);
}

/**
 * Saves an object to the haystack and creates a Needle from it.
 *
 * @param needleId The ID of the new Needle.
 * @param buff The buffer with the data to be saved in the haystack.
 * @param size The size of the buffer in bytes.
 * @throw A HaystackErr if Haystack is in read-only mode, or the Needle does not
 *  fit in the haystack.
 */
Needle
Haystack::Write(uint64_t needleId, char *buff, uint64_t size)
{
    LockGuard lk(mtx);
    constexpr auto kFlagsSize = sizeof(NeedleFlags);

    if (isReadOnly or currentSize+kFlagsSize+size > maxSize)
        throw HaystackErr(HsErr::NoFit);

    Needle needle(id, currentSize, needleId, size);
    file.seekp(currentSize);
    file.write(reinterpret_cast<char*>(&needle.flags), kFlagsSize);
    file.write(buff, size);

    currentSize += kFlagsSize + size;
    isReadOnly = currentSize >= maxSize;

    return needle;
}

/**
 * Marks a Needle as deleted.
 *
 * @param needle The haystack needle. The isDeleted flag is set to 1 after the
 *  needle is deleted.
 * @throw A HaystackErr if the Needle information does not match the haystack or
 *  what is found at the offset provided by the needle.
 */
void
Haystack::Delete(Needle &needle)
{
    LockGuard lk(mtx);
    constexpr auto kFlagsSize = sizeof(NeedleFlags);

    if (needle.haystackId != id or needle.offset+kFlagsSize > currentSize)
        throw HaystackErr(HsErr::BadNeedle);

    NeedleFlags nf;
    file.seekg(needle.offset);
    file.read(reinterpret_cast<char*>(&nf), kFlagsSize);

    if (nf.id != needle.flags.id)
        throw HaystackErr(HsErr::BadNeedle);

    needle.flags.isDeleted = 1;
    if (not nf.isDeleted) {
        const auto offset = needle.offset + offsetof(NeedleFlags, isDeleted);
        file.seekp(offset);
        file.write(&needle.flags.isDeleted, sizeof(char));
    }
}

/**
 * Marks a Needle as deleted.
 *
 * @param needle The haystack needle.
 * @throw A HaystackErr if the Needle information does not match the haystack or
 *  what is found at the offset provided by the needle.
 */
std::vector<Needle>
Haystack::Needles()
{
    LockGuard lk(mtx);
    constexpr auto kFlagsSize = sizeof(NeedleFlags);

    NeedleFlags nf;
    std::vector<Needle> needles;
    for (uint64_t pos = 0; pos < currentSize;) {
        file.seekg(pos);
        file.read(reinterpret_cast<char*>(&nf), kFlagsSize);
        needles.emplace_back(id, pos, nf);
        pos += kFlagsSize + nf.size;
    }

    return needles;
}
