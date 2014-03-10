#ifndef TUNNELMANAGER_H
#define TUNNELMANAGER_H

#include "Tunnel.h"
#include "FragmentHandler.h"

#include "../Log.h"

#include <i2pcpp/datatypes/BuildRecord.h>
#include <i2pcpp/datatypes/BuildRequestRecord.h>
#include <i2pcpp/datatypes/BuildResponseRecord.h>

#include <boost/asio.hpp>

#include <mutex>
#include <unordered_map>

namespace i2pcpp {
    class RouterContext;

    class TunnelManager {
        public:
            TunnelManager(boost::asio::io_service &ios, RouterContext &ctx);
            TunnelManager(const TunnelManager &) = delete;
            TunnelManager& operator=(TunnelManager &) = delete;

            void begin();
            void receiveRecords(uint32_t const msgId, std::list<BuildRecordPtr> records);
            void receiveGatewayData(RouterHash const from, uint32_t const tunnelId, ByteArray const data);
            void receiveData(RouterHash const from, uint32_t const tunnelId, StaticByteArray<1024> const data);

        private:
            void timerCallback(const boost::system::error_code &e, bool participating, uint32_t tunnelId);
            void callback(const boost::system::error_code &e);
            void createTunnel();

            boost::asio::io_service &m_ios;
            RouterContext &m_ctx;

            std::unordered_map<uint32_t, TunnelPtr> m_pending;
            std::unordered_map<uint32_t, TunnelPtr> m_tunnels;
            std::unordered_map<uint32_t, std::pair<BuildRequestRecordPtr, std::unique_ptr<boost::asio::deadline_timer>>> m_participating;

            mutable std::mutex m_pendingMutex;
            mutable std::mutex m_tunnelsMutex;
            mutable std::mutex m_participatingMutex;

            FragmentHandler m_fragmentHandler;

            boost::asio::deadline_timer m_timer;

            i2p_logger_mt m_log;
    };
}

#endif