#ifndef _LINGX_CORE_FILE_H
#define _LINGX_CORE_FILE_H

#include <lingx/core/common.h>
#include <fcntl.h>

namespace lnx {

class File {
public:
    File(const File&) = delete;
    File& operator=(const File&) = delete;

    File(const char* path, int flags, mode_t mode = 0644);

    void set_log(const LogPtr& log) noexcept
    { log_ = log; }

    ssize_t read(void* buf, size_t len, off_t off) noexcept;

    int close() noexcept;

    ~File() { close(); }

private:
    int fd_ = -1;
    std::string path_;
    off_t offset_ = 0;

    LogPtr log_;
};

}

#endif
