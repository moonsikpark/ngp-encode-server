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

#include "streamer/ffmpegstreamsource.hpp"
#include <fstream>

using namespace std;

FFMpegStreamSource::FFMpegStreamSource(std::shared_ptr<AVCodecContextManager> ctxmgr) : ctxmgr(ctxmgr), sampleDuration_us(1000 * 1000 / 30), StreamSource()
{
    tlog::info() << "FFMpegStreamSource: ctor";
}

void FFMpegStreamSource::start()
{
    sampleTime_us = std::numeric_limits<uint64_t>::max() - sampleDuration_us + 1;
    loadNextSample();
}

void FFMpegStreamSource::stop()
{
    StreamSource::stop();
}

void FFMpegStreamSource::loadNextSample()
{
    tlog::info() << "FFMpegStreamSource::loadNextSample(): enter";
    int ret;
    AVPacketManager pktmgr;
    vector<uint8_t> contents;
    while (true)
    {
        tlog::info() << "FFMpegStreamSource::loadNextSample(): avcodec_receive_packet";
        AVPacket *pkt = pktmgr.get();
        {
            // The lock must be in this scope so that it would be unlocked right after avcodec_receive_packet() returns.
            ResourceLock<std::mutex, AVCodecContext> avcodeccontextlock{ctxmgr->get_mutex(), ctxmgr->get_context()};
            ret = avcodec_receive_packet(avcodeccontextlock.get(), pkt);
        }
        switch (ret)
        {
        case AVERROR(EAGAIN): // output is not available in the current state - user must try to send input
            // We must sleep here so that other threads can acquire AVCodecContext.
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            break;
        case 0:
            tlog::info() << "FFMpegStreamSource::loadNextSample(): Received packet; pts=" << pkt->pts << " dts=" << pkt->dts << " size=" << pkt->size;
            contents.resize(pkt->size);
            memcpy(contents.data(), pkt->data, pkt->size);
            sample = *reinterpret_cast<vector<byte> *>(&memcpy);
            sampleTime_us += sampleDuration_us;
            return;
        case AVERROR(EINVAL): // codec not opened, or it is a decoder other errors: legitimate encoding errors
        default:
            tlog::error() << "FFMpegStreamSource::loadNextSample(): Failed to receive packet: " << averror_explain(ret);
        case AVERROR_EOF: // the encoder has been fully flushed, and there will be no more output packets
            throw std::runtime_error("AVERROR(EINVAL) or AVERROR_EOF");
            return;
        }
    }
}

vector<byte> FFMpegStreamSource::initialNALUS()
{
    vector<byte> units{};
    if (previousUnitType7.has_value())
    {
        auto nalu = previousUnitType7.value();
        units.insert(units.end(), nalu.begin(), nalu.end());
    }
    if (previousUnitType8.has_value())
    {
        auto nalu = previousUnitType8.value();
        units.insert(units.end(), nalu.begin(), nalu.end());
    }
    if (previousUnitType5.has_value())
    {
        auto nalu = previousUnitType5.value();
        units.insert(units.end(), nalu.begin(), nalu.end());
    }
    return units;
}
