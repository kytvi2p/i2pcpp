/**
 * @file InboundMessageFragments.cpp
 * @brief Implements InboundMessageFragments.h.
 */
#include "InboundMessageFragments.h"

#include <string>
#include <bitset>
#include <iomanip>

#include <botan/pipe.h>
#include <botan/filters.h>

#include "../../exceptions/FormattingError.h"
#include "../../util/make_unique.h"

#include "../UDPTransport.h"

#include "InboundMessageState.h"

namespace i2pcpp {
    namespace SSU {
        InboundMessageFragments::InboundMessageFragments(UDPTransport &transport) :
            m_transport(transport),
            m_log(boost::log::keywords::channel = "IMF") {}

        void InboundMessageFragments::receiveData(RouterHash const &rh, ByteArrayConstItr &begin, ByteArrayConstItr end)
        {
            I2P_LOG_SCOPED_TAG(m_log, "RouterHash", rh);

            if((end - begin) < 1) throw FormattingError();
            std::bitset<8> flag = *(begin++);

            if(flag[7]) {
                if((end - begin) < 1) throw FormattingError();
                unsigned char numAcks = *(begin++);
                if((end - begin) < (numAcks * 4)) throw FormattingError();

                while(numAcks--) {
                    uint32_t msgId = (begin[0] << 24) | (begin[1] << 16) | (begin[2] << 8) | (begin[3]);
                    begin += 4;

                    std::lock_guard<std::mutex> lock(m_transport.m_omf.m_mutex);
                    m_transport.m_omf.delState(msgId);
                }
            }

            if(flag[6]) {

                unsigned char numFields = *(begin++);
                while(numFields--) {
                    // Read 4 byte msgId
                    uint32_t msgId = (begin[0] << 24) | (begin[1] << 16) | (begin[2] << 8) | (begin[3]);
                    begin += 4;

                    // Read ACK bitfield (1 byte)
                    std::lock_guard<std::mutex> lock(m_transport.m_omf.m_mutex);
                    auto itr = m_transport.m_omf.m_states.find(msgId);
                    uint8_t byteNum = 0;
                    do {
                        uint8_t byte = *begin;
                        for(int i = 6, j = 0; i >= 0; i--, j++) {
                            // If the bit is 1, the fragment has been received
                            if(byte & (1 << i)) { 
                                if(itr != m_transport.m_omf.m_states.end())
                                    itr->second.markFragmentAckd((byteNum * 7) + j);
                            }
                        }

                        ++byteNum;
                    // If the low bit is 1, another bitfield follows
                    } while(*(begin++) & (1 << 7));

                    if(itr != m_transport.m_omf.m_states.end() && itr->second.allFragmentsAckd())
                        m_transport.m_omf.delState(msgId);
                }
            }

            if((end - begin) < 1) throw FormattingError();
            unsigned char numFragments = *(begin++);
            I2P_LOG(m_log, debug) << "number of fragments: " << std::to_string(numFragments);

            for(int i = 0; i < numFragments; i++) {
                if((end - begin) < 7) throw FormattingError();
                uint32_t msgId = (begin[0] << 24) | (begin[1] << 16) | (begin[2] << 8) | (begin[3]);
                begin += 4;
                I2P_LOG(m_log, debug) << "fragment[" << i << "] message id: " << std::hex << msgId << std::dec;

                uint32_t fragInfo = (begin[0] << 16) | (begin[1] << 8) | (begin[2]);
                begin += 3;

                uint16_t fragNum = fragInfo >> 17;
                I2P_LOG(m_log, debug) << "fragment[" << i << "] fragment #: " << fragNum;

                bool isLast = (fragInfo & 0x010000);
                I2P_LOG(m_log, debug) << "fragment[" << i << "] isLast: " << isLast;

                uint16_t fragSize = fragInfo & ((1 << 14) - 1);
                I2P_LOG(m_log, debug) << "fragment[" << i << "] size: " << fragSize;

                if((end - begin) < fragSize) throw FormattingError();
                ByteArray fragData(begin, begin + fragSize);
                I2P_LOG(m_log, debug) << "fragment[" << i << "] data: " << fragData;

                std::lock_guard<std::mutex> lock(m_mutex);
                auto itr = m_states.get<0>().find(msgId);
                if(itr == m_states.get<0>().end()) {
                    InboundMessageState ims(rh, msgId);
                    ims.addFragment(fragNum, fragData, isLast);

                    checkAndPost(msgId, ims);
                    addState(msgId, rh, std::move(ims));
                } else {
                    m_states.get<0>().modify(itr, AddFragment(fragNum, fragData, isLast));

                    checkAndPost(msgId, itr->state);
                }
            }
        }

        void InboundMessageFragments::addState(const uint32_t msgId, const RouterHash &rh, InboundMessageState ims)
        {
            auto timer = std::make_shared<boost::asio::deadline_timer>(m_transport.m_ios, boost::posix_time::time_duration(0, 0, 10));
            timer->async_wait(boost::bind(&InboundMessageFragments::timerCallback, this, boost::asio::placeholders::error, msgId));

            ContainerEntry sc(ims, timer);

            sc.msgId = msgId;
            sc.hash = rh;

            m_states.insert(std::move(sc));
        }

        void InboundMessageFragments::delState(const uint32_t msgId)
        {
            m_states.get<0>().erase(msgId);
        }

        void InboundMessageFragments::timerCallback(const boost::system::error_code& e, const uint32_t msgId)
        {
            if(!e) {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_states.get<0>().erase(msgId);
            }
        }

        inline void InboundMessageFragments::checkAndPost(const uint32_t msgId, InboundMessageState const &ims)
        {
            if(ims.allFragmentsReceived()) {
                const ByteArray& data = ims.assemble();
                if(data.size())
                    m_transport.m_ios.post(boost::bind(boost::ref(m_transport.m_receivedSignal), ims.getRouterHash(), msgId, data));
            }
        }

        InboundMessageFragments::ContainerEntry::ContainerEntry(InboundMessageState ims, std::shared_ptr<boost::asio::deadline_timer>t) :
            state(std::move(ims)),
            timer(std::move(t)) {}

        InboundMessageFragments::AddFragment::AddFragment(const uint8_t fragNum, ByteArray const &data, bool isLast) :
            m_fragNum(fragNum),
            m_data(data),
            m_isLast(isLast) {}

        void InboundMessageFragments::AddFragment::operator()(ContainerEntry &ce)
        {
            ce.state.addFragment(m_fragNum, m_data, m_isLast);
        }
    }
}
