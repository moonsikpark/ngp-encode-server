/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   pov.h
 *  @author Moonsik Park, Korean Institute of Science and Technology
 **/

#ifndef _POV_H_
#define _POV_H_

#include <common.h>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

void pov_websocket_thread();

#endif // _POV_H_
