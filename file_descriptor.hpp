#pragma once

class FileDescriptor {
public:
	FileDescriptor() = default;
	explicit FileDescriptor(int fd);

	~FileDescriptor();

	FileDescriptor(const FileDescriptor&) = delete;
	FileDescriptor& operator=(const FileDescriptor&) = delete;

	FileDescriptor(FileDescriptor&& other) noexcept;
	FileDescriptor& operator=(FileDescriptor&& other) noexcept;

	int get() const;
	int release();
	void reset(int fd = -1);
	bool valid() const;
private:
	int fd_ = -1;
};