// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once


namespace OS
{
class MapSyncMode final
{
public:
	int value;

	MapSyncMode() noexcept
		: value(0)
	{
	}

	void SetAsync() noexcept;
	void SetSync() noexcept;
	void Invalidate() noexcept;
};
} // namespace OS
