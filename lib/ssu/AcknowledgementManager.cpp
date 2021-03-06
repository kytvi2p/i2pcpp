/**
 * @file AcknowledgmentManager.cpp
 * @brief Implements AcknowledgmentManager.h.
 */
#include "AcknowledgementManager.h"

#include "Context.h"
#include "Packet.h"

#include <boost/bind.hpp>

namespace i2pcpp {
    namespace SSU {
        AcknowledgementManager::AcknowledgementManager(Context &c) :
            m_context(c),
            m_timer(m_context.ios, boost::posix_time::time_duration(0, 0, 1)),
            m_log(boost::log::keywords::channel = "AM")
        {
            m_timer.async_wait(boost::bind(&AcknowledgementManager::flushAckCallback, this, boost::asio::placeholders::error));
        }

        void AcknowledgementManager::flushAckCallback(const boost::system::error_code& e)
        {
            std::lock_guard<std::mutex> lock(m_context.packetHandler.m_imf.m_mutex);
            auto& stateTable = m_context.packetHandler.m_imf.m_states;

            for(auto itr = stateTable.get<1>().cbegin(); itr != stateTable.get<1>().cend();) {
                std::lock_guard<std::mutex> lock(m_context.peers.getMutex());

                auto hashToAckFor = itr->hash;

                if(!m_context.peers.peerExists(hashToAckFor))
                    continue;

                CompleteAckList completeAckList;
                PartialAckList partialAckList;

                while(itr != stateTable.get<1>().cend() && itr->hash == hashToAckFor) {
                    I2P_LOG(m_log, debug) << "sending ack to " << hashToAckFor << " for msgId " << std::hex << itr->msgId << std::dec;

                    if(itr->state.allFragmentsReceived()) {
                        completeAckList.push_back(itr->msgId);
                        stateTable.get<1>().erase(itr++);
                    } else {
                        partialAckList[itr->msgId] = itr->state.getFragmentsReceived();
                        ++itr;
                    }
                }

                if(completeAckList.size() || partialAckList.size()) {
                    PeerState ps = m_context.peers.getPeer(hashToAckFor);

                    std::vector<PacketBuilder::FragmentPtr> emptyFragList;
                    PacketPtr p = PacketBuilder::buildData(ps.getEndpoint(), false, completeAckList, partialAckList, emptyFragList);
                    p->encrypt(ps.getCurrentSessionKey(), ps.getCurrentMacKey());
                    m_context.sendPacket(p);
                }
            }

            m_timer.expires_at(m_timer.expires_at() + boost::posix_time::time_duration(0, 0, 1));
            m_timer.async_wait(boost::bind(&AcknowledgementManager::flushAckCallback, this, boost::asio::placeholders::error));
        }
    }
}
