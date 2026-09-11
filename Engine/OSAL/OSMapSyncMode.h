
#pragma once

namespace OS
{

class MapSyncMode final
{
public:
	int value;

	MapSyncMode() noexcept : value(0) {}

	void setAsync() noexcept;
	void setSync() noexcept;
	void invalidate() noexcept;
};

} // namespace OS
