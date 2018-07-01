#include <ostream>

#include "needle.hh"


Needle::Needle()
    : haystackId(0), offset(0), flags()
{}

Needle::Needle(
    uint64_t haystackId,
    uint64_t offset,
    uint64_t needleId,
    uint64_t size)
    : haystackId(haystackId),
      offset(offset),
      flags(needleId, size)
{}

Needle::Needle(
    uint64_t haystackId,
    uint64_t offset,
    const NeedleFlags &nf)
    : haystackId(haystackId),
      offset(offset),
      flags(nf)
{}


std::ostream&
operator<<(std::ostream &os, const Needle &needle)
{
    os << "Needle(haystackId=" << needle.haystackId
       << ", offset=" << needle.offset
       << ", id=" << needle.flags.id
       << ", size=" << needle.flags.size
       << ", isDeleted=" << bool(needle.flags.isDeleted)
       << ")";
    return os;
}

bool
operator==(const NeedleFlags &nf1, const NeedleFlags &nf2) noexcept{
    return nf1.id == nf2.id
       and nf1.size == nf2.size
       and nf1.isDeleted == nf2.isDeleted;
}

bool
operator==(const Needle &n1, const Needle &n2) noexcept
{
    return n1.haystackId == n2.haystackId
       and n1.offset == n2.offset
       and n1.flags == n2.flags;
}
