#include "file_descriptor.hpp"
#include <unistd.h>

FileDescriptor::FileDescriptor(int fd)
	: fd_(fd) {}

FileDescriptor::~FileDescriptor() {
	reset();
}

FileDescriptor::FileDescriptor(FileDescriptor&& other) noexcept
	: fd_(other.fd_) {
	other.fd_ = -1;
}

FileDescriptor& FileDescriptor::operator=(FileDescriptor&& other) noexcept {
	if (this != &other) {
		reset();
		fd_ = other.fd_;
		other.fd_ = -1;
	}

	return *this;
}

int FileDescriptor::get() const {
	return fd_;
}

int FileDescriptor::release() {
	int fd = fd_;
	fd_ = -1;
	return fd;
}

void FileDescriptor::reset(int fd) {
	if (fd_ != -1) {
		close(fd_);
	}

	fd_ = fd;
}

bool FileDescriptor::valid() const {
	return fd_ != -1;
}