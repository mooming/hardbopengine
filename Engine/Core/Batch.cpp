// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Batch.h"

#include <type_traits>

#ifdef TEST_ENABLED
namespace hbe::pipeline
{
void BatchTest::Prepare()
{
	AddTest("Batch is an aggregate whose construction forms agree", [this](auto& ls)
	{
		static_assert(std::is_aggregate_v<Batch>, "Batch must stay an aggregate: no user-declared constructors");
		static_assert(std::is_trivially_copyable_v<Batch>, "A batch handle travels by plain copy through channels");
		static_assert(std::is_trivially_destructible_v<Batch>, "Ring slots are reused as raw bytes without destructors");
		static_assert(std::is_copy_constructible_v<Batch>, "A batch handle copy-constructs on every channel hop");

		const Batch defaultBatch;
		if (defaultBatch.data != nullptr || defaultBatch.count != 0 || defaultBatch.typeID != 0
			|| defaultBatch.requestID != 0)
		{
			ls << "A default-constructed batch must sit at its member initializers." << lferr;
		}

		alignas(Batch) std::byte items[64];
		const TypeID stampedTypeID = 7;
		const RequestID requestId = 42;

		const Batch ordered{stampedTypeID, requestId, items, 7};
		const Batch designated{.typeID = stampedTypeID, .requestID = requestId, .data = items, .count = 7};

		if (ordered.data != designated.data || ordered.count != designated.count
			|| ordered.typeID != designated.typeID || ordered.requestID != designated.requestID)
		{
			ls << "Ordered and designated aggregate construction must produce identical batches." << lferr;
		}

		const Batch partial{stampedTypeID};
		if (partial.typeID != stampedTypeID || partial.requestID != 0 || partial.data != nullptr
			|| partial.count != 0)
		{
			ls << "Omitted members of an aggregate batch must fall back to their member initializers." << lferr;
		}

		const Batch copy = designated;
		if (copy.data != designated.data || copy.count != designated.count
			|| copy.typeID != designated.typeID || copy.requestID != designated.requestID)
		{
			ls << "A batch handle must copy-construct: it travels by value on every channel hop." << lferr;
		}

		ls << "Batch is " << sizeof(Batch) << " bytes on a " << sizeof(void*) * 8 << "-bit pointer platform." << lf;

		if constexpr (sizeof(void*) == 8)
		{
			if (sizeof(Batch) != 32)
			{
				ls << "On a 64-bit platform a batch is " << sizeof(Batch) << " bytes, not the decided 32." << lferr;
			}
		}
	});
}
} // namespace hbe::pipeline
#endif // TEST_ENABLED
