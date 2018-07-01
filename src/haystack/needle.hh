#pragma once

#include <cstdint>
#include <ostream>

struct NeedleFlags
{
    uint64_t id;
    uint64_t size;
    char isDeleted;

    NeedleFlags() = default;
    NeedleFlags(uint64_t id, uint64_t size)
        : id(id), size(size), isDeleted(0) {}
    NeedleFlags(const NeedleFlags &needleFlags) = default;
    NeedleFlags(NeedleFlags &&needleFlags) = default;
    NeedleFlags& operator=(const NeedleFlags &needleFlags) = default;
    NeedleFlags& operator=(NeedleFlags &&needleFlags) = default;
    ~NeedleFlags() = default;
};

struct Needle
{
    uint64_t haystackId;
    uint64_t offset;
    NeedleFlags flags;

    Needle();
    Needle(
        uint64_t haystackId,
        uint64_t offset,
        uint64_t needleId,
        uint64_t size);
    Needle(uint64_t haystackId, uint64_t offset, const NeedleFlags &nf);
    Needle(const Needle &needle) = default;
    Needle(Needle &&needle) = default;
    Needle& operator=(const Needle &needle) = default;
    Needle& operator=(Needle &&needle) = default;
    ~Needle() = default;
};

std::ostream&
operator<<(std::ostream &os, const Needle &n);

bool
operator==(const NeedleFlags &nf1, const NeedleFlags &nf2) noexcept;

bool
operator==(const Needle &n1, const Needle &n2) noexcept;
