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
        free(record);
        return 0;
    }
    int pageIndex = -1;
    int minFreeSpace = 4096;
    for(int i = pageCount; i >= 0; i--){
        fileHandle.readPage(i, currentPage);                // Put the page content to currentPage
        int freeSpace = getFreeSpaceOfCurrentPage(currentPage);
        int costByThisRecord = recordSize + RID_OFFSET_SIZE + RID_RECORD_LENGTH;

        if (costByThisRecord <= freeSpace)  // Current page has enough space
        {
            if(pageIndex == -1){
                pageIndex = i;
                minFreeSpace = freeSpace;
            }else if(minFreeSpace > freeSpace){
                pageIndex = i;
                minFreeSpace = freeSpace;
            }
        }
        memset(currentPage, 0, PAGE_SIZE);
    }
    if(pageIndex != -1){
        fileHandle.readPage(pageIndex, currentPage);                // Put the page content to currentPage
        int freeSpace = getFreeSpaceOfCurrentPage(currentPage);
        int offset = PAGE_SIZE - freeSpace - SLOT_NUMBER_SPACE_SIZE - FREE_SPACE_SIZE - getSlotNumber(currentPage) * 2 * sizeof(short);
        UpdateSlots(currentPage, fileHandle, record, offset, recordSize, pageIndex, rid);
        free(currentPage);
        free(record);
    }else{
        // Append a new page
        UpdateFirstSlots(currentPage, fileHandle, record, recordSize);
        rid.pageNum = pageCount + 1;
        rid.slotNum = getSlotNumber(currentPage);
        free(currentPage);
        free(record);
    }
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

RC RecordBasedFileManager::UpdateSlots(void *currentPage, FileHandle &fileHandle, void *record, int offset, int recordSize, int pageCount, RID &rid)
{
    memcpy((char *)currentPage + offset, (char *)record, recordSize);
    *((int *)((char *)currentPage + PAGE_SIZE - FREE_SPACE_SIZE)) = getFreeSpaceOfCurrentPage(currentPage) - recordSize - 2 * sizeof(short);
    *((int *)((char *)currentPage + PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE)) = getSlotNumber(currentPage) + 1;
    int end = PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE - 2 * sizeof(short);
    int insertPos = -1;
    //check whether there is previous deleted slot
    for(int i = 1; i < getSlotNumber(currentPage); i++){
        if(*(short *)((char *) currentPage + end) == 0){ //find one, use this slot
            insertPos = end;
            rid.slotNum = i;
            break;
        }else{
            end -= 2 * sizeof(short);
        }
    }
    if(insertPos == -1){ //no previous deleted slot
        insertPos = PAGE_SIZE - SLOT_NUMBER_SPACE_SIZE - FREE_SPACE_SIZE - getSlotNumber(currentPage) * 2 * sizeof(short);
        rid.slotNum = getSlotNumber(currentPage);
        *((short *)((char *)currentPage + insertPos)) = (short)recordSize;
        short startPos = *((short *) ((char *) currentPage + insertPos + 2 * sizeof(short))) + *((short *)((char *) currentPage + insertPos + 3 * sizeof(short) ));
        *((short *)((char *)currentPage + insertPos + sizeof(short))) = (short) startPos;
    }else{  //use the previous deleted slot
        *((short *)((char *)currentPage + insertPos)) = (short)recordSize;
        *((short *)((char *)currentPage + insertPos + sizeof(short))) = offset;
    }

    fileHandle.writePage(pageCount, currentPage);
    rid.pageNum = pageCount;
    return 0;
}

RC RecordBasedFileManager::getSlotNumber(void *currentPage)
{
    return *((int *)((char *)currentPage + PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE));
}
RC RecordBasedFileManager::deleteRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor, const RID &rid) {
    void *currentPage = malloc(PAGE_SIZE);
    //read the page to the buffer
    fileHandle.readPage(rid.pageNum, currentPage);

    int pos = rid.slotNum;
    int pageNum = rid.pageNum;
    int offset = PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE -
                 pos * 2 * sizeof(short); //get the record directory with rid
    int len = *((short *) ((char *) currentPage + offset)); //get the length of record
    int start = *((short *) ((char *) currentPage + offset + sizeof(short))); //get the start pos of the record

    if(start == -1 && len == 0) {
        free(currentPage);
        return -1;
    }
    while(len < 0){ //this slot is just a pointer, point to a different page. len: -slotNum, start: -pageNum
        //free all the pointers because the record in the end will be deleted.
        *((short *) ((char *) currentPage + offset)) = 0;
        *((short *) ((char *) currentPage + offset + sizeof(short))) = -1;
        fileHandle.writePage(pageNum, currentPage);

        //get the page which the pointer points at
        memset(currentPage, 0, PAGE_SIZE);
        pageNum = 0 - start;
        fileHandle.readPage(pageNum, currentPage);
        offset = PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE - (0 - len) * 2 * sizeof(short);
        len = *((short *)((char *)currentPage + offset));
        start = *((short *)((char *) currentPage + offset + sizeof(short)));

        if(start == -1 && len == 0) {
            free(currentPage);
            return -1;
        }
    }

    *((short *) ((char *) currentPage + offset + sizeof(short))) = -1; //delete this record, set the start pos to -1;
    *((short *) ((char *) currentPage + offset)) = 0; //set the length of record to 0;

    //update the directory of slots that are not deleted
    updateSlotDirectory(currentPage, len, start + len);
    //shift the content to the left (free space saved by the deleted record)
    shiftContentToLeft(currentPage, len, start, 0);
    //decrease the slot number by 1
    *((int *)((char *)currentPage + PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE)) = getSlotNumber(currentPage) - 1;
    //update the free space
    *(int *)((char *)currentPage + PAGE_SIZE - FREE_SPACE_SIZE) += len;
    //write back to the file
    fileHandle.writePage(pageNum, currentPage);
    free(currentPage);

    return 0;
}
RC RecordBasedFileManager::updateSlotDirectory(void * currentPage, int len, int end){
    int slotNums = getSlotNumber(currentPage);
    for(int i = 1; i <= slotNums; i++){
        int offset = PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE -
                 i * 2 * sizeof(short);
        //add the slot that comes after 'end' should be updated (mistake: change slot directory according to the RID is wrong)
        if(*((short *) ((char *) currentPage + offset + sizeof(short))) >= end){
            *((short *) ((char *) currentPage + offset + sizeof(short))) = *((short *) ((char *) currentPage + offset + sizeof(short))) - len; //update the start pos of the rest of the record
        }
    }
    return 0;
}
RC RecordBasedFileManager::shiftContentToLeft(void *currentPage, int len, int start, int recordSize){
    //if recordSize is 0, it is used in deletion. Otherwise, it is used in update..-
    int left = len + start; //the left end of the content that needed to be removed
    int right = 0; //the right end of the content that needed to be removed

    int last_offset = PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE - getSlotNumber(currentPage) * 2 * sizeof(short);

    int len2 = *((short *) ((char *) currentPage + last_offset));
    int start2 = *((short *) ((char *) currentPage + last_offset + sizeof(short)));

    right = start2 + len2;
    if(right <= left){
        return 0;
    }
    void *content = malloc(PAGE_SIZE);
    //start...(len)...left...right
    memcpy((char *)content, (char *)currentPage + left, right - left);
    memcpy((char *)currentPage + start + recordSize, content, right - left);
    free(content);

    return 0;
}

RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                      const RID &rid, void *data)
{

    void *currentPage = malloc(PAGE_SIZE);
    fileHandle.readPage(rid.pageNum, currentPage);
    int pos = rid.slotNum;

    int offset = PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE - pos * 2 * sizeof(short); //get the record directory with rid
    short len = *((short *)((char *)currentPage + offset)); //get the length of record
    short start = *((short *)((char *) currentPage + offset + sizeof(short))); //get the start pos of the record
    if(start == -1 && len == 0) { //no such record
        free(currentPage);
        return -1;
    }
    while(len < 0){ //this slot is just a pointer, point to a different page. len: -slotNum, start: -pageNum
        memset(currentPage, 0, PAGE_SIZE);
        fileHandle.readPage(0 - start, currentPage);
        offset = PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE - (0 - len) * 2 * sizeof(short);
        len = *((short *)((char *)currentPage + offset)); //get the length of record
        start = *((short *)((char *) currentPage + offset + sizeof(short))); //get the start pos of the record


        if(start == -1 && len == 0){
            free(currentPage);
            return -1;
        }
    }

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
    free(nullFieldsIndicator);
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
    void *actualContent = (char *)malloc(PAGE_SIZE);

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
    free(nullFieldsIndicator);
    delete []offsetInFormattedData;
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
            std::cout << attr.name <<": NULL" << " ";
        } else {
            void *buffer;
            switch (attr.type) {
                case TypeInt:
                    buffer = malloc(sizeof(int));
                    memcpy(buffer, (char *) data + offset, sizeof(int));
                    offset += sizeof(int);
                    std::cout << attr.name << ": " << (*(int *) buffer) << " ";
                    break;
                case TypeReal:
                    buffer = malloc(sizeof(float));
                    memcpy(buffer, (char *) data + offset, sizeof(float));
                    offset += sizeof(float);
                    std::cout << attr.name << ": " << (*(float *) buffer) << " ";
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
                    std :: cout << " ";
                    break;
            }
            free(buffer);
        }
        if (leftShift == 0) {
            leftShift += 8;
        }
        leftShift--;
    }
    std::cout << std::endl;
    free(nullFieldsIndicator);
    return 0;
}
RC RecordBasedFileManager::updateRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                        const void *data, const RID &rid) {
    int slotNum = rid.slotNum;
    int pageNum = rid.pageNum;
    void* currentPage = malloc(PAGE_SIZE);
    void* record = malloc(PAGE_SIZE); //used to store the type A record;
    fileHandle.readPage(pageNum, currentPage);

    int len_dic = PAGE_SIZE - 2 * sizeof(int) - slotNum * sizeof(short) * 2;
    int start_dic = len_dic + sizeof(short);
    int len = *(short *)((char *)currentPage + len_dic);//length of the record to be updated
    int pos = *(short *)((char *)currentPage + start_dic); //start pos of the record to be updated;

    //transform type B data into type A data
    int recordSize = transformData(recordDescriptor, data, record); //return the transformed type A data's recordSize;
    //case1: type A data fits in place
    if(recordSize <= len){
        //1.update the record in place, 2.shift the rest of the data to the left, 3.update the slot directory, 4.update space
        memcpy((char *)currentPage + pos, record, recordSize); //1. update the record in place
        shiftContentToLeft(currentPage, len, pos, recordSize); //2.shift the res of the date to the left.

        *(short *)((char *)currentPage + len_dic) = recordSize; //3. update the udpated slot directory;
        updateSlotDirectory(currentPage, len - recordSize, len + pos); //update the rest of slot directory

        //4.update space
        *(int *)((char *)currentPage + PAGE_SIZE - sizeof(int)) += len - recordSize;

        //5.write back
        fileHandle.writePage(rid.pageNum, currentPage);
        free(currentPage);
        free(record);
        return 0;
    }
    //recordSize > len
    //case2: type A data fits in the same page, but needs to move its place;
    if(recordSize - len <= getFreeSpaceOfCurrentPage(currentPage)){
        int last_slot_len_dic = PAGE_SIZE - 2 * sizeof(int) - getSlotNumber(currentPage) * sizeof(short) * 2;
        int last_slot_start_dic = last_slot_len_dic + sizeof(short);
        int last_slot_len = *(short *)((char *)currentPage + last_slot_len_dic);
        int last_slot_start_pos = *(short *)((char *)currentPage + last_slot_start_dic);
        //1.shift content to the left(delete the previous one)
        shiftContentToLeft(currentPage, len, pos, 0);
        //2.append the updated record at the end
        memcpy((char *)currentPage + last_slot_start_pos + last_slot_len - len, record, recordSize);

        //3.update the slot directory
        updateSlotDirectory(currentPage, len, len + pos);
        *(short *)((char *)currentPage + len_dic) = recordSize;
        *(short *)((char *)currentPage + start_dic) = last_slot_start_pos + last_slot_len - len;

        //4.update the free space
        *(int *)((char *)currentPage + PAGE_SIZE - sizeof(int)) -= recordSize - len;

        //5.write back
        fileHandle.writePage(rid.pageNum, currentPage);
        free(currentPage);
        free(record);
        return 0;
    }

    //case3: type A data cannot fit in the same page, needs to find another page which has free space;
    //recordSize - len > getFreeSpaceOfCurrentPage(currentPage) the first step is just like deletion, delete this record from this page
    //1.shift content to the left (free space saved by the deleted record)
    shiftContentToLeft(currentPage, len, pos, 0);
    //2.updateSlotDirectory
    updateSlotDirectory(currentPage, len, pos + len);
    //3.decrease the slot number by 1
    *(int *)((char *)currentPage + PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE) = getSlotNumber(currentPage) - 1;
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

    return 0;
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



