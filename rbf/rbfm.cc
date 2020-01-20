#include "rbfm.h"

const int SLOT_NUMBER_SPACE_SIZE = 4;
const int FREE_SPACE_SIZE = 4;
const int RID_OFFSET_SIZE = 2;
const int RID_RECORD_LENGTH = 2;
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
    // return -1;
    if (PagedFileManager::instance().createFile(fileName) != 0)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

RC RecordBasedFileManager::destroyFile(const std::string &fileName)
{
    // return -1;
    if (PagedFileManager::instance().destroyFile(fileName) != 0)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

RC RecordBasedFileManager::openFile(const std::string &fileName, FileHandle &fileHandle)
{
    // return -1;
    if (PagedFileManager::instance().openFile(fileName, fileHandle) != 0)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

RC RecordBasedFileManager::closeFile(FileHandle &fileHandle)
{
    // return -1;
    if (PagedFileManager::instance().closeFile(fileHandle) != 0)
    {
        return -1;
    }
    else
    {
        return 0;
    }
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
        
        free(currentPage);
        return 0;
    }
    fileHandle.readPage(pageCount, currentPage);                // Put the page content to currentPage
    int freeSpace = getFreeSpaceOfCurrentPage(currentPage);     
    int costByThisRecord = recordSize + RID_OFFSET_SIZE + RID_RECORD_LENGTH;
    if (costByThisRecord > freeSpace)  // Current page has no enough space 
    {
        // Append a new page
        UpdateFirstSlots(currentPage, fileHandle, record, recordSize);

        rid.pageNum = pageCount;
        rid.slotNum = getSlotNumber(currentPage);
        free(currentPage);
        // return -1;
    }
    else                // Add the record to the current page
    {

        int offset = PAGE_SIZE - freeSpace - SLOT_NUMBER_SPACE_SIZE - FREE_SPACE_SIZE - getSlotNumber(currentPage) * 2 * sizeof(short);

        UpdateSlots(currentPage, fileHandle, record, offset, recordSize, pageCount);

        rid.pageNum = pageCount;
        rid.slotNum = getSlotNumber(currentPage);
        free(currentPage);
        return 0;
    }
}
;RC RecordBasedFileManager::getFreeSpaceOfCurrentPage(void *currentPage)
{
    return *((int *)currentPage + PAGE_SIZE - FREE_SPACE_SIZE);
}
RC RecordBasedFileManager::UpdateFirstSlots(void *currentPage, FileHandle &fileHandle, void *record, int recordSize)
{
    memcpy(currentPage, record, recordSize);
    *((int *)currentPage + PAGE_SIZE - FREE_SPACE_SIZE) = PAGE_SIZE - recordSize - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE - 2 * sizeof(short);
    *((int *)currentPage + PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE) = 1;
    int insertPos = PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE - 2 * sizeof(short);
    *((short *)currentPage + insertPos) = (short)recordSize;
    *((short *)currentPage + insertPos + sizeof(short)) = (short)0;
    fileHandle.appendPage(currentPage);
}

RC RecordBasedFileManager::UpdateSlots(void *currentPage, FileHandle &fileHandle, void *record, int offset, int recordSize, int pageCount)
{
    memcpy((char *)currentPage + offset, record, recordSize);
    *((int *)currentPage + PAGE_SIZE - FREE_SPACE_SIZE) = getFreeSpaceOfCurrentPage(currentPage) - recordSize;
    *((int *)currentPage + PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE) = getSlotNumber(currentPage) + 1;
    int insertPos = PAGE_SIZE - SLOT_NUMBER_SPACE_SIZE - FREE_SPACE_SIZE - getSlotNumber(currentPage) * 2 * sizeof(short) - 2 * sizeof(short);
    *((short *)currentPage + insertPos) = (short)recordSize;
    *((short *)currentPage + insertPos + sizeof(short)) = (short)offset;

    int rc = fileHandle.writePage(pageCount, currentPage);
    return 0;
}

RC RecordBasedFileManager::getSlotNumber(void *currentPage)
{
    return *((int *)currentPage + PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE);
}

RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                      const RID &rid, void *data)
{
    // return -1;

    void *currentPage = malloc(PAGE_SIZE);
    fileHandle.readPage(rid.pageNum, currentPage);
    int pos = rid.slotNum;

    int offset = PAGE_SIZE - FREE_SPACE_SIZE - SLOT_NUMBER_SPACE_SIZE - pos * 2 * sizeof(short);

    short len = *((short *)currentPage + offset);
    short start = *((short *) currentPage + offset + sizeof(short));

    void *contentOfRecords = malloc(PAGE_SIZE);
    memcpy(contentOfRecords, (char *)currentPage + start, len);
    prepareRecord(data,  recordDescriptor, contentOfRecords, len);

    return 0;
}

void RecordBasedFileManager::prepareRecord(void *buffer, const std::vector<Attribute> &recordDescriptor, void *contentOfRecords, int len) {
 
    int fieldCount = recordDescriptor.size();
    auto *nullFieldsIndicator = (unsigned char *) malloc(ceil((double) fieldCount / 8));
    memset(nullFieldsIndicator, 0, ceil((double) fieldCount / 8));


    // Beginning of the actual data
    // Note that the left-most bit represents the first field. Thus, the offset is 7 from right, not 0.
    // e.g., if a record consists of four fields and they are all nulls, then the bit representation will be: [11110000]

    // Is the name field not-NULL?
    int i = sizeof(short);
    std::vector<short> offsetOfField;
    for (int j = 0; j < fieldCount; j++) {
        offsetOfField.push_back(*((short *) contentOfRecords + i));
        i += 2;
    }
    int startAddress = (offsetOfField.size() + 1) * sizeof(short);
    for (int i = 0; i < fieldCount; i++) {
        if (offsetOfField[i] < 0) {
            *(nullFieldsIndicator + i / 8) |= (unsigned) 1 << (unsigned) (7 - (i % 8));
        }
    }

    int offset = 0;
    // Null-indicators
    *(int *)((char *)buffer + offset) = fieldCount;
    offset += sizeof(int); 
    // Null-indicator for the fields
    memcpy((char *) buffer + offset, nullFieldsIndicator, ceil((double) fieldCount / 8));
    offset += ceil((double) fieldCount / 8);

    // void *recordAddress = malloc(PAGE_SIZE);
    // int recordOffset = 0;
    for (int i = 0; i < fieldCount; i++) {
        if (offsetOfField[i] == -1) {

        }
        else {
            switch (recordDescriptor[i].type)
            {
            case TypeInt:
                memcpy((int *)(char *)buffer + offset, (int *)(char *)contentOfRecords + startAddress, offsetOfField[i] - startAddress);
                offset += sizeof(int);
                startAddress = offsetOfField[i];
                break;
            case TypeReal:
                memcpy((float *)(char *)buffer + offset, (float *)(char *)contentOfRecords + startAddress, offsetOfField[i] - startAddress);
                offset += sizeof(float);
                startAddress = offsetOfField[i];
                break;
            case TypeVarChar:
                *((int *) (char *) buffer + offset) = offsetOfField[i] - startAddress;
                offset += sizeof(int);
                memcpy((char *) buffer + offset, (char *)contentOfRecords + startAddress, offsetOfField[i] - startAddress );
                offset += offsetOfField[i] - startAddress;
                startAddress = offsetOfField[i];
                break;
            default:
                break;
            }

        }
    }

    


}
RC RecordBasedFileManager::deleteRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                        const RID &rid)
{
    return -1;
}
RC RecordBasedFileManager::getActualByteForNullsIndicator(int fieldCount)
{
    return ceil((double)fieldCount / CHAR_BIT);
}
RC RecordBasedFileManager::printRecord(const std::vector<Attribute> &recordDescriptor, const void *data)
{
    // return -1;
    bool nullBit = false;

    int fieldCount = recordDescriptor.size();
    int nullFieldsIndicatorActualSize = getActualByteForNullsIndicator(fieldCount);

    int offset = 0;

    auto *nullFieldsIndicator = (unsigned char *)malloc(sizeof(char) * nullFieldsIndicatorActualSize);
    memset(nullFieldsIndicator, 0, nullFieldsIndicatorActualSize);
    memcpy((char *)nullFieldsIndicator, data, nullFieldsIndicatorActualSize);
    offset += nullFieldsIndicatorActualSize;

    int leftShift = 7;
    // std::cout << "gogo" << std::endl;
    for (int i = 0; i < recordDescriptor.size(); i++)
    {
        Attribute attr = recordDescriptor[i];

        if (nullFieldsIndicator[i / 8] & ((unsigned)1 << (unsigned)leftShift))
        {
            std::cout << attr.name << ": NULL"
                      << "\t";
        }
        else
        {
            void *buffer;
            switch (attr.type)
            {
            case TypeInt:

                buffer = malloc(sizeof(int));
                memcpy(buffer, (int *)data + offset, sizeof(int));
                offset += sizeof(int);
                std::cout << attr.name << ": " << (*(int *)buffer) << "\t";
                break;
            case TypeReal:
                buffer = malloc(sizeof(float));
                memcpy(buffer, (float *)data + offset, sizeof(float));
                offset += sizeof(float);
                std::cout << attr.name << ": " << (*(float *)buffer) << "\t";
                break;
            case TypeVarChar:
                buffer = malloc(sizeof(int));
                memcpy(buffer, (int *)data + offset, sizeof(int));
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
                    std ::cout << *tempPointer;
                    tempPointer++;
                }
                std ::cout << "\t";
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
    return 0;
}
RC RecordBasedFileManager::updateRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                        const void *data, const RID &rid)
{
    return -1;
}

RC RecordBasedFileManager::readAttribute(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                         const RID &rid, const std::string &attributeName, void *data)
{
    return -1;
}

RC RecordBasedFileManager::scan(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                const std::string &conditionAttribute, const CompOp compOp, const void *value,
                                const std::vector<std::string> &attributeNames, RBFM_ScanIterator &rbfm_ScanIterator)
{
    return -1;
}
