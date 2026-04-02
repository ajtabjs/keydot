#include "mapped_file.h"
#ifdef _WIN32
#include "utils.h" // For DBG
#include <iostream>

MappedFile::MappedFile(const std::string& path) {
    m_hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (m_hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Error: Could not open file " << path << std::endl;
        return;
    }

    LARGE_INTEGER file_size_li;
    if (!GetFileSizeEx(m_hFile, &file_size_li)) {
        std::cerr << "Error: Could not get file size." << std::endl;
        return;
    }
    m_file_size = static_cast<size_t>(file_size_li.QuadPart);
    DBG("[IO] File size: ", m_file_size, " bytes");

    m_hMapping = CreateFileMapping(m_hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (m_hMapping == NULL) {
        std::cerr << "Error: Could not create file mapping." << std::endl;
        return;
    }

    m_pMappedData = MapViewOfFile(m_hMapping, FILE_MAP_READ, 0, 0, 0);
    if (m_pMappedData == NULL) {
        std::cerr << "Error: Could not map view of file." << std::endl;
        return;
    }

    DBG("[IO] Mapped view @ ", m_pMappedData, " size=", m_file_size, " bytes");
}

MappedFile::~MappedFile() {
    if (m_pMappedData) UnmapViewOfFile(m_pMappedData);
    if (m_hMapping) CloseHandle(m_hMapping);
    if (m_hFile != INVALID_HANDLE_VALUE) CloseHandle(m_hFile);
}

bool MappedFile::is_valid() const {
    return m_pMappedData != NULL;
}

std::span<const uint8_t> MappedFile::get_data() const {
    return { static_cast<const uint8_t*>(m_pMappedData), m_file_size };
}

#else // !_WIN32

#include <cstring>

MappedFile::MappedFile(const std::string& path) {
    m_fd = open(path.c_str(), O_RDONLY);
    if (m_fd == -1) {
        return;
    }

    struct stat st;
    if (fstat(m_fd, &st) == -1) {
        close(m_fd);
        m_fd = -1;
        return;
    }
    m_file_size = static_cast<size_t>(st.st_size);

    if (m_file_size == 0) {
        return;
    }

    m_data = mmap(nullptr, m_file_size, PROT_READ, MAP_PRIVATE, m_fd, 0);
    if (m_data == MAP_FAILED) {
        close(m_fd);
        m_fd = -1;
        m_data = nullptr;
        return;
    }

    madvise(m_data, m_file_size, MADV_SEQUENTIAL);
}

MappedFile::~MappedFile() {
    if (m_data && m_data != MAP_FAILED) {
        munmap(m_data, m_file_size);
    }
    if (m_fd != -1) {
        close(m_fd);
    }
}

bool MappedFile::is_valid() const {
    return m_data != nullptr && m_data != MAP_FAILED;
}

std::span<const uint8_t> MappedFile::get_data() const {
    return { static_cast<const uint8_t*>(m_data), m_file_size };
}

#endif // _WIN32