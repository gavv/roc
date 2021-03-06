/*
 * Copyright (c) 2015 Roc authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//! @file roc_netio/target_libuv/roc_netio/network_loop.h
//! @brief Network event loop thread.

#ifndef ROC_NETIO_NETWORK_LOOP_H_
#define ROC_NETIO_NETWORK_LOOP_H_

#include <uv.h>

#include "roc_address/socket_addr.h"
#include "roc_core/atomic.h"
#include "roc_core/buffer_pool.h"
#include "roc_core/iallocator.h"
#include "roc_core/list.h"
#include "roc_core/mpsc_queue.h"
#include "roc_core/mpsc_queue_node.h"
#include "roc_core/optional.h"
#include "roc_core/semaphore.h"
#include "roc_core/thread.h"
#include "roc_netio/basic_port.h"
#include "roc_netio/iclose_handler.h"
#include "roc_netio/iconn.h"
#include "roc_netio/iconn_acceptor.h"
#include "roc_netio/iconn_handler.h"
#include "roc_netio/iterminate_handler.h"
#include "roc_netio/resolver.h"
#include "roc_netio/tcp_connection_port.h"
#include "roc_netio/tcp_server_port.h"
#include "roc_netio/udp_receiver_port.h"
#include "roc_netio/udp_sender_port.h"
#include "roc_packet/iwriter.h"
#include "roc_packet/packet_pool.h"

namespace roc {
namespace netio {

//! Network event loop thread.
class NetworkLoop : private ITerminateHandler,
                    private ICloseHandler,
                    private IResolverRequestHandler,
                    private core::Thread {
public:
    class ICompletionHandler;

    //! Opaque port handle.
    typedef struct PortHandle* PortHandle;

    //! Base task class.
    //! The user is responsible for allocating and deallocating the task.
    class Task : public core::MpscQueueNode {
    public:
        ~Task();

        //! Check that the task finished and succeeded.
        bool success() const;

    protected:
        friend class NetworkLoop;

        Task();

        //! Task state.
        enum State { Initialized, Pending, ClosingPort, Finishing, Finished };

        void (NetworkLoop::*func_)(Task&); //!< Task implementation method.

        //! Task state, defines whether task is finished already.
        //! The task becomes immutable after setting state to Finished.
        core::Atomic<int> state_;

        //! Task result, defines wether finished task succeeded or failed.
        //! Makes sense only after setting state_ to Finished.
        //! This atomic should be assigned before setting state_ to Finished.
        core::Atomic<int> success_;

        core::SharedPtr<BasicPort> port_; //!< On which port the task operates.
        PortHandle port_handle_;          //!< Port handle.

        ICompletionHandler* handler_;         //!< Completion handler.
        core::Optional<core::Semaphore> sem_; //!< Completion semaphore.
    };

    //! Subclasses for specific tasks.
    class Tasks {
    public:
        //! Add UDP datagram receiver port.
        class AddUdpReceiverPort : public Task {
        public:
            //! Set task parameters.
            //! @remarks
            //!  - Updates @p config with the actual bind address.
            //!  - Passes received packets to @p writer. It is called from network thread.
            //!    It should not block the caller.
            AddUdpReceiverPort(UdpReceiverConfig& config, packet::IWriter& writer);

            //! Get created port handle.
            //! @pre
            //!  Should be called only if success() is true.
            PortHandle get_handle() const;

        private:
            friend class NetworkLoop;

            UdpReceiverConfig* config_;
            packet::IWriter* writer_;
        };

        //! Add UDP datagram sender port.
        class AddUdpSenderPort : public Task {
        public:
            //! Set task parameters.
            //! @remarks
            //!  Updates @p config with the actual bind address.
            AddUdpSenderPort(UdpSenderConfig& config);

            //! Get created port handle.
            //! @pre
            //!  Should be called only if success() is true.
            PortHandle get_handle() const;

            //! Get created port writer;
            //! @remarks
            //!  The writer can be used to send packets from the port. It may be called
            //!  from any thread. It will not block the caller.
            //! @pre
            //!  Should be called only if success() is true.
            packet::IWriter* get_writer() const;

        private:
            friend class NetworkLoop;

            UdpSenderConfig* config_;
            packet::IWriter* writer_;
        };

        //! Add TCP server port.
        class AddTcpServerPort : public Task {
        public:
            //! Set task parameters.
            //! @remarks
            //!  - Updates @p config with the actual bind address.
            //!  - Listens for incoming connections and passes new connections
            //!    to @p conn_acceptor. It should return handler that will be
            //!    notified when connection state changes.
            AddTcpServerPort(TcpServerConfig& config, IConnAcceptor& conn_acceptor);

            //! Get created port handle.
            //! @pre
            //!  Should be called only if success() is true.
            PortHandle get_handle() const;

        private:
            friend class NetworkLoop;

            TcpServerConfig* config_;
            IConnAcceptor* conn_acceptor_;
        };

        //! Add TCP client port.
        class AddTcpClientPort : public Task {
        public:
            //! Set task parameters.
            //! @remarks
            //!  - Updates @p config with the actual bind address.
            //!  - Notofies @p conn_handler when connection state changes.
            AddTcpClientPort(TcpClientConfig& config, IConnHandler& conn_handler);

            //! Get created port handle.
            //! @pre
            //!  Should be called only if success() is true.
            PortHandle get_handle() const;

        private:
            friend class NetworkLoop;

            TcpClientConfig* config_;
            IConnHandler* conn_handler_;
        };

        //! Remove port.
        class RemovePort : public Task {
        public:
            //! Set task parameters.
            RemovePort(PortHandle handle);

        private:
            friend class NetworkLoop;
        };

        //! Resolve endpoint address.
        class ResolveEndpointAddress : public Task {
        public:
            //! Set task parameters.
            //! @remarks
            //!  Gets endpoint hostname, resolves it, and writes the resolved IP address
            //!  and the port from the endpoint to the resulting SocketAddr.
            ResolveEndpointAddress(const address::EndpointURI& endpoint_uri);

            //! Get resolved address.
            //! @pre
            //!  Should be called only if success() is true.
            const address::SocketAddr& get_address() const;

        private:
            friend class NetworkLoop;

            ResolverRequest resolve_req_;
        };
    };

    //! Task completion handler.
    class ICompletionHandler {
    public:
        virtual ~ICompletionHandler();

        //! Called when a task is finished.
        virtual void network_task_finished(Task&) = 0;
    };

    //! Initialize.
    //! @remarks
    //!  Start background thread if the object was successfully constructed.
    NetworkLoop(packet::PacketPool& packet_pool,
                core::BufferPool<uint8_t>& buffer_pool,
                core::IAllocator& allocator);

    //! Destroy. Stop all receivers and senders.
    //! @remarks
    //!  Wait until background thread finishes.
    virtual ~NetworkLoop();

    //! Check if the object was successfully constructed.
    bool valid() const;

    //! Get number of receiver and sender ports.
    size_t num_ports() const;

    //! Enqueue a task for asynchronous execution and return.
    //! The task should not be destroyed until the callback is called.
    //! The @p callback will be invoked on event loop thread after the
    //! task completes. The given @p cb_arg is passed to the callback.
    //! The callback should not block the caller.
    void schedule(Task& task, ICompletionHandler& handler);

    //! Enqueue a task for asynchronous execution and wait for its completion.
    //! The task should not be destroyed until this method returns.
    //! Should not be called from schedule() callback.
    //! @returns
    //!  true if the task succeeded or false if it failed.
    bool schedule_and_wait(Task& task);

private:
    static void task_sem_cb_(uv_async_t* handle);
    static void stop_sem_cb_(uv_async_t* handle);

    virtual void handle_terminate_completed(IConn&, void*);
    virtual void handle_close_completed(BasicPort&, void*);
    virtual void handle_resolved(ResolverRequest& req);

    virtual void run();

    void process_pending_tasks_();
    void finish_task_(Task&);

    void async_terminate_conn_port_(const core::SharedPtr<TcpConnectionPort>& port,
                                    Task* task);
    AsyncOperationStatus async_close_port_(const core::SharedPtr<BasicPort>& port,
                                           Task* task);
    void finish_closing_port_(const core::SharedPtr<BasicPort>& port, Task* task);

    void update_num_ports_();

    void close_all_sems_();
    void close_all_ports_();

    void task_add_udp_receiver_(Task&);
    void task_add_udp_sender_(Task&);
    void task_remove_port_(Task&);
    void task_add_tcp_server_(Task&);
    void task_add_tcp_client_(Task&);
    void task_resolve_endpoint_address_(Task&);

    packet::PacketPool& packet_pool_;
    core::BufferPool<uint8_t>& buffer_pool_;
    core::IAllocator& allocator_;

    bool started_;

    uv_loop_t loop_;
    bool loop_initialized_;

    uv_async_t stop_sem_;
    bool stop_sem_initialized_;

    uv_async_t task_sem_;
    bool task_sem_initialized_;

    core::MpscQueue<Task, core::NoOwnership> pending_tasks_;

    Resolver resolver_;

    core::List<BasicPort> open_ports_;
    core::List<BasicPort> closing_ports_;

    core::Atomic<int> num_open_ports_;
};

} // namespace netio
} // namespace roc

#endif // ROC_NETIO_NETWORK_LOOP_H_
