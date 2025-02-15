#include "storage/disk_manager.h"

#include <assert.h>    // for assert
#include <string.h>    // for memset
#include <sys/stat.h>  // for stat
#include <unistd.h>    // for lseek

#include "defs.h"

DiskManager::DiskManager() { memset(fd2pageno_, 0, MAX_FD * (sizeof(std::atomic<page_id_t>) / sizeof(char))); }

/**
 * @brief Write the contents of the specified page into disk file
 *
 */
void DiskManager::write_page(int fd, page_id_t page_no, const char *offset, int num_bytes) {
    // Todo:
    // 1.lseek()定位到文件头，通过(fd,page_no)可以定位指定页面及其在磁盘文件中的偏移量
    // 2.调用write()函数
    // 注意处理异常
    if(fd >= MAX_FD) {
        throw UnixError();
    }
    off_t offs = lseek(fd, page_no * PAGE_SIZE, SEEK_SET);
    if(offs == -1) {
        // std::cout << "[DiskManager] write_page: lseek error" << std::endl;
        throw UnixError();
    }
    // std::cout << "[DiskManager] write_page: offset find and seek to" << offs << std::endl;
    write(fd, offset, num_bytes);
}

/**
 * @brief Read the contents of the specified page into the given memory area
 */
void DiskManager::read_page(int fd, page_id_t page_no, char *offset, int num_bytes) {
    // Todo:
    // 1.lseek()定位到文件头，通过(fd,page_no)可以定位指定页面及其在磁盘文件中的偏移量
    // 2.调用read()函数
    // 注意处理异常
    off_t offs = lseek(fd, page_no * PAGE_SIZE, SEEK_SET);
    if(offs == -1) {
        // std::cout << "[DiskManager] read_page: lseek error" << std::endl;
        throw UnixError();
    }
    // std::cout << "[DiskManager] read_page: offset find and seek to" << offs << std::endl;
    ssize_t res = read(fd, offset, num_bytes);
    // 0 is EOF, -1 is error
    if (res == -1) {
        // std::cout << "[DiskManager] read_page: read error" << std::endl;
        throw UnixError();
    }

}

/**
 * @brief Allocate new page (operations like create index/table)
 * For now just keep an increasing counter
 */
page_id_t DiskManager::AllocatePage(int fd) {
    // Todo:
    // 简单的自增分配策略，指定文件的页面编号加1
    if(fd >= MAX_FD) {
        throw UnixError();
    }
    return fd2pageno_[fd]++;
}

/**
 * @brief Deallocate page (operations like drop index/table)
 * Need bitmap in header page for tracking pages
 * This does not actually need to do anything for now.
 */
void DiskManager::DeallocatePage(__attribute__((unused)) page_id_t page_id) {}

bool DiskManager::is_dir(const std::string &path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

void DiskManager::create_dir(const std::string &path) {
    // Create a subdirectory
    std::string cmd = "mkdir " + path;
    if (system(cmd.c_str()) < 0) {  // 创建一个名为path的目录
        throw UnixError();
    }
}

void DiskManager::destroy_dir(const std::string &path) {
    std::string cmd = "rm -r " + path;
    if (system(cmd.c_str()) < 0) {
        throw UnixError();
    }
}

/**
 * @brief 用于判断指定路径文件是否存在 
 */
bool DiskManager::is_file(const std::string &path) {
    // Todo:
    // 用struct stat获取文件信息
    struct stat buf;
    int err = stat(path.c_str(), &buf);
    if(err != 0) return false;
    if(S_ISREG(buf.st_mode)) return true;
    return false;
}

/**
 * @brief 用于创建指定路径文件
 */
void DiskManager::create_file(const std::string &path) {
    // Todo:
    // 调用open()函数，使用O_CREAT模式
    // 注意不能重复创建相同文件
    if (is_file(path) == true) {
        throw FileExistsError(path);
    }
    int err = open(path.c_str(), O_CREAT, 0666); // 0666: rw-rw-rw-
    if(err == -1) {
        throw UnixError();
    }
}

/**
 * @brief 用于删除指定路径文件 
 */
void DiskManager::destroy_file(const std::string &path) {
    // Todo:
    // 调用unlink()函数
    // 注意不能删除未关闭的文件

    if(!is_file(path)) {
        throw FileNotFoundError(path);
    }
    //is open
    if(path2fd_.count(path)) {
        throw UnixError();
    }
    int err = unlink(path.c_str());
    if(err == -1) {
        throw UnixError();
    }
    
}

/**
 * @brief 用于打开指定路径文件
 */
int DiskManager::open_file(const std::string &path) {
    // Todo:
    // 调用open()函数，使用O_RDWR模式
    // 注意不能重复打开相同文件，并且需要更新文件打开列表
    if (!is_file(path)) {
        throw FileNotFoundError(path);
    }
    //is open
    if(path2fd_.count(path)) {
        return path2fd_[path];
    }
    int fd = open(path.c_str(), O_RDWR);
    if(fd == -1 || fd >= MAX_FD) {
        throw UnixError();
    }
    path2fd_[path] = fd;
    fd2path_[fd] = path;
    return fd;
}

/**
 * @brief 用于关闭指定路径文件
 */
void DiskManager::close_file(int fd) {
    // Todo:
    // 调用close()函数
    // 注意不能关闭未打开的文件，并且需要更新文件打开列表

    if(fd2path_.count(fd) && fd < MAX_FD) {
        int err = close(fd);
        if(err == -1) {
            throw UnixError();
        }
        path2fd_.erase(fd2path_[fd]);
        fd2path_.erase(fd);
    }
    

}

int DiskManager::GetFileSize(const std::string &file_name) {
    struct stat stat_buf;
    int rc = stat(file_name.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

std::string DiskManager::GetFileName(int fd) {
    if (!fd2path_.count(fd)) {
        throw FileNotOpenError(fd);
    }
    return fd2path_[fd];
}

int DiskManager::GetFileFd(const std::string &file_name) {
    if (!path2fd_.count(file_name)) {
        return open_file(file_name);
    }
    return path2fd_[file_name];
}

bool DiskManager::ReadLog(char *log_data, int size, int offset, int prev_log_end) {
    // read log file from the previous end
    if (log_fd_ == -1) {
        log_fd_ = open_file(LOG_FILE_NAME);
    }
    offset += prev_log_end;
    int file_size = GetFileSize(LOG_FILE_NAME);
    if (offset >= file_size) {
        return false;
    }

    size = std::min(size, file_size - offset);
    lseek(log_fd_, offset, SEEK_SET);
    ssize_t bytes_read = read(log_fd_, log_data, size);
    if (bytes_read != size) {
        throw UnixError();
    }
    return true;
}

void DiskManager::WriteLog(char *log_data, int size) {
    if (log_fd_ == -1) {
        log_fd_ = open_file(LOG_FILE_NAME);
    }

    // write from the file_end
    lseek(log_fd_, 0, SEEK_END);
    ssize_t bytes_write = write(log_fd_, log_data, size);
    if (bytes_write != size) {
        throw UnixError();
    }
}
