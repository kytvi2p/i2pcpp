#ifndef DATE_H
#define DATE_H

#include <chrono>

#include "Datatype.h"

namespace i2pcpp {
	class Date : public Datatype {
		public:
			Date();
			Date(ByteArray::const_iterator &dateItr);

			ByteArray getBytes() const;

		private:
			uint64_t m_value;
	};
}

#endif
