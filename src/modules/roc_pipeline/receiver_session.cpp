/*
 * Copyright (c) 2017 Roc authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "roc_pipeline/receiver_session.h"
#include "roc_audio/resampler_map.h"
#include "roc_core/log.h"
#include "roc_core/panic.h"
#include "roc_fec/codec_map.h"

namespace roc {
namespace pipeline {

ReceiverSession::ReceiverSession(const ReceiverSessionConfig& session_config,
                                 const ReceiverCommonConfig& common_config,
                                 const address::SocketAddr& src_address,
                                 const rtp::FormatMap& format_map,
                                 packet::PacketPool& packet_pool,
                                 core::BufferPool<uint8_t>& byte_buffer_pool,
                                 core::BufferPool<audio::sample_t>& sample_buffer_pool,
                                 core::IAllocator& allocator)
    : src_address_(src_address)
    , allocator_(allocator)
    , audio_reader_(NULL) {
    const rtp::Format* format = format_map.format(session_config.payload_type);
    if (!format) {
        return;
    }

    queue_router_.reset(new (queue_router_) packet::Router(allocator_));
    if (!queue_router_) {
        return;
    }

    source_queue_.reset(new (source_queue_) packet::SortedQueue(0));
    if (!source_queue_) {
        return;
    }

    packet::IWriter* pwriter = source_queue_.get();

    if (!queue_router_->add_route(*pwriter, packet::Packet::FlagAudio)) {
        return;
    }

    packet::IReader* preader = source_queue_.get();

    delayed_reader_.reset(new (delayed_reader_) packet::DelayedReader(
        *preader, session_config.target_latency, format->sample_spec));
    if (!delayed_reader_) {
        return;
    }
    preader = delayed_reader_.get();

    validator_.reset(new (validator_) rtp::Validator(
        *preader, session_config.rtp_validator, format->sample_spec));
    if (!validator_) {
        return;
    }
    preader = validator_.get();

    if (session_config.fec_decoder.scheme != packet::FEC_None) {
        repair_queue_.reset(new (repair_queue_) packet::SortedQueue(0));
        if (!repair_queue_) {
            return;
        }
        if (!queue_router_->add_route(*repair_queue_, packet::Packet::FlagRepair)) {
            return;
        }

        fec_decoder_.reset(fec::CodecMap::instance().new_decoder(
                               session_config.fec_decoder, byte_buffer_pool, allocator_),
                           allocator_);
        if (!fec_decoder_) {
            return;
        }

        fec_parser_.reset(new (fec_parser_) rtp::Parser(format_map, NULL));
        if (!fec_parser_) {
            return;
        }

        fec_reader_.reset(new (fec_reader_) fec::Reader(
            session_config.fec_reader, session_config.fec_decoder.scheme, *fec_decoder_,
            *preader, *repair_queue_, *fec_parser_, packet_pool, allocator_));
        if (!fec_reader_ || !fec_reader_->valid()) {
            return;
        }
        preader = fec_reader_.get();

        fec_validator_.reset(new (fec_validator_) rtp::Validator(
            *preader, session_config.rtp_validator, format->sample_spec));
        if (!fec_validator_) {
            return;
        }
        preader = fec_validator_.get();
    }

    payload_decoder_.reset(format->new_decoder(allocator_), allocator_);
    if (!payload_decoder_) {
        return;
    }

    depacketizer_.reset(new (depacketizer_) audio::Depacketizer(
        *preader, *payload_decoder_,
        audio::SampleSpec(format->sample_spec.sample_rate(),
                          common_config.output_sample_spec.channel_mask()),
        common_config.beeping));
    if (!depacketizer_) {
        return;
    }

    audio::IReader* areader = depacketizer_.get();

    if (session_config.watchdog.no_playback_timeout != 0
        || session_config.watchdog.broken_playback_timeout != 0
        || session_config.watchdog.frame_status_window != 0) {
        watchdog_.reset(new (watchdog_) audio::Watchdog(
            *areader,
            audio::SampleSpec(format->sample_spec.sample_rate(),
                              common_config.output_sample_spec.channel_mask()),
            session_config.watchdog, allocator_));
        if (!watchdog_ || !watchdog_->valid()) {
            return;
        }
        areader = watchdog_.get();
    }

    if (common_config.resampling) {
        if (common_config.poisoning) {
            resampler_poisoner_.reset(new (resampler_poisoner_)
                                          audio::PoisonReader(*areader));
            if (!resampler_poisoner_) {
                return;
            }
            areader = resampler_poisoner_.get();
        }

        resampler_.reset(
            audio::ResamplerMap::instance().new_resampler(
                session_config.resampler_backend, allocator, sample_buffer_pool,
                session_config.resampler_profile, common_config.internal_frame_length,
                audio::SampleSpec(format->sample_spec.sample_rate(),
                                  common_config.output_sample_spec.channel_mask())),
            allocator);

        if (!resampler_) {
            return;
        }

        resampler_reader.reset(new (resampler_reader)
                                   audio::ResamplerReader(*areader, *resampler_));

        if (!resampler_reader || !resampler_reader->valid()) {
            return;
        }
        areader = resampler_reader.get();
    }

    if (common_config.poisoning) {
        session_poisoner_.reset(new (session_poisoner_) audio::PoisonReader(*areader));
        if (!session_poisoner_) {
            return;
        }
        areader = session_poisoner_.get();
    }

    latency_monitor_.reset(new (latency_monitor_) audio::LatencyMonitor(
        *source_queue_, *depacketizer_, resampler_reader.get(),
        session_config.latency_monitor, session_config.target_latency,
        format->sample_spec, common_config.output_sample_spec));
    if (!latency_monitor_ || !latency_monitor_->valid()) {
        return;
    }

    audio_reader_ = areader;
}

void ReceiverSession::destroy() {
    allocator_.destroy(*this);
}

bool ReceiverSession::valid() const {
    return audio_reader_;
}

bool ReceiverSession::handle(const packet::PacketPtr& packet) {
    roc_panic_if(!valid());

    packet::UDP* udp = packet->udp();
    if (!udp) {
        return false;
    }

    if (udp->src_addr != src_address_) {
        return false;
    }

    queue_router_->write(packet);
    return true;
}

bool ReceiverSession::update(packet::timestamp_t time) {
    roc_panic_if(!valid());

    if (watchdog_) {
        if (!watchdog_->update()) {
            return false;
        }
    }

    if (latency_monitor_) {
        if (!latency_monitor_->update(time)) {
            return false;
        }
    }

    return true;
}

audio::IReader& ReceiverSession::reader() {
    roc_panic_if(!valid());

    return *audio_reader_;
}

} // namespace pipeline
} // namespace roc
