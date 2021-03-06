/*
 * Copyright (c) 2017 Roc authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//! @file roc_pipeline/receiver_endpoint.h
//! @brief Receiver endpoint pipeline.

#ifndef ROC_PIPELINE_RECEIVER_ENDPOINT_H_
#define ROC_PIPELINE_RECEIVER_ENDPOINT_H_

#include "roc_address/interface.h"
#include "roc_address/protocol.h"
#include "roc_core/iallocator.h"
#include "roc_core/mpsc_queue.h"
#include "roc_core/optional.h"
#include "roc_core/ref_counter.h"
#include "roc_core/scoped_ptr.h"
#include "roc_packet/iparser.h"
#include "roc_packet/iwriter.h"
#include "roc_pipeline/config.h"
#include "roc_pipeline/receiver_session_group.h"
#include "roc_pipeline/receiver_state.h"
#include "roc_rtp/format_map.h"
#include "roc_rtp/parser.h"

namespace roc {
namespace pipeline {

//! Receiver endpoint pipeline.
//! @remarks
//!  Created for every transport endpoint. Belongs to endpoint set.
//!  Passes packets to the session group of the endpoint set.
class ReceiverEndpoint : public packet::IWriter,
                         public core::RefCounter<ReceiverEndpoint>,
                         public core::ListNode {
public:
    //! Initialize.
    ReceiverEndpoint(address::Protocol proto,
                     ReceiverState& receiver_state,
                     ReceiverSessionGroup& session_group,
                     const rtp::FormatMap& format_map,
                     core::IAllocator& allocator);

    //! Check if the port pipeline was succefully constructed.
    bool valid() const;

    //! Get protocol.
    address::Protocol proto() const;

    //! Handle packet.
    //! Called outside of pipeline from any thread, typically from netio thread.
    virtual void write(const packet::PacketPtr& packet);

    //! Flush queued packets.
    //! Called from pipeline thread.
    void flush_packets();

private:
    friend class core::RefCounter<ReceiverEndpoint>;

    void destroy();

    const address::Protocol proto_;

    core::IAllocator& allocator_;

    ReceiverState& receiver_state_;
    ReceiverSessionGroup& session_group_;

    packet::IParser* parser_;

    core::Optional<rtp::Parser> rtp_parser_;
    core::ScopedPtr<packet::IParser> fec_parser_;

    core::MpscQueue<packet::Packet> queue_;
};

} // namespace pipeline
} // namespace roc

#endif // ROC_PIPELINE_RECEIVER_ENDPOINT_H_
