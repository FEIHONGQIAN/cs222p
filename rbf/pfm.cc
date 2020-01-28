#include "pfm.h"
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

const int success = 0;
const int fail = -1;
const int origin = 0;
const int counterCount = 3;
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

    FILE *pFile;
    pFile = fopen(fileName.c_str(), "r");
    if (pFile)
    {
        fclose(pFile);
        return fail;
    }
    pFile = fopen(fileName.c_str(), "w");
    if (!pFile)
    {
        return fail;
    }

    void *hiddenPageData = malloc(PAGE_SIZE);
    int offset = 0;
    *(int *)((char *)hiddenPageData + offset) = 0;
    offset += sizeof(int);
    *(int *)((char *)hiddenPageData + offset) = 1; //Create the file will write counter to the hidden page, hence write counter starts with 1
    offset += sizeof(int);
    *(int *)((char *)hiddenPageData + offset) = 0;

    if (fwrite(hiddenPageData, 1, PAGE_SIZE, pFile) != PAGE_SIZE)
    {
        free(hiddenPageData);
        return fail;
    }
    fflush(pFile);
    fclose(pFile);
    free(hiddenPageData);

    return success;
}

RC PagedFileManager::destroyFile(const std::string &fileName)
{
    if (access(fileName.c_str(), F_OK) == -1)
    {
        return fail;
    }
    if (remove(fileName.c_str()) == 0)
    {
        return success;
    }
    else
    {
        return fail;
    }
}

RC PagedFileManager::openFile(const std::string &fileName, FileHandle &fileHandle)
{
    FILE *file;
    file = fopen(fileName.c_str(), "r++");
    if (!file)
    {
        return fail;
    }

    if (fileHandle.filePointer)
    {
        return fail;
    }

    void *hiddenPageData = malloc(PAGE_SIZE);
    int size = fread(hiddenPageData, sizeof(int), counterCount, file);
    if (size != counterCount)
    {
        free(hiddenPageData);
        return fail;
    }

    int offset = 0;
    *(int *)((char *)hiddenPageData + offset) += 1; // Open the file will read the hidden once, hence increase the counter
    fileHandle.readPageCounter = *(int *)((char *)hiddenPageData + offset);
    offset += sizeof(int);
    fileHandle.writePageCounter = *(int *)((char *)hiddenPageData + offset);
    offset += sizeof(int);
    fileHandle.appendPageCounter = *(int *)((char *)hiddenPageData + offset);

    fileHandle.filePointer = file;

    free(hiddenPageData);

    return success;
}

RC PagedFileManager::closeFile(FileHandle &fileHandle)
{
    FILE *file = fileHandle.filePointer;
    if (!file)
    {
        return fail;
    }

    void *hiddenPageData = malloc(PAGE_SIZE);
    int offset = 0;
    *(int *)((char *)hiddenPageData + offset) = fileHandle.readPageCounter;
    offset += sizeof(int);
    *(int *)((char *)hiddenPageData + offset) = fileHandle.writePageCounter + 1; // Write to hidden page, hence increase by 1
    offset += sizeof(int);
    *(int *)((char *)hiddenPageData + offset) = fileHandle.appendPageCounter;

    if (fwrite(hiddenPageData, sizeof(int), counterCount, file) != counterCount)
    {
        free(hiddenPageData);
        return fail;
    }
    free(hiddenPageData);

    if (fclose(file) != success)
    {
        return fail;
    }

    return success;

    //all of the file's pages are flushed to disk when the file is closed ?
}

FileHandle::FileHandle()
{
    readPageCounter = 0;
    writePageCounter = 0;
    appendPageCounter = 0;

    filePointer = nullptr;
}

FileHandle::~FileHandle() = default;

RC FileHandle::readPage(PageNum pageNum, void *data)
{
    FILE *file = filePointer;
    if (!file)
    {
        return fail; // if the file does not exist
    }
    if (pageNum > getNumberOfPages() - 1)
    {
        return fail; //the pageNum is larger than the number of pages in the file
    }
    long int offset = (pageNum + 1) * PAGE_SIZE;
    if (fseek(file, offset, SEEK_SET) != success)
    {
        return fail; //set the position indicator to the page we need to read
    }
    if (fread(data, 1, PAGE_SIZE, file) != PAGE_SIZE)
    {
        return fail; //read the page and store them in the *data
    }
    readPageCounter++;
    rewind(file); // set the position indicator to the beginning of the file
    return success;
}

RC FileHandle::writePage(PageNum pageNum, const void *data)
{
    FILE *pFile = filePointer;
    if (!pFile)
    {
        return fail;
    }
    if (pageNum > getNumberOfPages() - 1)
    {
        return fail;
    }
    long offset = (pageNum + 1) * PAGE_SIZE;

    if (fseek(pFile, offset, SEEK_SET) != success)
    {
        return fail;
    }
    if (fwrite(data, 1, PAGE_SIZE, pFile) != PAGE_SIZE)
    {
        return fail;
    }
    fflush(pFile);
    writePageCounter++;
    rewind(pFile);
    return success;
}

RC FileHandle::appendPage(const void *data)
{
    FILE *pFile = filePointer;
    if (!pFile)
    {
        return fail;
    }

    long offset = (getNumberOfPages() + 1) * PAGE_SIZE;

    if (fseek(pFile, offset, SEEK_SET) != success)
    {
        return fail;
    }
    if (fwrite(data, 1, PAGE_SIZE, pFile) != PAGE_SIZE)
    {
        return fail;
    }
    fflush(pFile);
    appendPageCounter++;
    rewind(pFile);
    return success;
}

unsigned FileHandle::getNumberOfPages()
{
    FILE *file = filePointer;
    if (!file)
    {
        return fail;
    }

    fseek(file, 0, SEEK_END);
    long sizeOfFile = ftell(file);
    unsigned num = ceil(sizeOfFile / PAGE_SIZE);

    rewind(file);
    return num - 1;
}

RC FileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount)
{
    readPageCount = readPageCounter;
    writePageCount = writePageCounter;
    appendPageCount = appendPageCounter;

    return 0;
}