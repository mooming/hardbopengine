
#pragma once

namespace OS
{

class FileOpenMode final
{
public:
	int value;

	FileOpenMode() noexcept : value(0) {}

	void setReadOnly() noexcept;
	void setWriteOnly() noexcept;
	void setReadWrite() noexcept;
	void setCreate() noexcept;
	void setTruncate() noexcept;
	void setAppend() noexcept;
};

} // namespace OS
