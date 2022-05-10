/*
 * libdatachannel streamer example
 * Copyright (c) 2020 Filip Klembara (in2core)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ffmpegstreamsource_hpp
#define ffmpegstreamsource_hpp

#include <string>
#include <vector>
#include <common.h>
#include "streamer/stream.hpp"

class FFMpegStreamSource : public StreamSource
{
    std::optional<std::vector<std::byte>> previousUnitType5 = std::nullopt;
    std::optional<std::vector<std::byte>> previousUnitType7 = std::nullopt;
    std::optional<std::vector<std::byte>> previousUnitType8 = std::nullopt;
    std::shared_ptr<AVCodecContextManager> ctxmgr;
    uint64_t loopTimestampOffset = 0;

public:
    const uint64_t sampleDuration_us;

    virtual void start();
    virtual void stop();
    FFMpegStreamSource(std::shared_ptr<AVCodecContextManager> ctxmgr);
    virtual void loadNextSample();
    std::vector<std::byte> initialNALUS();
};

#endif /* ffmpegstreamsource_hpp */
