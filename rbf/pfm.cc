#include "pfm.h"
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <stdio.h>

const int success = 0;
const int fail = -1;
PagedFileManager &PagedFileManager::instance()
{
    static PagedFileManager _pf_manager = PagedFileManager();
    return _pf_manager;
}

PagedFileManager::PagedFileManager() = default;

PagedFileManager::~PagedFileManager() = default;

PagedFileManager::PagedFileManager(const PagedFileManager &) = default;

PagedFileManager &PagedFileManager::operator=(const PagedFileManager &) = default;

RC PagedFileManager::createFile(const std::string &fileName)
{

    if (access(fileName.c_str(), F_OK) != -1)
    {
        return fail;
    }
    std::fstream file;
    file.open(fileName, std::ios::out);

    if (!file)
    {
        return fail;
    }

    file.close();
    return success;
}

RC PagedFileManager::destroyFile(const std::string &fileName)
{
    if (access(fileName.c_str(), F_OK) == -1)
    {
        return fail;
    }
    if (remove(fileName.c_str()) == 0)
        return success;
    else
        return fail;
}

RC PagedFileManager::openFile(const std::string &fileName, FileHandle &fileHandle)
{
    return -1;
}

RC PagedFileManager::closeFile(FileHandle &fileHandle)
{
    return -1;
}

FileHandle::FileHandle()
{
    readPageCounter = 0;
    writePageCounter = 0;
    appendPageCounter = 0;
}

FileHandle::~FileHandle() = default;

RC FileHandle::readPage(PageNum pageNum, void *data)
{
    return -1;
}

RC FileHandle::writePage(PageNum pageNum, const void *data)
{
    return -1;
}

RC FileHandle::appendPage(const void *data)
{
    return -1;
}

unsigned FileHandle::getNumberOfPages()
{
    return -1;
}

RC FileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount)
{
    return -1;
}