/**
 * @file PacketHandler.h
 * @brief Defines the i2pcpp::SSU::PacketHandler class.
 */
#ifndef SSUPACKETHANDLER_H
#define SSUPACKETHANDLER_H

#include "InboundMessageFragments.h"

#include <i2pcpp/Log.h>

#include <i2pcpp/datatypes/SessionKey.h>

namespace i2pcpp {
    namespace SSU {
        class Context;
        class PeerState;
        class Packet; typedef std::shared_ptr<Packet> PacketPtr;
        class EstablishmentState; typedef std::shared_ptr<EstablishmentState> EstablishmentStatePtr;

        /**
         * Handles received i2pcpp::SSU::Packet objects.
         */
        class PacketHandler {
            friend class AcknowledgementManager;

            public:
                /**
                 * Constructs from a reference to the i2pcpp::SSU::UDPTransport
                 *  and an i2pcpp::SessionKey.
                 */
                PacketHandler(Context &c, SessionKey const &sk);
                PacketHandler(const PacketHandler &) = delete;
                PacketHandler& operator=(PacketHandler &) = delete;

                /**
                 * Called when a new i2pcpp::SSU::Packet has been received.
                 * @param p pointer to the received i2pcpp::SSU::Packet
                 */
                void packetReceived(PacketPtr p);

            private:
                /**
                 * Handles a newly received packet, after the session establishment.
                 * @param p the i2pcpp::SSU::PeerState associated with the peer
                 *  who sent this packet
                 * @todo better error handling?
                 */
                void handlePacket(PacketPtr const &packet, PeerState const &state);

                /**
                 * Handles a newly received packet, during session establishment.
                 * @param state pointer to the i2pcpp::SSU::EstablishmenState
                 *   associated with the peer who sent this packet
                 * @todo better error handling?
                 */
                void handlePacket(PacketPtr const &packet, EstablishmentStatePtr const &state);

                /**
                 * Handles a newly received packet, before the session establishment.
                 * @param p the i2pcpp::SSU::PeerState associated with the peer
                 *  who sent this packet
                 * @todo better error handling?
                 */
                void handlePacket(PacketPtr &p);

                /**
                 * Handles a session request packet.
                 * @param begin iterator to the begin of the payload
                 * @param end iterator to the end of the payload
                 * @param state pointer to the i2pcpp::SSU::EstablishmentState
                 *  associated with the peer who sent this packet
                 */
                void handleSessionRequest(ByteArrayConstItr &begin, ByteArrayConstItr end, EstablishmentStatePtr const &state);

                /**
                 * Handles a session created packet.
                 * @param begin iterator to the begin of the payload
                 * @param end iterator to the end of the payload
                 * @param state pointer to the i2pcpp::SSU::EstablishmentState
                 *  associated with the peer who sent this packet
                 */
                void handleSessionCreated(ByteArrayConstItr &begin, ByteArrayConstItr end, EstablishmentStatePtr const &state);

                /**
                 * Handles a session confirmed packet.
                 * @param begin iterator to the begin of the payload
                 * @param end iterator to the end of the payload
                 * @param state pointer to the i2pcpp::SSU::EstablishmentState
                 *  associated with the peer who sent this packet
                 */
                void handleSessionConfirmed(ByteArrayConstItr &begin, ByteArrayConstItr end, EstablishmentStatePtr const &state);

                /**
                 * Handles a session destroyed packet.
                 * @param ps the i2pcpp::SSU::PeerState associated with the peer
                 *  who sent this packet
                 */
                void handleSessionDestroyed(PeerState const &ps);

                /**
                 * Handles a session request packet.
                 * @param state pointer to the i2pcpp::SSU::EstablishmentState
                 *  associated with the peer who sent this packet
                 */
                void handleSessionDestroyed(EstablishmentStatePtr const &state);

                Context& m_context;

                SessionKey m_inboundKey;

                InboundMessageFragments m_imf;

                i2p_logger_mt m_log;
        };
    }
}

#endif
