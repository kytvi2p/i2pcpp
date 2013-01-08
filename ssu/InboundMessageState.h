#ifndef SSUINBOUNDMESSAGESTATE_H
#define SSUINBOUNDMESSAGESTATE_H

#include <memory>
#include <map>
#include <mutex>

#include "../datatypes/ByteArray.h"
#include "../datatypes/RouterHash.h"

namespace i2pcpp {
	namespace SSU {
		class InboundMessageState {
			public:
				InboundMessageState(RouterHash const &routerHash, const uint32_t msgId) : m_msgId(msgId), m_routerHash(routerHash), m_gotLast(false), m_byteTotal(0) {}

				void addFragment(const unsigned char fragNum, ByteArray const &data, bool isLast);
				ByteArray assemble() const;

				uint32_t getMsgId() const { return m_msgId; }
				const RouterHash& getRouterHash() const { return m_routerHash; }
				unsigned char getNumFragments() const { return m_fragments.size(); }

				explicit operator bool() const;

			private:
				uint32_t m_msgId;
				RouterHash m_routerHash;
				bool m_gotLast;
				unsigned char m_lastFragment;
				uint32_t m_byteTotal;

				std::map<unsigned char, ByteArray> m_fragments;
		};

		typedef std::shared_ptr<InboundMessageState> InboundMessageStatePtr;
	}
}

#endif
