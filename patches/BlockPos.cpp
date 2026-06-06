// Copyright © 2026 SculkCatalystMC. All rights reserved.
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// SPDX-License-Identifier: MPL-2.0

#include "sculk/protocol/codec/level/block/BlockPos.hpp"

namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE {

void BlockPos::write(BinaryStream& stream) const {
    stream.writeVarInt(mX);
    stream.writeUnsignedVarInt(static_cast<std::uint32_t>(mY));
    stream.writeVarInt(mZ);
}

Result<> BlockPos::read(ReadOnlyBinaryStream& stream) {
    _SCULK_READ(stream.readVarInt(mX));
    std::uint32_t unsigned_y{};
    _SCULK_READ(stream.readUnsignedVarInt(unsigned_y));
    mY = static_cast<std::int32_t>(unsigned_y);
    return stream.readVarInt(mZ);
}

} // namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE
