/**
 * @file PacketHandler.cpp
 * @brief Implements PacketHandler.h
 */
#include "PacketHandler.h"
#include "Packet.h"
#include "EstablishmentState.h"
#include "Context.h"

namespace i2pcpp {
    namespace SSU {
        PacketHandler::PacketHandler(Context &c, SessionKey const &sk) :
            m_context(c),
            m_inboundKey(sk),
            m_imf(c),
            m_log(boost::log::keywords::channel = "PH") {}

        void PacketHandler::packetReceived(PacketPtr p)
        {
            I2P_LOG_SCOPED_TAG(m_log, "Endpoint", p->getEndpoint());

            auto ep = p->getEndpoint();

            std::lock_guard<std::mutex> lock(m_context.peers.getMutex());

            if(m_context.peers.peerExists(ep)) {
                handlePacket(p, m_context.peers.getPeer(ep));
            } else {
                EstablishmentStatePtr es = m_context.establishmentManager.getState(ep);
                if(es)
                    handlePacket(p, es);
                else
                    handlePacket(p);
            }
        }

        void PacketHandler::handlePacket(PacketPtr const &packet, PeerState const &state)
        {
            if(!packet->verify(state.getCurrentMacKey())) {
                I2P_LOG(m_log, error) << "packet verification failed";
                return;
            }

            m_context.peers.resetPeerTimer(state.getHash());

            packet->decrypt(state.getCurrentSessionKey());
            ByteArray &data = packet->getData();

            auto dataItr = data.cbegin();
            unsigned char flag = *(dataItr++);
            Packet::PayloadType ptype = (Packet::PayloadType)(flag >> 4);

            dataItr += 4; // TODO validate timestamp

            switch(ptype) {
                case Packet::PayloadType::DATA:
                    I2P_LOG(m_log, debug) << "data packet received";
                    m_imf.receiveData(state.getHash(), dataItr, data.cend());
                    break;

                case Packet::PayloadType::SESSION_DESTROY:
                    I2P_LOG(m_log, debug) << "received session destroy";
                    handleSessionDestroyed(state);
                    break;

                default:
                    break;
            }
        }

        void PacketHandler::handlePacket(PacketPtr const &packet, EstablishmentStatePtr const &state)
        {
            if(!packet->verify(state->getMacKey())) {
                I2P_LOG(m_log, error) << "packet verification failed";
                return;
            }

            ByteArray &data = packet->getData();
            if(state->getDirection() == EstablishmentState::Direction::OUTBOUND)
                state->setIV(data.begin() + 16, data.begin() + 32);

            packet->decrypt(state->getSessionKey());
            data = packet->getData();

            auto begin = data.cbegin();
            unsigned char flag = *(begin++);
            Packet::PayloadType ptype = (Packet::PayloadType)(flag >> 4);

            begin += 4; // TODO validate timestamp

            switch(ptype) {
                case Packet::PayloadType::SESSION_CREATED:
                    handleSessionCreated(begin, data.cend(), state);
                    break;

                case Packet::PayloadType::SESSION_CONFIRMED:
                    handleSessionConfirmed(begin, data.cend(), state);
                    break;

                case Packet::PayloadType::SESSION_DESTROY:
                    I2P_LOG(m_log, debug) << "received session destroy";
                    handleSessionDestroyed(state);
                    break;

                default:
                    break;
            }
        }

        void PacketHandler::handlePacket(PacketPtr &p)
        {
            Endpoint ep = p->getEndpoint();

            if(!p->verify(m_inboundKey)) {
                I2P_LOG(m_log, error) << "dropping new packet with invalid key";
                return;
            }

            p->decrypt(m_inboundKey);
            ByteArray &data = p->getData();

            auto dataItr = data.cbegin();
            auto end = data.cend();

            unsigned char flag = *(dataItr++);
            Packet::PayloadType ptype = (Packet::PayloadType)(flag >> 4);

            dataItr += 4; // TODO validate timestamp

            switch(ptype) {
                case Packet::PayloadType::SESSION_REQUEST:
                    handleSessionRequest(dataItr, end, m_context.establishmentManager.createState(ep));
                    break;

                default:
                    I2P_LOG(m_log, error) << "dropping new, out-of-state packet";
            }
        }

        void PacketHandler::handleSessionRequest(ByteArrayConstItr &begin, ByteArrayConstItr end, EstablishmentStatePtr const &state)
        {
            state->setTheirDH(begin, begin + 256), begin += 256;

            unsigned char ipSize = *(begin++);

            if(ipSize != 4 && ipSize != 16)
                return;

            ByteArray ip(begin, begin + ipSize);
            begin += ipSize;
            short port = parseUint16(begin);

            state->setMyEndpoint(Endpoint(ip, port));

            state->setRelayTag(0); // TODO Relay support

            state->setState(EstablishmentState::State::REQUEST_RECEIVED);
            m_context.establishmentManager.post(state);
        }

        void PacketHandler::handleSessionCreated(ByteArrayConstItr &begin, ByteArrayConstItr end, EstablishmentStatePtr const &state)
        {
            if(state->getState() != EstablishmentState::State::REQUEST_SENT)
                return;

            state->setTheirDH(begin, begin + 256), begin += 256;

            unsigned char ipSize = *(begin++);

            if(ipSize != 4 && ipSize != 16)
                return;

            ByteArray ip(begin, begin + ipSize);
            begin += ipSize;
            uint16_t port = parseUint16(begin);

            state->setMyEndpoint(Endpoint(ip, port));

            uint32_t relayTag = parseUint32(begin);
            state->setRelayTag(relayTag);

            uint32_t ts = parseUint32(begin);
            state->setSignatureTimestamp(ts);

            state->setSignature(begin, begin + 48);

            state->setState(EstablishmentState::State::CREATED_RECEIVED);
            m_context.establishmentManager.post(state);
        }

        void PacketHandler::handleSessionConfirmed(ByteArrayConstItr &begin, ByteArrayConstItr end, EstablishmentStatePtr const &state)
        {
            if(state->getState() != EstablishmentState::State::CREATED_SENT)
                return;

            unsigned char info = *(begin++);
            uint16_t size = parseUint16(begin);
            (void)info; (void)size; // Stop compiler from complaining

            RouterIdentity ri(begin, end);
            state->setTheirIdentity(ri);

            uint32_t ts = parseUint32(begin);
            state->setSignatureTimestamp(ts);

            state->setSignature(end - 40, end);

            state->setState(EstablishmentState::State::CONFIRMED_RECEIVED);
            m_context.establishmentManager.post(state);
        }

        void PacketHandler::handleSessionDestroyed(PeerState const &ps)
        {
            // m_peers is already locked above
            m_context.peers.delPeer(ps.getEndpoint());
            m_context.ios.post(boost::bind(boost::ref(m_context.disconnectedSignal), ps.getHash()));
        }

        void PacketHandler::handleSessionDestroyed(EstablishmentStatePtr const &state)
        {
            state->setState(EstablishmentState::State::FAILURE);
            m_context.establishmentManager.post(state);
        }
    }
}
