// Copyright (c) 2022 Moonsik Park.

#include "base/server/packet_stream.h"

#include "base/logging.h"

extern "C" {
#include "libavcodec/avcodec.h"  // AVPacket, AV_PKT_FLAG_KEY
}

void PacketStreamServer::consume_packet(AVPacket *pkt) {
  // TODO: Find better way to indicate key frame.
  pkt->data[0] = pkt->flags == AV_PKT_FLAG_KEY ? 0 : 1;
  send_to_all((const char *)pkt->data, pkt->size);
  // tlog::debug() << "PacketStreamServer: sent packet.";
}
