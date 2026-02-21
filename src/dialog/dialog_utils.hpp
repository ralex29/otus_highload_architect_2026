#pragma once

#include <cstddef>
#include <cstdint>

#include <boost/uuid/uuid.hpp>

namespace social_net_service::dialog
{
    inline constexpr size_t kBucketCount = 1024;

    inline boost::uuids::uuid MinUuid(const boost::uuids::uuid& u1,
                                      const boost::uuids::uuid& u2)
    {
        return u1 < u2 ? u1 : u2;
    }

    inline size_t FnvHash(const boost::uuids::uuid& uuid)
    {
        constexpr uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
        constexpr uint64_t kFnvPrime = 1099511628211ULL;

        uint64_t hash = kFnvOffsetBasis;
        for (const auto byte : uuid.data)
        {
            hash ^= static_cast<uint64_t>(byte);
            hash *= kFnvPrime;
        }
        return static_cast<size_t>(hash);
    }

    inline int GetVirtualBucket(const boost::uuids::uuid& user1,
                                 const boost::uuids::uuid& user2)
    {
        const auto key = MinUuid(user1, user2);
        return static_cast<int>(FnvHash(key) % kBucketCount);
    }

} // namespace social_net_service::dialog
