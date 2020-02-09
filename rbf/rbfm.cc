#include "rbfm.h"
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <cstdio>
#include <cmath>
#include <string.h>
#include <algorithm>

const int SLOT_NUMBER_SPACE_SIZE = 4;
const int FREE_SPACE_SIZE = 4;
const int RID_OFFSET_SIZE = 2;
const int RID_RECORD_LENGTH = 2;
const int success = 0;
const int fail = -1;

RecordBasedFileManager &RecordBasedFileManager::instance()
{
    static RecordBasedFileManager _rbf_manager = RecordBasedFileManager();
    return _rbf_manager;
}

RecordBasedFileManager::RecordBasedFileManager() = default;

RecordBasedFileManager::~RecordBasedFileManager() = default;

RecordBasedFileManager::RecordBasedFileManager(const RecordBasedFileManager &) = default;

RecordBasedFileManager &RecordBasedFileManager::operator=(const RecordBasedFileManager &) = default;

RC RecordBasedFileManager::createFile(const std::string &fileName)
{
    return PagedFileManager::instance().createFile(fileName);
}

RC RecordBasedFileManager::destroyFile(const std::string &fileName)
{
    return PagedFileManager::instance().destroyFile(fileName);
}

RC RecordBasedFileManager::openFile(const std::string &fileName, FileHandle &fileHandle)
{
    return PagedFileManager::instance().openFile(fileName, fileHandle);
}

RC RecordBasedFileManager::closeFile(FileHandle &fileHandle)
{
    return PagedFileManager::instance().closeFile(fileHandle);
}

RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                        const void *data, RID &rid)
{
    void *record = malloc(PAGE_SIZE);
    int recordSize = transformData(recordDescriptor, data, record); // Get record and recordSize
    fileHandle.slotCounter += 1;
    void *currentPage = malloc(PAGE_SIZE); // Create a new page to cache the content of the page
    int pageCount = fileHandle.getNumberOfPages() - 1;
    if (pageCount == -1) // If there is no page
    {
        UpdateFirstSlots(currentPage, fileHandle, record, recordSize); // Append a new page at the end of the file
        rid.pageNum = pageCount + 1;
        rid.slotNum = getSlotNumber(currentPage);

        free(currentPage);
        free(record);
        return success;
    }
    int pageIndex = -1;
    int minFreeSpace = 4096;
    for (auto i = pageCount; i >= 0; i--)
    {
        fileHandle.readPage(i, currentPage); // Put the page content to currentPage
        int freeSpace = getFreeSpaceOfCurrentPage(currentPage);
        int costByThisRecord = recordSize + RID_OFFSET_SIZE + RID_RECORD_LENGTH;

        if (costByThisRecord <= freeSpace) // Current page has enough space
        {
            if (pageIndex == -1)
            {
                pageIndex = i;
                minFreeSpace = freeSpace;
                if (i == pageCount)
                {
                    break;
                }
            }
            else if (minFreeSpace > freeSpace)
            {
                pageIndex = i;
                minFreeSpace = freeSpace;
            }
        }
        memset(currentPage, 0, PAGE_SIZE);
    }
    if (pageIndex != -1) //insert into page with enough space
    {
        fileHandle.readPage(pageIndex, currentPage); // Put the page content to currentPage
        int offset = countOffsetForNewRecord(currentPage);
        UpdateSlots(currentPage, fileHandle, record, offset, recordSize, pageIndex, rid);
        free(currentPage);
        free(record);
    }
    else
    {
        // Append a new page
        UpdateFirstSlots(currentPage, fileHandle, record, recordSize);
        rid.pageNum = pageCount + 1;
        rid.slotNum = getSlotNumber(currentPage);
        free(currentPage);
        free(record);
    }
    return success;
};
RC RecordBasedFileManager::countOffsetForNewRecord(void *page)
{
    int offset = PAGE_SIZE - getFreeSpaceOfCurrentPage(page) - SLOT_NUMBER_SPACE_SIZE - FREE_SPACE_SIZE - getSlotNumber(page) * 2 * sizeof(short);
    return offset;
}
RC RecordBasedFileManager::updateFreeSpace(void *page, int space)
{
    *((int *)((char *)page + PAGE_SIZE - FREE_SPACE_SIZE)) = space;
    return success;
}
RC RecordBasedFileManager::getFreeSpaceOfCurrentPage(void *currentPage)
{
    return *((int *)((char *)currentPage + PAGE_SIZE - FREE_SPACE_SIZE));
}
RC RecordBasedFileManager::updateSlotNum(void *page, int num)
{
    *((int *)((char *)page + PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE)) = num;
    return success;
}
RC RecordBasedFileManager::updateSlotDirectoryForOneSlot(void *page, int offset, int len, int start)
{
    *((short *)((char *)page + offset)) = (short)len;
    *((short *)((char *)page + offset + sizeof(short))) = (short)start;
    return success;
}
RC RecordBasedFileManager::UpdateFirstSlots(void *currentPage, FileHandle &fileHandle, void *record, int recordSize)
{
    //1.insert record
    memcpy((char *)currentPage, (char *)record, recordSize);

    //2.update free space
    int space = PAGE_SIZE - recordSize - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE - 2 * sizeof(short);
    updateFreeSpace(currentPage, space);

    //3.update slot number
    int slotNum = 1;
    updateSlotNum(currentPage, slotNum);

    //4.update slot directory
    int insertPos = PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE - 2 * sizeof(short);
    updateSlotDirectoryForOneSlot(currentPage, insertPos, recordSize, 0);

    fileHandle.appendPage(currentPage);

    return success;
}

RC RecordBasedFileManager::UpdateSlots(void *currentPage, FileHandle &fileHandle, void *record, int offset, int recordSize, int pageCount, RID &rid)
{
    //1.insert record
    memcpy((char *)currentPage + offset, (char *)record, recordSize);

    int insertPos = findInsertPos(currentPage, rid);
    if (insertPos == -1)
    { //no previous deleted slot
        //2.update rid
        rid.slotNum = getSlotNumber(currentPage) + 1;

        //3.update slot directory
        insertPos = PAGE_SIZE - SLOT_NUMBER_SPACE_SIZE - FREE_SPACE_SIZE - (getSlotNumber(currentPage) + 1) * 2 * sizeof(short);
        *((short *)((char *)currentPage + insertPos)) = (short)recordSize;
        *((short *)((char *)currentPage + insertPos + sizeof(short))) = offset;

        //4.update free space and slot number
        int slotNum = getSlotNumber(currentPage) + 1;
        int space = getFreeSpaceOfCurrentPage(currentPage) - recordSize - 2 * sizeof(short);
        updateFreeSpace(currentPage, space);
        updateSlotNum(currentPage, slotNum);
    }
    else
    { //use the previous deleted slot, and there is no need to update the slot number
        //2. update slot directory
        *((short *)((char *)currentPage + insertPos)) = (short)recordSize;
        *((short *)((char *)currentPage + insertPos + sizeof(short))) = offset;
        //3. update free space
        int space = getFreeSpaceOfCurrentPage(currentPage) - recordSize;
        updateFreeSpace(currentPage, space);
    }

    fileHandle.writePage(pageCount, currentPage);
    rid.pageNum = pageCount;
    return success;
}
RC RecordBasedFileManager::findInsertPos(void *page, RID &rid)
{
    int end = PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE - 2 * sizeof(short);
    int insertPos = -1;
    //check whether there is previous deleted slot
    for (int i = 1; i <= getSlotNumber(page); i++)
    {
        if (*(short *)((char *)page + end) == 0)
        { //find one, use this slot
            insertPos = end;
            rid.slotNum = i;
            break;
        }
        else
        {
            end -= 2 * sizeof(short);
        }
    }
    return insertPos;
}
RC RecordBasedFileManager::getSlotNumber(void *currentPage)
{
    return *((unsigned int *)((char *)currentPage + PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE));
}
RC RecordBasedFileManager::deleteRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor, const RID &rid)
{
    void *currentPage = malloc(PAGE_SIZE);
    //read the page to the buffer
    fileHandle.readPage(rid.pageNum, currentPage);

    int pos = rid.slotNum;
    int pageNum = rid.pageNum;
    int offset = PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE -
                 pos * 2 * sizeof(short);                                   //get the record directory with rid
    int len = *((short *)((char *)currentPage + offset));                   //get the length of record
    int start = *((short *)((char *)currentPage + offset + sizeof(short))); //get the start pos of the record

    if (start == -1 && len == 0)
    {
        free(currentPage);
        return fail;
    }
    while (len < 0)
    { //this slot is just a pointer, point to a different page. len: -slotNum, start: -pageNum
        //free all the pointers because the record in the end will be deleted.
        *((short *)((char *)currentPage + offset)) = 0;
        *((short *)((char *)currentPage + offset + sizeof(short))) = -1;
        fileHandle.writePage(pageNum, currentPage);

        //get the page which the pointer points at
        memset(currentPage, 0, PAGE_SIZE);
        pageNum = 0 - start;
        fileHandle.readPage(pageNum, currentPage);
        offset = PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE - (0 - len) * 2 * sizeof(short);
        len = *((short *)((char *)currentPage + offset));
        start = *((short *)((char *)currentPage + offset + sizeof(short)));

        if (start == -1 && len == 0)
        {
            free(currentPage);
            return fail;
        }
    }

    *((short *)((char *)currentPage + offset + sizeof(short))) = -1; //delete this record, set the start pos to -1;
    *((short *)((char *)currentPage + offset)) = 0;                  //set the length of record to 0;

    //update the directory of slots that are not deleted
    updateSlotDirectory(currentPage, len, start + len);
    //shift the content to the left (free space saved by the deleted record)
    shiftContentToLeft(currentPage, len, start, 0);
    //the slotNum should remain the same;
    //*((int *)((char *)currentPage + PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE)) = getSlotNumber(currentPage) - 1;
    //update the free space
    *(int *)((char *)currentPage + PAGE_SIZE - FREE_SPACE_SIZE) += len;
    //write back to the file
    fileHandle.writePage(pageNum, currentPage);
    free(currentPage);

    return success;
}
RC RecordBasedFileManager::updateSlotDirectory(void *currentPage, int len, int end)
{
    int slotNums = getSlotNumber(currentPage);
    for (int i = 1; i <= slotNums; i++)
    {
        int offset = PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE -
                     i * 2 * sizeof(short);
        //add the slot that comes after 'end' should be updated (mistake: change slot directory according to the RID is wrong)
        if (*((short *)((char *)currentPage + offset + sizeof(short))) >= end)
        {
            *((short *)((char *)currentPage + offset + sizeof(short))) = *((short *)((char *)currentPage + offset + sizeof(short))) - len; //update the start pos of the rest of the record
        }
    }
    return success;
}
RC RecordBasedFileManager::shiftContentToLeft(void *currentPage, int len, int start, int recordSize)
{
    //if recordSize is 0, it is used in deletion. Otherwise, it is used in update...
    int left = len + start;                                                                                                                                     //the left end of the content that needed to be removed
    int right = PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE - getSlotNumber(currentPage) * 2 * sizeof(short) - getFreeSpaceOfCurrentPage(currentPage); //the right end of the content that needed to be removed

    if (right <= left)
    {
        return success;
    }
    void *content = malloc(PAGE_SIZE);
    //start...(len)...left...right
    memcpy((char *)content, (char *)currentPage + left, right - left);
    memcpy((char *)currentPage + start + recordSize, content, right - left);
    free(content);

    return success;
}

RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                      const RID &rid, void *data)
{

    void *currentPage = malloc(PAGE_SIZE);
    fileHandle.readPage(rid.pageNum, currentPage);
    int pos = rid.slotNum;

    int offset = PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE - pos * 2 * sizeof(short); //get the record directory with rid
    short len = *((short *)((char *)currentPage + offset));                                      //get the length of record
    short start = *((short *)((char *)currentPage + offset + sizeof(short)));                    //get the start pos of the record
    if (start == -1 && len == 0)
    { //no such record
        free(currentPage);
        return fail;
    }
    while (len < 0)
    { //this slot is just a pointer, point to a different page. len: -slotNum, start: -pageNum
        memset(currentPage, 0, PAGE_SIZE);
        fileHandle.readPage(0 - start, currentPage);
        offset = PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE - (0 - len) * 2 * sizeof(short);
        len = *((short *)((char *)currentPage + offset));                   //get the length of record
        start = *((short *)((char *)currentPage + offset + sizeof(short))); //get the start pos of the record

        if (start == -1 && len == 0)
        {
            free(currentPage);
            return fail;
        }
    }

    void *contentOfRecords = malloc(PAGE_SIZE);
    memcpy((char *)contentOfRecords, (char *)currentPage + start, len);

    prepareRecord(data, recordDescriptor, contentOfRecords, len);

    free(currentPage);
    free(contentOfRecords);
    return success;
}

void RecordBasedFileManager::prepareRecord(void *buffer, const std::vector<Attribute> &recordDescriptor, void *contentOfRecords, int len)
{

    int fieldCount = recordDescriptor.size();
    auto *nullFieldsIndicator = (unsigned char *)malloc(ceil((double)fieldCount / CHAR_BIT));
    memset(nullFieldsIndicator, 0, ceil((double)fieldCount / CHAR_BIT));

    int i = sizeof(short);
    std::vector<short> offsetOfField;

    for (int j = 0; j < fieldCount; j++)
    {
        offsetOfField.push_back(*(short *)((char *)contentOfRecords + i));
        i += 2;
    }
    int startAddress = (offsetOfField.size() + 1) * sizeof(short);
    for (int i = 0; i < fieldCount; i++)
    {
        if (offsetOfField[i] < 0)
        {
            *(nullFieldsIndicator + i / 8) |= (unsigned)1 << (unsigned)(7 - (i % 8));
        }
    }
    int offset = 0;

    memcpy((char *)buffer + offset, nullFieldsIndicator, ceil((double)fieldCount / CHAR_BIT));
    offset += ceil((double)fieldCount / CHAR_BIT);

    for (int i = 0; i < fieldCount; i++)
    {
        if (offsetOfField[i] == -1)
        {
        }
        else
        {
            int varCharLen;
            switch (recordDescriptor[i].type)
            {
            case TypeInt:
                memcpy((int *)((char *)buffer + offset), (int *)((char *)contentOfRecords + startAddress),
                       offsetOfField[i] - startAddress);
                offset += sizeof(int);
                startAddress = offsetOfField[i];
                break;
            case TypeReal:
                memcpy((float *)((char *)buffer + offset), (float *)((char *)contentOfRecords + startAddress),
                       offsetOfField[i] - startAddress);
                offset += sizeof(float);
                startAddress = offsetOfField[i];
                break;
            case TypeVarChar:
                varCharLen = offsetOfField[i] - startAddress;
                memcpy((char *)buffer + offset, &varCharLen, sizeof(int));
                offset += sizeof(int);
                memcpy((char *)buffer + offset, (char *)contentOfRecords + startAddress,
                       offsetOfField[i] - startAddress);
                offset += offsetOfField[i] - startAddress;
                startAddress = offsetOfField[i];
                break;
            default:
                break;
            }
        }
    }
    free(nullFieldsIndicator);
}
// Calculate actual bytes for nulls-indicator for the given field counts
RC RecordBasedFileManager::getActualByteForNullsIndicator(int fieldCount)
{
    return ceil((double)fieldCount / CHAR_BIT);
}
int RecordBasedFileManager::transformData(const std::vector<Attribute> &recordDescriptor, const void *data, void *record)
{
    short fieldCount = recordDescriptor.size();
    int nullFieldsIndicatorActualSize = getActualByteForNullsIndicator(fieldCount);
    // A type: fieldCount + offsetForEachField + Value for each field
    int recordLen = 0; // the length in bytes of the formatted data
    void *actualContent = (char *)malloc(PAGE_SIZE);

    int offset = 0; // Offset for B type data

    auto *nullFieldsIndicator = (unsigned char *)malloc(sizeof(char) * nullFieldsIndicatorActualSize);
    memset(nullFieldsIndicator, 0, nullFieldsIndicatorActualSize);
    memcpy((char *)nullFieldsIndicator, data, nullFieldsIndicatorActualSize);
    offset += nullFieldsIndicatorActualSize;

    unsigned short *offsetInFormattedData = new unsigned short[fieldCount + 1];

    offsetInFormattedData[0] = sizeof(short) * (fieldCount + 1);
    int end = offsetInFormattedData[0];
    int leftShift = 7;
    for (int i = 0; i < (int)recordDescriptor.size(); i++)
    {
        Attribute attr = recordDescriptor[i];

        if (nullFieldsIndicator[i / 8] & ((unsigned)1 << (unsigned)leftShift))
        { //null
            offsetInFormattedData[i + 1] = -1;
        }
        else
        {
            void *buffer;
            switch (attr.type)
            {
            case TypeInt:
                buffer = malloc(sizeof(int));
                memcpy(buffer, (char *)data + offset, sizeof(int));
                offset += sizeof(int);

                offsetInFormattedData[i + 1] = end + sizeof(int);
                end = offsetInFormattedData[i + 1];
                memcpy((char *)actualContent + recordLen, (int *)buffer, sizeof(int));
                recordLen += sizeof(int);
                break;
            case TypeReal:
                buffer = malloc(sizeof(float));
                memcpy(buffer, (char *)data + offset, sizeof(float));
                offset += sizeof(float);

                offsetInFormattedData[i + 1] = end + sizeof(float);
                end = offsetInFormattedData[i + 1];
                memcpy((char *)actualContent + recordLen, (float *)buffer, sizeof(float));
                recordLen += sizeof(float);
                break;
            case TypeVarChar:
                buffer = malloc(sizeof(int));
                memcpy(buffer, (char *)data + offset, sizeof(int));
                int varCharLen = *(int *)buffer;
                offset += sizeof(int);
                free(buffer);
                buffer = malloc(varCharLen);
                memcpy((char *)buffer, (char *)data + offset, varCharLen);
                offset += varCharLen;

                offsetInFormattedData[i + 1] = end + varCharLen;
                end = offsetInFormattedData[i + 1];

                auto *tempPointer = (char *)buffer;
                for (int i = 0; i < varCharLen; i++)
                {
                    memcpy((char *)actualContent + recordLen, tempPointer, 1);
                    tempPointer++;
                    recordLen++;
                }
                break;
            }
            free(buffer);
        }
        if (leftShift == 0)
        {
            leftShift += 8;
        }
        leftShift--;
    }

    *((short *)((char *)record)) = fieldCount;
    int index = sizeof(short);

    for (int i = 1; i <= fieldCount; i++)
    {
        *((short *)((char *)record + index)) = offsetInFormattedData[i];
        index += sizeof(short);
    }
    memcpy((char *)record + index, actualContent, recordLen);
    free(actualContent);
    free(nullFieldsIndicator);
    delete[] offsetInFormattedData;
    return recordLen + index;
}
RC RecordBasedFileManager::printRecord(const std::vector<Attribute> &recordDescriptor, const void *data) // Based on recordDescriptor, print data pointed by *data
{
    int fieldCount = recordDescriptor.size();
    int nullFieldsIndicatorActualSize = getActualByteForNullsIndicator(fieldCount);

    int offset = 0;

    auto *nullFieldsIndicator = (unsigned char *)malloc(sizeof(char) * nullFieldsIndicatorActualSize);
    memset(nullFieldsIndicator, 0, nullFieldsIndicatorActualSize);
    memcpy((char *)nullFieldsIndicator, data, nullFieldsIndicatorActualSize);
    offset += nullFieldsIndicatorActualSize;

    int leftShift = 7;
    for (int i = 0; i < (int)recordDescriptor.size(); i++)
    {
        Attribute attr = recordDescriptor[i];

        if (nullFieldsIndicator[i / 8] & ((unsigned)1 << (unsigned)leftShift))
        {
            std::cout << attr.name << ": NULL"
                      << " ";
        }
        else
        {
            void *buffer;
            switch (attr.type)
            {
            case TypeInt:
                buffer = malloc(sizeof(int));
                memcpy(buffer, (char *)data + offset, sizeof(int));
                offset += sizeof(int);
                std::cout << attr.name << ": " << (*(int *)buffer) << " ";
                break;
            case TypeReal:
                buffer = malloc(sizeof(float));
                memcpy(buffer, (char *)data + offset, sizeof(float));
                offset += sizeof(float);
                std::cout << attr.name << ": " << (*(float *)buffer) << " ";
                break;
            case TypeVarChar:
                buffer = malloc(sizeof(int));
                memcpy(buffer, (char *)data + offset, sizeof(int));
                int varCharLen = *(int *)buffer;
                offset += sizeof(int);
                free(buffer);
                buffer = malloc(varCharLen);
                memcpy((char *)buffer, (char *)data + offset, varCharLen);
                offset += varCharLen;

                std::cout << attr.name << ": ";
                auto *tempPointer = (char *)buffer;
                for (int i = 0; i < varCharLen; i++)
                {
                    std::cout << *tempPointer;
                    tempPointer++;
                }
                std ::cout << " ";
                break;
            }
            free(buffer);
        }
        if (leftShift == 0)
        {
            leftShift += 8;
        }
        leftShift--;
    }
    std::cout << std::endl;
    free(nullFieldsIndicator);
    return success;
}
RC RecordBasedFileManager::updateRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                        const void *data, const RID &rid)
{
    int slotNum = rid.slotNum;
    int pageNum = rid.pageNum;
    void *currentPage = malloc(PAGE_SIZE);
    void *record = malloc(PAGE_SIZE); //used to store the type A record;
    fileHandle.readPage(pageNum, currentPage);

    int len_dic = PAGE_SIZE - 2 * sizeof(int) - slotNum * sizeof(short) * 2;
    int start_dic = len_dic + sizeof(short);
    int len = *(short *)((char *)currentPage + len_dic);   //length of the record to be updated
    int pos = *(short *)((char *)currentPage + start_dic); //start pos of the record to be updated;

    while (len < 0)
    { //this slot is just a pointer, point to a different page. len: -slotNum, start: -pageNum
        memset(currentPage, 0, PAGE_SIZE);
        fileHandle.readPage(0 - pos, currentPage);
        len_dic = PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE - (0 - len) * 2 * sizeof(short);
        len = *((short *)((char *)currentPage + len_dic));                 //get the length of record
        pos = *((short *)((char *)currentPage + len_dic + sizeof(short))); //get the start pos of the record

        if (pos == -1 && len == 0)
        {
            free(currentPage);
            return fail;
        }
    }

    //transform type B data into type A data
    int recordSize = transformData(recordDescriptor, data, record); //return the transformed type A data's recordSize;
    //case1: type A data fits in place
    if (recordSize <= len)
    {
        //1.update the record in place, 2.shift the rest of the data to the left, 3.update the slot directory, 4.update space 5.write back to the file
        memcpy((char *)currentPage + pos, record, recordSize); //1. update the record in place
        shiftContentToLeft(currentPage, len, pos, recordSize); //2.shift the res of the date to the left.

        *(short *)((char *)currentPage + len_dic) = recordSize;        //3. update the udpated slot directory;
        updateSlotDirectory(currentPage, len - recordSize, len + pos); //update the rest of slot directory

        //4.update space
        *(int *)((char *)currentPage + PAGE_SIZE - sizeof(int)) += len - recordSize;

        //5.write back
        fileHandle.writePage(rid.pageNum, currentPage);
        free(currentPage);
        free(record);
        return success;
    }
    //recordSize > len
    //case2: type A data fits in the same page, but needs to move its place;
    if (recordSize - len <= getFreeSpaceOfCurrentPage(currentPage))
    {
        //1.shift content to the left(delete the previous one)
        shiftContentToLeft(currentPage, len, pos, 0);
        //2.append the updated record at the end
        int end = PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE - getSlotNumber(currentPage) * 2 * sizeof(short) - getFreeSpaceOfCurrentPage(currentPage) - len;
        memcpy((char *)currentPage + end, record, recordSize);

        //3.update the slot directory
        updateSlotDirectory(currentPage, len, len + pos);
        *(short *)((char *)currentPage + len_dic) = recordSize;
        *(short *)((char *)currentPage + start_dic) = end;

        //4.update the free space
        *(int *)((char *)currentPage + PAGE_SIZE - sizeof(int)) -= recordSize - len;

        //5.write back
        fileHandle.writePage(rid.pageNum, currentPage);
        free(currentPage);
        free(record);
        return success;
    }
    //case3: type A data cannot fit in the same page, needs to find another page which has free space;
    //recordSize - len > getFreeSpaceOfCurrentPage(currentPage) the first step is just like deletion, delete this record from this page
    //1.shift content to the left (free space saved by the deleted record)
    shiftContentToLeft(currentPage, len, pos, 0);
    //2.updateSlotDirectory
    updateSlotDirectory(currentPage, len, pos + len);
    //3.the slot number of current page should remain the same
    //*(int *)((char *)currentPage + PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE) = getSlotNumber(currentPage);
    //4.update the free space
    *(int *)((char *)currentPage + PAGE_SIZE - sizeof(int)) += len;
    //now, we still need to update the slot directory of the deleted record, find a new place for it.
    //5.find a new place for it. rid2 is the new place for rid.
    RID rid2;
    insertRecord(fileHandle, recordDescriptor, data, rid2);
    //6.update the slot directory
    *(short *)((char *)currentPage + len_dic) = 0 - rid2.slotNum;
    *(short *)((char *)currentPage + start_dic) = 0 - rid2.pageNum;

    //7.write back to the file
    fileHandle.writePage(rid.pageNum, currentPage);
    free(currentPage);
    free(record);

    return success;
}
RC RecordBasedFileManager::readAttribute(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                         const RID &rid, const std::string &attributeName, void *data)
{
    void *currentPage = malloc(PAGE_SIZE);
    void *record = malloc(PAGE_SIZE);
    unsigned int pageNum = rid.pageNum;
    unsigned int slotNum = rid.slotNum;
    std::cout << "before read attributes" << std::endl;
    std::cout << "page num is: " << pageNum << std::endl;
    int rc = fileHandle.readPage(pageNum, currentPage);
    if (rc != success) {
        return fail;
    }
    std::cout << "after read attributes" << std::endl;

    int len_dic = PAGE_SIZE - 2 * sizeof(int) - slotNum * sizeof(short) * 2;
    int start_dic = len_dic + sizeof(short);
    int len = *(short *)((char *)currentPage + len_dic);         //length of the record
    int start_pos = *(short *)((char *)currentPage + start_dic); //start pos of the record

    while (len < 0)
    { //this slot is just a pointer, point to a different page. len: -slotNum, start: -pageNum
        memset(currentPage, 0, PAGE_SIZE);
        fileHandle.readPage(0 - start_pos, currentPage);
        len_dic = PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE - (0 - len) * 2 * sizeof(short);
        len = *((short *)((char *)currentPage + len_dic));                       //get the length of record
        start_pos = *((short *)((char *)currentPage + len_dic + sizeof(short))); //get the start pos of the record

        if (start_pos == -1 && len == 0)
        {
            free(record);
            free(currentPage);
            return fail;
        }
    }

    memcpy((char *)record, (char *)currentPage + start_pos, len);

    bool isVarChar = false;
    int attribute_id = -1;
    for (int i = 0; i < (int)recordDescriptor.size(); i++)
    {
        if (recordDescriptor[i].name.compare(attributeName) == 0)
        {
            attribute_id = i + 1;
            if (recordDescriptor[i].type == TypeVarChar)
            {
                isVarChar = true;
            }
            break;
        }
    }

    if (attribute_id == -1)
    { //no such attribute name
        free(currentPage);
        free(record);
        if (attributeName == "")
        {
        }
        return fail;
    }

    auto *nullFieldsIndicator = (unsigned char *)malloc(ceil((double)1 / CHAR_BIT));
    memset(nullFieldsIndicator, 0, ceil((double)1 / CHAR_BIT));

    int fieldCount = *(short *)((char *)record);
    int start = -1; //the start pos of the record
    if (attribute_id == 1)
    {
        start = (fieldCount + 1) * sizeof(short);
    }
    else
    {
        for (int i = 2; i <= attribute_id; i++)
        {
            int pos = *(short *)((char *)record + (i - 1) * sizeof(short));
            ;
            if (pos == -1)
                continue;
            start = pos;
        }
        if (start == -1)
        {
            start = (fieldCount + 1) * sizeof(short);
        }
    }

    int end = *(short *)((char *)record + attribute_id * sizeof(short)); //the end pos of the record
    if (end == -1)
    {
        *(nullFieldsIndicator + 0 / 8) |= (unsigned)1 << (unsigned)(7 - (0 % 8));
    }

    memcpy((char *)data, nullFieldsIndicator, 1);
    if (end != -1)
    {
        int stringLen = end - start;
        if (!isVarChar)
        {
            memcpy((char *)data + 1, (char *)record + start, end - start);
        }
        else
        {
            // std::cout << "it is a varchar type " << std::endl;
            // std::cout << "the length of the var char is:" << stringLen << std::endl;
            // std::cout << "the contents in the var char are:" << std::endl;
            // for (int i = 0; i < end - start; i++)
            // {
            //     std::cout << *((char *)record + start + i);
            // }
            // std::cout << std::endl;
            memcpy((char *)data + 1, &stringLen, sizeof(int));
            memcpy((char *)data + 1 + sizeof(int), (char *)record + start, end - start);
            // std::cout << "memcpy no error" << std::endl;
        }
    }

    //    if (end - start != 4) {
    //        for (int j = 0; j < end - start; j++)
    //        std::cout << "the length of the string is:" <<  *(char *) ((char *)data + 1 + j) << std::endl;
    //    }
    free(currentPage);
    free(record);
    free(nullFieldsIndicator);
    //    std::cout << "exit successfully" << "length is: " << end - start << std::endl;
    return success;
}

RC RecordBasedFileManager::scan(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                const std::string &conditionAttribute, const CompOp compOp, const void *value,
                                const std::vector<std::string> &attributeNames, RBFM_ScanIterator &rbfm_ScanIterator)
{
    bool searchConditionAttributeFlag = false;
    rbfm_ScanIterator.fileHandle = fileHandle;
    rbfm_ScanIterator.recordDescriptor = recordDescriptor;
    rbfm_ScanIterator.conditionAttribute = conditionAttribute;
    rbfm_ScanIterator.compOp = compOp;
    rbfm_ScanIterator.value = value;
    rbfm_ScanIterator.attributeNames = attributeNames;

    std::vector<std::string> attributeNameVec;

    for (int i = 0; i < (int)recordDescriptor.size(); i++)
    {
        if (recordDescriptor[i].name == rbfm_ScanIterator.conditionAttribute)
        {
            rbfm_ScanIterator.conditionAttributePos = i;
            rbfm_ScanIterator.conditionAttributeType = (AttrType)recordDescriptor[i].type;
            searchConditionAttributeFlag = true;
        }
        attributeNameVec.push_back(recordDescriptor[i].name);
    }
    if (!searchConditionAttributeFlag && conditionAttribute.size() != 0)
    {
        std::cout << "Cannot find condition Attribute" << std::endl;
        return -1;
    }

    for (auto &attributeName : attributeNames)
    {
        std::vector<std::string>::iterator it = std::find(attributeNameVec.begin(), attributeNameVec.end(), attributeName);
        if (it == attributeNameVec.end())
        {
            std::cout << "This attribute is not part of the attribute" << std::endl;
        }
        int index = std::distance(attributeNameVec.begin(), it);
        rbfm_ScanIterator.attributeTypes.push_back(recordDescriptor[index].type);
    }

    return 0;
}
RC RBFM_ScanIterator::getTotalSlotNumber(FileHandle &fileHandle)
{
    int totalPage = fileHandle.getNumberOfPages();

    //    std::cout << "total Page is: " << totalPage << std::endl;
    //    int aa = 0;
    int pageNum = currentPageNum;
    int slotNum = currentSlotNum;
    for (int i = pageNum; i < totalPage; i++)
    {
        auto *currentpageData = malloc(PAGE_SIZE);
        if (fileHandle.readPage(pageNum, currentpageData) != 0)
        {
            free(currentpageData);
            return -1;
        }
        int totalSlotNumberForCurrentPage = rbfm->getSlotNumber(currentpageData);

        for (int j = slotNum; j <= totalSlotNumberForCurrentPage; j++)
        {
            int len = rbfm->getLengthForRecord(currentpageData, slotNum);
            int start = rbfm->getOffsetForRecord(currentpageData, slotNum);
            if (len + start < 0)
            {
                // std::cout << "invalid string" << std::endl;
                UpdatePageNumAndSLotNum(i, j, totalSlotNumberForCurrentPage, totalPage);
            }
            else
            {
                UpdatePageNumAndSLotNum(i, j, totalSlotNumberForCurrentPage, totalPage);
                free(currentpageData);
                return 0;
            }
        }
        free(currentpageData);
    }
    currentPageNum = 0;
    currentSlotNum = 1;
    return -12;
}
RC RecordBasedFileManager::getOffsetForRecord(void *currentPage, int slotNum)
{
    return *(short *)((char *)currentPage + PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE - slotNum * 2 * sizeof(short) + sizeof(short));
}

RC RecordBasedFileManager::getLengthForRecord(void *currentPage, int slotNum)
{
    return *(short *)((char *)currentPage + PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE - slotNum * 2 * sizeof(short));
}

RBFM_ScanIterator::RBFM_ScanIterator()
{
    rbfm = &RecordBasedFileManager::instance();
}
bool RBFM_ScanIterator::processWithTypeInt(int valueOfRecord, CompOp compOp, const void *value)
{
    int valueToCompare = *(int *)((char *)value);
    switch (compOp)
    {
    case EQ_OP:
        return valueOfRecord == valueToCompare;
        break;
    case LT_OP:
        return valueOfRecord < valueToCompare;
        break;
    case LE_OP:
        return valueOfRecord <= valueToCompare;
        break;
    case GT_OP:
        return valueOfRecord > valueToCompare;
        break;
    case GE_OP:
        return valueOfRecord >= valueToCompare;
        break;
    case NE_OP:
        return valueOfRecord != valueToCompare;
        break;
    default:
        std::cout << "TypeInt should not enter this statement" << std::endl;
        break;
    }
    return false;
}

bool RBFM_ScanIterator::processWithTypeReal(float valueOfRecord, CompOp compOp, const void *value)
{
    float valueToCompare = *(float *)((char *)value);
    switch (compOp)
    {
    case EQ_OP:
        return valueOfRecord == valueToCompare;
        break;
    case LT_OP:
        return valueOfRecord < valueToCompare;
        break;
    case LE_OP:
        return valueOfRecord <= valueToCompare;
        break;
    case GT_OP:
        return valueOfRecord > valueToCompare;
        break;
    case GE_OP:
        return valueOfRecord >= valueToCompare;
        break;
    case NE_OP:
        return valueOfRecord != valueToCompare;
        break;
    default:
        std::cout << "TypeReal should not enter this statement" << std::endl;
        break;
    }
    return false;
}
bool RBFM_ScanIterator::processWithTypeVarChar(std::string valueOfRecord, CompOp compOp, const void *value)
{
    // int stringLen = *(int *)((char *)value);
    int stringLen = 0;
    memcpy(&stringLen, (char *)value, sizeof(int));
    std::string valueToCompare = "";
    for (int i = 0; i < stringLen; i++)
    {
        valueToCompare += *((char *)value + sizeof(int) + i);
    }
    int isStringLarger = valueOfRecord.compare(valueToCompare);
    switch (compOp)
    {
    case EQ_OP:
        return isStringLarger == 0;
        break;
    case LT_OP:
        return isStringLarger < 0;
        break;
    case LE_OP:
        return isStringLarger <= 0;
        break;
    case GT_OP:
        return isStringLarger > 0;
        break;
    case GE_OP:
        return isStringLarger >= 0;
        break;
    case NE_OP:
        return isStringLarger != 0;
        break;
    default:
        std::cout << "TypeVarChar should not enter this statement" << std::endl;
        break;
    }
    return false;
}
bool RBFM_ScanIterator::processOnConditionAttribute(void *recordDataOfGivenAttribute, const void *value, CompOp compOp, AttrType conditionAttributeType, int isValidAttribute)
{
    if (isValidAttribute != 0)
    {
        return true;
    }
    auto *nullFieldIndicator = malloc(1);
    int offset = 1;
    memcpy(nullFieldIndicator, recordDataOfGivenAttribute, 1);
    bool isNullForThisField = *((char *)nullFieldIndicator) & ((unsigned)1 << (unsigned)7);
    free(nullFieldIndicator);
    if (compOp == NO_OP)
    {
        return true;
    }
    if (isNullForThisField)
    {
        // std::cout << "No value" << std::endl;
        if (compOp == EQ_OP && value == NULL)
        {
            return true;
        }
        return false;
    }
    if (value == NULL)
    {
        return false;
    }
    // auto *buffer = malloc(PAGE_SIZE);
    bool isSatisfiedRecord = false;
    int valueOfRecord = 0;
    float valueOfRecordFloat = 0.0;
    int stringLen = 0;
    std::string valueOfRecordVarChar = "";
    switch (conditionAttributeType)
    {
    case TypeInt:
        memcpy(&valueOfRecord, ((char *)recordDataOfGivenAttribute + offset), sizeof(int));
        // valueOfRecord = *(int *)((char *)recordDataOfGivenAttribute + offset);
        isSatisfiedRecord = processWithTypeInt(valueOfRecord, compOp, value);
        break;
    case TypeReal:
        memcpy(&valueOfRecordFloat, (char *)recordDataOfGivenAttribute + offset, sizeof(float));
        // valueOfRecordFloat = *(float *)((char *)recordDataOfGivenAttribute + offset);
        isSatisfiedRecord = processWithTypeReal(valueOfRecordFloat, compOp, value);
        break;
    case TypeVarChar:
        memcpy(&stringLen, (char *)recordDataOfGivenAttribute + offset, sizeof(int));
        // stringLen = *(int *)((char *)recordDataOfGivenAttribute + offset);
        offset += sizeof(int);
        valueOfRecordVarChar = "";
        for (int kk = 0; kk < stringLen; kk++)
        {
            valueOfRecordVarChar += *((char *)recordDataOfGivenAttribute + offset + kk);
        }
        isSatisfiedRecord = processWithTypeVarChar(valueOfRecordVarChar, compOp, value);
        // if (isSatisfiedRecord)
        // {
        //     std::cout << "this is satisfied" << std::endl;
        // }
        // else
        // {
        //     std::cout << "not satisfied" << std::endl;
        // }
        // *(int *)((char *)buffer + offset) = offsetOfField[i] - startAddress;
        // offset += sizeof(int);
        // memcpy((char *)buffer + offset, (char *)contentOfRecords + startAddress,
        //        offsetOfField[i] - startAddress);
        // offset += offsetOfField[i] - startAddress;
        // startAddress = offsetOfField[i];
        break;
    default:
        break;
    }
    return isSatisfiedRecord;
}

RC RBFM_ScanIterator::UpdatePageNumAndSLotNum(int i, int j, int totalSlotNumberForCurrentPage, int totalPage)
{
    if (j == totalSlotNumberForCurrentPage)
    {
        //        if (currentPageNum != totalPage - 1)
        //        {
        currentPageNum++;
        currentSlotNum = 1;
        //        }
        //        else {
        ////            currentSlotNum++;
        //        }
        //        currentPageNum = i + 1;
        //        //        if (currentPageNum == totalPage)
        //        //        {
        //        //            std::cout << "It's already the last slot for the last page, reset is needed" << std::endl;
        //        //            currentPageNum = 0;
        //        //            currentSlotNum = 1;
        //        //            return -1;
        //        //        }
        ////        if (i == totalPage)
        //            currentSlotNum = 1;
    }
    else
    {
        //        currentPageNum = i;
        currentSlotNum = j + 1;
    }
    return 0;
}

RC RBFM_ScanIterator::RetrieveProjectedAttributes(RID &rid, void *data)
{
    int fieldCount = attributeNames.size();
    //    std::cout << "fieldcount is" << fieldCount << std::endl;
    int nullFieldsIndicatorActualSize = rbfm->getActualByteForNullsIndicator(fieldCount);
    memset(data, 0, nullFieldsIndicatorActualSize);
    int offset = nullFieldsIndicatorActualSize;

    for (int i = 0; i < fieldCount; i++)
    {
        auto *recordDataOfGivenAttribute = malloc(PAGE_SIZE);
               std::cout << "attributesName is: " << attributeTypes[i] << std::endl;
        int rc = rbfm->readAttribute(fileHandle, recordDescriptor, rid, attributeNames[i], recordDataOfGivenAttribute);
        if (rc != success)
        {
            std::cout << "no such attribute!!" << std::endl;
        }
        bool isAttributeNull = *((char *)recordDataOfGivenAttribute) & ((unsigned)1 << (unsigned)7);
        if (isAttributeNull)
        {
            *((char *)data + i / 8) |= ((unsigned)1 << (unsigned)(7 - (i % 8)));
            continue;
        }
        else
        {
            //            std::cout << "here comes" << std::endl;
        }
        int stringLen = 0;
        //        std::cout << *(float *)((char *)recordDataOfGivenAttribute + 1) << std::endl;
        switch (attributeTypes[i])
        {
        case TypeInt:
            //            std::cout << "aaa" << std::endl;
            memcpy((char *)data + offset, (char *)recordDataOfGivenAttribute + 1, sizeof(int));
            offset += sizeof(int);
            break;
        case TypeReal:
            //            std::cout << "bbb" << std::endl;
            memcpy((char *)data + offset, (char *)recordDataOfGivenAttribute + 1, sizeof(float));
            offset += sizeof(float);
            break;
        case TypeVarChar:
            //            std::cout << "ccc" << std::endl;
            stringLen = *(int *)((char *)recordDataOfGivenAttribute + 1);
            //            std::cout << "the sring has length: " << stringLen << std::endl;
            memcpy((char *)data + offset, (char *)recordDataOfGivenAttribute + 1, sizeof(int) + stringLen);
            offset += sizeof(int) + stringLen;
            break;
        default:
            std::cout << "No matching attribute found, function in RetrieveProjectedAttributes must be wrong" << std::endl;
        }
        free(recordDataOfGivenAttribute);
    }
    return 0;
}
RC RBFM_ScanIterator::getNextRecord(RID &rid, void *data)
{
    unsigned int pageNum = fileHandle.getNumberOfPages();
    std::cout << "pageNum is: " << pageNum << std::endl;
    if (currentPageNum == pageNum)
    {
        currentPageNum = 0;
        currentSlotNum = 1;
        std::cout << "finish get next record" << std::endl;
        return RBFM_EOF;
    }
    void *currentPage = malloc(PAGE_SIZE);
    std::cout << "before read page" << std::endl;
    unsigned int eee = currentPageNum;
    std::cout << "the page num is: " << eee << std::endl;
    int rc = fileHandle.readPage(eee, currentPage);
    std::cout << "after read page" << std::endl;
    if (rc == fail)
    {
        std::cout << "read page faled" << std::endl;
        free(currentPage);
        currentPageNum = 0;
        currentSlotNum = 1;
        std::cout << "finish get next record2" << std::endl;
        return RBFM_EOF;
    }

    int len = rbfm->getLengthForRecord(currentPage, currentSlotNum);
    int start = rbfm->getOffsetForRecord(currentPage, currentSlotNum);
    if (len + start < 0)
    {
        rid.pageNum = currentPageNum;
        rid.slotNum = currentSlotNum;
        moveToNextRecord(currentPage);
        free(currentPage);
        return getNextRecord(rid, data);
    }
    else
    {
        rid.pageNum = currentPageNum;
        rid.slotNum = currentSlotNum;
        auto *recordDataOfGivenAttribute = malloc(PAGE_SIZE);
        int isValidAttribute = rbfm->readAttribute(fileHandle, recordDescriptor, rid, conditionAttribute, recordDataOfGivenAttribute);
        bool isValidRecord = processOnConditionAttribute(recordDataOfGivenAttribute, value, compOp, conditionAttributeType, isValidAttribute);
        free(recordDataOfGivenAttribute);
        if (!isValidRecord)
        {
            moveToNextRecord(currentPage);
            free(currentPage);
            return getNextRecord(rid, data);
        }
        else
        {
            moveToNextRecord(currentPage);
            std::cout << "start retrieveprojected attributes" << std::endl;
            RetrieveProjectedAttributes(rid, data);
            free(currentPage);
            return 0;
        }
    }
    //    int slotNum = rbfm -> getSlotNumber(currentPage);
    //    if(currentSlotNum > slotNum){
    //
    //    }

    // Iterative for each page, for each slot of the page, we change the format of the
    //    int totalPage = fileHandle.getNumberOfPages();
    //    int pageNum = currentPageNum;
    //    int slotNum = currentSlotNum;
    //    //
    //    for (int i = pageNum; i < totalPage; i++)
    //    {
    //        auto *currentpageData = malloc(PAGE_SIZE);
    //        if (fileHandle.readPage(i, currentpageData) != 0)
    //        {
    //            free(currentpageData);
    //            return -1;
    //        }
    //        rid.pageNum = i;
    //        int totalSlotNumberForCurrentPage = rbfm->getSlotNumber(currentpageData);
    //
    //        for (int j = slotNum; j <= totalSlotNumberForCurrentPage; j++)
    //        {
    //            rid.slotNum = j;
    //
    //            int len = rbfm->getLengthForRecord(currentpageData, j);
    //            int start = rbfm->getOffsetForRecord(currentpageData, j);
    //
    //            // If this slot is a deleted record or updated record, jump to next iteration
    //            if (start + len < 0)
    //            {
    //                int aa = UpdatePageNumAndSLotNum(i, j, totalSlotNumberForCurrentPage, totalPage);
    //            }
    //
    //            else
    //            {
    //                auto *recordDataOfGivenAttribute = malloc(PAGE_SIZE);
    //                //            rbfm->readAttribute(fileHandle, recordDescriptor, rid, conditionAttribute, recordDataOfGivenAttribute);
    //                //
    //                //                std::cout << "before read attributes" << std::endl;
    //                int isValidAttribute = rbfm->readAttribute(fileHandle, recordDescriptor, rid, conditionAttribute, recordDataOfGivenAttribute);
    //                //
    //                //                std::cout << "after read attributes, before process valid" << std::endl;
    //                bool isValidRecord = processOnConditionAttribute(recordDataOfGivenAttribute, value, compOp, conditionAttributeType, isValidAttribute);
    //                free(recordDataOfGivenAttribute);
    //                if (!isValidRecord)
    //                {
    //                    //                    std::cout << "rbfm cc, invalud record" << std::endl;
    //                    int bb = UpdatePageNumAndSLotNum(i, j, totalSlotNumberForCurrentPage, totalPage);
    //                }
    //                else
    //                {
    //                    int rc = UpdatePageNumAndSLotNum(i, j, totalSlotNumberForCurrentPage, totalPage);
    //                    //                    std::cout << "before retrieve attributes" << std::endl;
    //                    RetrieveProjectedAttributes(rid, data);
    //                    free(currentpageData);
    //                    return 0;
    //                }
    //                //
    //            }
    //        }
    //        free(currentpageData);
    //    }
    //
    //    currentPageNum = 0;
    //    currentSlotNum = 1;
    //
    //    //    rbfm->closeFile(fileHandle);
    //    return RBFM_EOF;
}
RC RBFM_ScanIterator::moveToNextRecord(void *currentPage)
{
    unsigned int totalpageSlot = rbfm->getSlotNumber(currentPage);
    if (currentSlotNum == totalpageSlot)
    {
        currentPageNum++;
        currentSlotNum = 1;
    }
    else
    {
        currentSlotNum++;
    }
    return success;
}