/*
 * Copyright (c) 2020 Roc authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//! @file roc_peer/receiver.h
//! @brief Receiver peer.

#ifndef ROC_PEER_RECEIVER_H_
#define ROC_PEER_RECEIVER_H_

#include "roc_address/endpoint_uri.h"
#include "roc_address/interface.h"
#include "roc_address/protocol.h"
#include "roc_core/mutex.h"
#include "roc_ctl/control_loop.h"
#include "roc_peer/basic_peer.h"
#include "roc_peer/context.h"
#include "roc_pipeline/itask_scheduler.h"
#include "roc_pipeline/receiver_source.h"
#include "roc_rtp/format_map.h"

namespace roc {
namespace peer {

//! Receiver peer.
class Receiver : public BasicPeer, private pipeline::ITaskScheduler {
public:
    //! Initialize.
    Receiver(Context& context, const pipeline::ReceiverConfig& pipeline_config);

    //! Deinitialize.
    ~Receiver();

    //! Check if successfully constructed.
    bool valid();

    //! Set multicast interface address for given endpoint type.
    bool set_multicast_group(address::Interface iface, const char* ip);

    //! Bind peer to local endpoint.
    bool bind(address::Interface iface, address::EndpointURI& uri);

    //! Get receiver source.
    sndio::ISource& source();

private:
    struct InterfacePort {
        netio::UdpReceiverConfig config;
        netio::NetworkLoop::PortHandle handle;

        InterfacePort()
            : handle(NULL) {
        }
    };

    virtual void schedule_task_processing(pipeline::TaskPipeline&,
                                          core::nanoseconds_t delay);

    virtual void cancel_task_processing(pipeline::TaskPipeline&);

    core::Mutex mutex_;

    rtp::FormatMap format_map_;

    pipeline::ReceiverSource pipeline_;
    pipeline::ReceiverSource::EndpointSetHandle endpoint_set_;

    InterfacePort ports_[address::Iface_Max];

    ctl::ControlLoop::Tasks::ProcessPipelineTasks process_pipeline_tasks_;
};

} // namespace peer
} // namespace roc

#endif // ROC_PEER_RECEIVER_H_
