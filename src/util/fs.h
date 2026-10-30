// Copyright (c) 2017-2020 The Bitcoin Core developers
// Copyright (c) 2020-2021 The PIVX Core developers
// Copyright (c) 2021 The DECENOMY Core Developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_FS_H
#define SCHILLINGCOIN_FS_H

#include <stdio.h>
#include <string>

#if defined WIN32 && defined __GLIBCXX__
#include <ext/stdio_filebuf.h>
#endif

#define BOOST_FILESYSTEM_NO_DEPRECATED
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

/** Filesystem operations and types */
namespace fs = boost::filesystem;

/** Bridge operations to C stdio */
namespace fsbridge {

FILE* fopen(const fs::path& p, const char* mode);

/* SCH compatibility helper used by logging.cpp */
FILE* freopen(const fs::path& p, const char* mode, FILE* stream);

class FileLock
{
public:
    FileLock() = delete;
    FileLock(const FileLock&) = delete;
    FileLock(FileLock&&) = delete;

    explicit FileLock(const fs::path& file);
    ~FileLock();

    bool TryLock();

    std::string GetReason()
    {
        return reason;
    }

private:
    std::string reason;

#ifndef WIN32
    int fd = -1;
#else
    void* hFile = (void*)-1; // INVALID_HANDLE_VALUE
#endif
};

std::string get_filesystem_error_message(const fs::filesystem_error& e);

#if defined WIN32 && defined __GLIBCXX__

class ifstream : public std::istream
{
public:
    ifstream() = default;

    explicit ifstream(
        const fs::path& p,
        std::ios_base::openmode mode = std::ios_base::in)
    {
        open(p, mode);
    }

    ~ifstream()
    {
        close();
    }

    void open(
        const fs::path& p,
        std::ios_base::openmode mode = std::ios_base::in);

    bool is_open()
    {
        return m_filebuf.is_open();
    }

    void close();

private:
    __gnu_cxx::stdio_filebuf<char> m_filebuf;
    FILE* m_file = nullptr;
};

class ofstream : public std::ostream
{
public:
    ofstream() = default;

    explicit ofstream(
        const fs::path& p,
        std::ios_base::openmode mode = std::ios_base::out)
    {
        open(p, mode);
    }

    ~ofstream()
    {
        close();
    }

    void open(
        const fs::path& p,
        std::ios_base::openmode mode = std::ios_base::out);

    bool is_open()
    {
        return m_filebuf.is_open();
    }

    void close();

private:
    __gnu_cxx::stdio_filebuf<char> m_filebuf;
    FILE* m_file = nullptr;
};

#else

typedef fs::ifstream ifstream;
typedef fs::ofstream ofstream;

#endif

} // namespace fsbridge

#endif // SCHILLINGCOIN_FS_H