
#pragma once

namespace OS
{

class MapSyncMode final
{
public:
	int value;

	MapSyncMode() noexcept : value(0) {}

	void SetAsync() noexcept;
	void SetSync() noexcept;
	void Invalidate() noexcept;
};

} // namespace OS
