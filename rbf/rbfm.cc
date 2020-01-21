#include "rbfm.h"
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

const int SLOT_NUMBER_SPACE_SIZE = 4;
const int FREE_SPACE_SIZE = 4;
const int RID_OFFSET_SIZE = 2;
const int RID_RECORD_LENGTH = 2;
RecordBasedFileManager &RecordBasedFileManager::instance() {
    static RecordBasedFileManager _rbf_manager = RecordBasedFileManager();
    return _rbf_manager;
}

RecordBasedFileManager::RecordBasedFileManager() = default;

RecordBasedFileManager::~RecordBasedFileManager() = default;

RecordBasedFileManager::RecordBasedFileManager(const RecordBasedFileManager &) = default;

RecordBasedFileManager &RecordBasedFileManager::operator=(const RecordBasedFileManager &) = default;

RC RecordBasedFileManager::createFile(const std::string &fileName) {
    return PagedFileManager::instance().createFile(fileName);
}

RC RecordBasedFileManager::destroyFile(const std::string &fileName) {
    return PagedFileManager::instance().destroyFile(fileName);
}

RC RecordBasedFileManager::openFile(const std::string &fileName, FileHandle &fileHandle) {
    return PagedFileManager::instance().openFile(fileName, fileHandle);
}

RC RecordBasedFileManager::closeFile(FileHandle &fileHandle) {
    return PagedFileManager::instance().closeFile(fileHandle);
}

RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                        const void *data, RID &rid)
{
    // return -1;
    void *record = malloc(PAGE_SIZE);
    int recordSize = transformData(recordDescriptor, data, record);  // Get record and recordSize

    void *currentPage = malloc(PAGE_SIZE);   // Create a new page to cache the content of the page
    int pageCount = fileHandle.getNumberOfPages() - 1;
    if (pageCount == -1)                          // If there is no page
    {
        UpdateFirstSlots(currentPage, fileHandle, record, recordSize);    // Append a new page at the end of the file
        rid.pageNum = pageCount + 1;
        rid.slotNum = getSlotNumber(currentPage);

        free(currentPage);
        return 0;
    }
    for(int i = pageCount; i >= 0; i--){
        fileHandle.readPage(i, currentPage);                // Put the page content to currentPage
        int freeSpace = getFreeSpaceOfCurrentPage(currentPage);
        int costByThisRecord = recordSize + RID_OFFSET_SIZE + RID_RECORD_LENGTH;

        if (costByThisRecord <= freeSpace)  // Current page has enough space
        {
            int offset = PAGE_SIZE - freeSpace - SLOT_NUMBER_SPACE_SIZE - FREE_SPACE_SIZE - getSlotNumber(currentPage) * 2 * sizeof(short);
            UpdateSlots(currentPage, fileHandle, record, offset, recordSize, i);
            rid.pageNum = i;
            rid.slotNum = getSlotNumber(currentPage);
            free(currentPage);
            return 0;
        }
        memset(currentPage, 0, PAGE_SIZE);
    }
        // Append a new page
    UpdateFirstSlots(currentPage, fileHandle, record, recordSize);
    rid.pageNum = pageCount + 1;
    rid.slotNum = getSlotNumber(currentPage);
    free(currentPage);
    return 0;
}
;RC RecordBasedFileManager::getFreeSpaceOfCurrentPage(void *currentPage)
{
    return *((int *)((char *)currentPage + PAGE_SIZE - FREE_SPACE_SIZE));
}
RC RecordBasedFileManager::UpdateFirstSlots(void *currentPage, FileHandle &fileHandle, void *record, int recordSize)
{
    memcpy((char *)currentPage, (char *)record, recordSize);
    *((int *)((char *)currentPage + PAGE_SIZE - FREE_SPACE_SIZE)) = PAGE_SIZE - recordSize - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE - 2 * sizeof(short);
    *((int *)((char *)currentPage + PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE)) = 1;

    int insertPos = PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE - 2 * sizeof(short);
    *((short *)((char *)currentPage + insertPos)) = (short)(recordSize);
    *((short *)((char *)currentPage + insertPos + sizeof(short))) = (short)0;
    fileHandle.appendPage(currentPage);

    return 0;
}

RC RecordBasedFileManager::UpdateSlots(void *currentPage, FileHandle &fileHandle, void *record, int offset, int recordSize, int pageCount)
{
    memcpy((char *)currentPage + offset, (char *)record, recordSize);
    *((int *)((char *)currentPage + PAGE_SIZE - FREE_SPACE_SIZE)) = getFreeSpaceOfCurrentPage(currentPage) - recordSize - 2 * sizeof(short);
    *((int *)((char *)currentPage + PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE)) = getSlotNumber(currentPage) + 1;
    int insertPos = PAGE_SIZE - SLOT_NUMBER_SPACE_SIZE - FREE_SPACE_SIZE - getSlotNumber(currentPage) * 2 * sizeof(short);// - 2 * sizeof(short);
    *((short *)((char *)currentPage + insertPos)) = (short)recordSize;
    short startPos = *((short *) ((char *) currentPage + insertPos + 2 * sizeof(short))) + *((short *)((char *) currentPage + insertPos + 3 * sizeof(short) ));

    *((short *)((char *)currentPage + insertPos + sizeof(short))) = (short) startPos;

    int rc = fileHandle.writePage(pageCount, currentPage);
    return 0;
}

RC RecordBasedFileManager::getSlotNumber(void *currentPage)
{
    return *((int *)((char *)currentPage + PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE));
}

RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                      const RID &rid, void *data)
{

    void *currentPage = malloc(PAGE_SIZE);
    fileHandle.readPage(rid.pageNum, currentPage);
    int pos = rid.slotNum;
    int offset = PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE - pos * 2 * sizeof(short);
    int len = *((short *)((char *)currentPage + offset));

    int start = *((short *)((char *) currentPage + offset + sizeof(short)));

    void *contentOfRecords = malloc(PAGE_SIZE);
    memcpy((char *)contentOfRecords, (char *)currentPage + start, len);

    prepareRecord(data,  recordDescriptor, contentOfRecords, len);

    free(currentPage);
    free(contentOfRecords);
    return 0;
}

void RecordBasedFileManager::prepareRecord(void *buffer, const std::vector<Attribute> &recordDescriptor, void *contentOfRecords, int len) {

    int fieldCount = recordDescriptor.size();
    auto *nullFieldsIndicator = (unsigned char *) malloc(ceil((double) fieldCount / 8));
    memset(nullFieldsIndicator, 0, ceil((double) fieldCount / 8));

    int i = sizeof(short);
    std::vector<short> offsetOfField;

    for (int j = 0; j < fieldCount; j++) {
        offsetOfField.push_back(*(short *) ((char *)contentOfRecords + i));
        i += 2;
    }
    int startAddress = (offsetOfField.size() + 1) * sizeof(short);
    for (int i = 0; i < fieldCount; i++) {
        if (offsetOfField[i] < 0) {
            *(nullFieldsIndicator + i / 8) |= (unsigned) 1 << (unsigned) (7 - (i % 8));
        }
    }
    int offset = 0;

    memcpy((char *) buffer + offset, nullFieldsIndicator, ceil((double) fieldCount / 8));
    offset += ceil((double) fieldCount / 8);

    for (int i = 0; i < fieldCount; i++) {
        if (offsetOfField[i] == -1) {

        } else {
            switch (recordDescriptor[i].type) {
                case TypeInt:
                    memcpy((int *)((char *) buffer + offset), (int *) ((char *) contentOfRecords + startAddress),
                           offsetOfField[i] - startAddress);
                    offset += sizeof(int);
                    startAddress = offsetOfField[i];
                    break;
                case TypeReal:
                    memcpy((float *) ((char *) buffer + offset), (float *) ((char *) contentOfRecords + startAddress),
                           offsetOfField[i] - startAddress);
                    offset += sizeof(float);
                    startAddress = offsetOfField[i];
                    break;
                case TypeVarChar:
                    *(int *) ((char *) buffer + offset) = offsetOfField[i] - startAddress;
                    offset += sizeof(int);
                    memcpy((char *) buffer + offset, (char *) contentOfRecords + startAddress,
                           offsetOfField[i] - startAddress);
                    offset += offsetOfField[i] - startAddress;
                    startAddress = offsetOfField[i];
                    break;
                default:
                    break;
            }

        }
    }
}
// Calculate actual bytes for nulls-indicator for the given field counts
RC RecordBasedFileManager::getActualByteForNullsIndicator(int fieldCount) {
    return ceil((double) fieldCount / CHAR_BIT);

}

int RecordBasedFileManager::transformData(const std::vector<Attribute> &recordDescriptor, const void *data, void *record) {
    short fieldCount = recordDescriptor.size();
    int nullFieldsIndicatorActualSize = getActualByteForNullsIndicator(fieldCount);
    //A类型 词条个数 + 每个词条的偏移量 + 所有词条
    int recordLen = 0; //记录formatted data（A类型）的长度
    void *actualContent = (char *)malloc(4096);

    int offset = 0; //B类型的指针

    auto *nullFieldsIndicator = (unsigned char *) malloc(sizeof(char) * nullFieldsIndicatorActualSize);
    memset(nullFieldsIndicator, 0, nullFieldsIndicatorActualSize);
    memcpy((char *) nullFieldsIndicator, data, nullFieldsIndicatorActualSize);
    offset += nullFieldsIndicatorActualSize;

    unsigned short *offsetInFormattedData = new unsigned short[fieldCount + 1];

    offsetInFormattedData[0] = sizeof(short) * (fieldCount + 1);
    int end = offsetInFormattedData[0];
    int leftShift = 7;
    for(int i = 0; i < recordDescriptor.size(); i++){
        Attribute attr = recordDescriptor[i];

        if (nullFieldsIndicator[i / 8] & ((unsigned) 1 << (unsigned) leftShift)) { //null
            offsetInFormattedData[i + 1] = -1;
        } else {
            void *buffer;
            switch (attr.type) {
                case TypeInt:
                    buffer = malloc(sizeof(int));
                    memcpy(buffer, (char *) data + offset, sizeof(int));
                    offset += sizeof(int);

                    offsetInFormattedData[i + 1] = end + sizeof(int);
                    end = offsetInFormattedData[i + 1];
                    memcpy((char *)actualContent + recordLen, (int *) buffer, sizeof(int));
                    recordLen += sizeof(int);
                    break;
                case TypeReal:
                    buffer = malloc(sizeof(float));
                    memcpy(buffer, (char *) data + offset, sizeof(float));
                    offset += sizeof(float);

                    offsetInFormattedData[i + 1] = end + sizeof(float);
                    end = offsetInFormattedData[i + 1];
                    memcpy((char *)actualContent + recordLen, (float *) buffer, sizeof(float));
                    recordLen += sizeof(float);
                    break;
                case TypeVarChar:
                    buffer = malloc(sizeof(int));
                    memcpy(buffer, (char *) data + offset, sizeof(int));
                    int varCharLen = *(int *) buffer;
                    offset += sizeof(int);
                    free(buffer);
                    buffer = malloc(varCharLen);
                    memcpy((char *) buffer, (char *) data + offset, varCharLen);
                    offset += varCharLen;

                    offsetInFormattedData[i + 1] = end + varCharLen;
                    end = offsetInFormattedData[i + 1];

                    auto *tempPointer = (char *) buffer;
                    for (int i = 0; i < varCharLen; i++) {
                        memcpy((char *)actualContent + recordLen, tempPointer, 1);
                        tempPointer++;
                        recordLen++;
                    }
                    break;
            }
            free(buffer);
        }
        if (leftShift == 0) {
            leftShift += 8;
        }
        leftShift--;
    }

    *((short *) ((char *) record))  = fieldCount;
    int index = sizeof(short);

    for(int i = 1; i <= fieldCount; i++){
        *((short *) ((char *) record + index)) = offsetInFormattedData[i];
        index += sizeof(short);
    }
    memcpy((char *) record + index, actualContent, recordLen);
    free(actualContent);
    return recordLen + index;
}
RC RecordBasedFileManager::printRecord(const std::vector<Attribute> &recordDescriptor, const void *data) {
    int fieldCount = recordDescriptor.size();
    int nullFieldsIndicatorActualSize = getActualByteForNullsIndicator(fieldCount);

    int offset = 0;

    auto *nullFieldsIndicator = (unsigned char *) malloc(sizeof(char) * nullFieldsIndicatorActualSize);
    memset(nullFieldsIndicator, 0, nullFieldsIndicatorActualSize);
    memcpy((char *) nullFieldsIndicator, data, nullFieldsIndicatorActualSize);
    offset += nullFieldsIndicatorActualSize;

    int leftShift = 7;
    for (int i = 0; i < recordDescriptor.size(); i++) {
        Attribute attr = recordDescriptor[i];

        if (nullFieldsIndicator[i / 8] & ((unsigned) 1 << (unsigned) leftShift)) {
            std::cout << attr.name << ": NULL" << "\t";
        } else {
            void *buffer;
            switch (attr.type) {
                case TypeInt:
                    buffer = malloc(sizeof(int));
                    memcpy(buffer, (char *) data + offset, sizeof(int));
                    offset += sizeof(int);
                    std::cout << attr.name << ": " << (*(int *) buffer) << "\t";
                    break;
                case TypeReal:
                    buffer = malloc(sizeof(float));
                    memcpy(buffer, (char *) data + offset, sizeof(float));
                    offset += sizeof(float);
                    std::cout << attr.name << ": " << (*(float *) buffer) << "\t";
                    break;
                case TypeVarChar:
                    buffer = malloc(sizeof(int));
                    memcpy(buffer, (char *) data + offset, sizeof(int));
                    int varCharLen = *(int *) buffer;
                    offset += sizeof(int);
                    free(buffer);
                    buffer = malloc(varCharLen);
                    memcpy((char *) buffer, (char *) data + offset, varCharLen);
                    offset += varCharLen;

                    std::cout << attr.name << ": ";
                    auto *tempPointer = (char *) buffer;
                    for (int i = 0; i < varCharLen; i++) {
                        std :: cout << *tempPointer;
                        tempPointer++;
                    }
                    std :: cout << "\t";
                    break;
            }
            free(buffer);
        }
        if (leftShift == 0) {
            leftShift += 8;
        }
        leftShift--;
        std::cout << std :: endl;
    }
    return 0;
}
RC RecordBasedFileManager::updateRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                        const void *data, const RID &rid) {
    return -1;
}

RC RecordBasedFileManager::readAttribute(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                         const RID &rid, const std::string &attributeName, void *data) {
    return -1;
}

RC RecordBasedFileManager::scan(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                const std::string &conditionAttribute, const CompOp compOp, const void *value,
                                const std::vector<std::string> &attributeNames, RBFM_ScanIterator &rbfm_ScanIterator) {
    return -1;
}


