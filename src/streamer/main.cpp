/*
 * libdatachannel client example
 * Copyright (c) 2019-2020 Paul-Louis Ageneau
 * Copyright (c) 2019 Murat Dogan
 * Copyright (c) 2020 Will Munn
 * Copyright (c) 2020 Nico Chatzi
 * Copyright (c) 2020 Lara Mackey
 * Copyright (c) 2020 Erik Cota-Robles
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

#include "nlohmann/json.hpp"
#include "common.h"
#include "streamer/opusfileparser.hpp"
#include "streamer/helpers.hpp"

using namespace rtc;
using namespace std;
using namespace std::chrono_literals;

using json = nlohmann::json;

int stmain(std::shared_ptr<AVCodecContextManager> ctxmgr, std::atomic<bool> &shutdown_requested)
{
    rtc::InitLogger(rtc::LogLevel::Debug);
    auto pc = std::make_shared<rtc::PeerConnection>();

    pc->onStateChange(
        [](rtc::PeerConnection::State state)
        { std::cout << "State: " << state << std::endl; });

    pc->onGatheringStateChange([pc](rtc::PeerConnection::GatheringState state)
                               {
			std::cout << "Gathering State: " << state << std::endl;
			if (state == rtc::PeerConnection::GatheringState::Complete) {
				auto description = pc->localDescription();
				json message = {{"type", description->typeString()},
				                {"sdp", std::string(description.value())}};
				std::cout << message << std::endl;
			} });
    const rtc::SSRC ssrc = 42;
    rtc::Description::Video media("video", rtc::Description::Direction::SendOnly);
    media.addH264Codec(96); // Must match the payload type of the external h264 RTP stream
    media.addSSRC(ssrc, "video-send");
    auto track = pc->addTrack(media);

    auto rtpConfig = make_shared<RtpPacketizationConfig>(ssrc, "video-send", 96, H264RtpPacketizer::defaultClockRate);
    // create packetizer
    auto packetizer = make_shared<H264RtpPacketizer>(H264RtpPacketizer::Separator::LongStartSequence, rtpConfig);
    // create H264 handler
    auto h264Handler = make_shared<H264PacketizationHandler>(packetizer);
    // add RTCP SR handler
    auto srReporter = make_shared<RtcpSrReporter>(rtpConfig);
    h264Handler->addToChain(srReporter);
    // add RTCP NACK handler
    auto nackResponder = make_shared<RtcpNackResponder>();
    h264Handler->addToChain(nackResponder);
    // set handler
    track->setMediaHandler(h264Handler);
    auto currentTime_us = double(currentTimeInMicroSeconds());
    auto currentTime_s = currentTime_us / (1000 * 1000);
    rtpConfig->timestamp = rtpConfig->secondsToTimestamp(currentTime_s);
    srReporter->setNeedsToReport();
    srReporter->rtpConfig->setStartTime(currentTime_s, RtpPacketizationConfig::EpochStart::T1970);
    srReporter->startRecording();

    pc->setLocalDescription();

    std::cout << "Please copy/paste the answer provided by the browser: " << std::endl;
    std::string sdp;
    std::getline(std::cin, sdp);

    json j = json::parse(sdp);
    rtc::Description answer(j["sdp"].get<std::string>(), j["type"].get<std::string>());
    pc->setRemoteDescription(answer);

    int report = 0;
    while (!shutdown_requested)
    {
        int ret;
        AVPacketManager pktmgr;
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
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            break;
        case 0:
            pkt->pts = pkt->dts = av_rescale_q(av_gettime(), AV_TIME_BASE_Q, (AVRational){1, 10});
            tlog::info() << "stmain: Received packet; pts=" << pkt->pts << " dts=" << pkt->dts << " size=" << pkt->size;
            currentTime_us = double(currentTimeInMicroSeconds());
            currentTime_s = currentTime_us / (1000 * 1000);
            rtpConfig->timestamp = rtpConfig->secondsToTimestamp(currentTime_s);

            report++;
            if (report == 50)
            {
                srReporter->setNeedsToReport();
                report = 0;
            }

            track->send(reinterpret_cast<const std::byte *>(pkt->data), pkt->size);
            break;
        case AVERROR(EINVAL): // codec not opened, or it is a decoder other errors: legitimate encoding errors
        default:
            tlog::error() << "stmain: Failed to receive packet: " << averror_explain(ret);
        case AVERROR_EOF: // the encoder has been fully flushed, and there will be no more output packets
            throw std::runtime_error("AVERROR(EINVAL) or AVERROR_EOF");
            return -1;
        }
    }
    return 0;
}
