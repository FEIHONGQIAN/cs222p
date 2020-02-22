#include "ix.h"
#include <stdio.h>
#include <string.h>

const int success = 0;
const int fail = -1;
const int LeafNodeType = 0;
const int NonLeafNodeType = 1;
const int newChildNULLIndicatorInitialValue = -3;

IndexManager &IndexManager::instance()
{
    static IndexManager _index_manager = IndexManager();
    return _index_manager;
}

RC IndexManager::createFile(const std::string &fileName)
{
    int rc = rbfm->createFile(fileName);
    if (rc == fail)
        return fail;

    rc = initialize(fileName);
    if (rc == fail)
        return fail;
    return success;
}

RC IndexManager::initialize(const std::string &fileName)
{
    IXFileHandle ixFileHandle;
    int rc = rbfm->openFile(fileName, ixFileHandle.fileHandle);
    if (rc == fail)
        return fail;

    void *metapage = malloc(PAGE_SIZE);

    //Step1: append metapage
    *(int *)((char *)metapage) = 1; // the initial root page number
    rc = ixFileHandle.fileHandle.appendPage(metapage);
    if (rc == fail)
    {
        rbfm->closeFile(ixFileHandle.fileHandle);
        free(metapage);
        return fail;
    }
    memset(metapage, 0, PAGE_SIZE);

    //Step2: append the first non leaf page
    //non leaf page header + ... + slot number + freeSpacePointer + NodeType
    *(int *)((char *)metapage) = 2;                                                         //non leaf page header (used to point at the left most children's page)
    *(int *)((char *)metapage + PAGE_SIZE - sizeof(int)) = NonLeafNodeType;                 //NodeType
    *(int *)((char *)metapage + PAGE_SIZE - 2 * sizeof(int)) = PAGE_SIZE - 3 * sizeof(int); //freeSpacePointer
    *(int *)((char *)metapage + PAGE_SIZE - 3 * sizeof(int)) = 0;                           //slot number
    rc = ixFileHandle.fileHandle.appendPage(metapage);                                      //append first
    if (rc == fail)
    {
        rbfm->closeFile(ixFileHandle.fileHandle);
        free(metapage);
        return fail;
    }
    memset(metapage, 0, PAGE_SIZE);

    //Step3: append the first leaf page
    //... + next page pointer + slot number + freeSpace Pointer + NodeType
    *(int *)((char *)metapage + PAGE_SIZE - sizeof(int)) = LeafNodeType;                    //NodeType
    *(int *)((char *)metapage + PAGE_SIZE - 2 * sizeof(int)) = PAGE_SIZE - 4 * sizeof(int); //freeSpacePointer
    *(int *)((char *)metapage + PAGE_SIZE - 3 * sizeof(int)) = 0;                           //slot number
    *(int *)((char *)metapage + PAGE_SIZE - 4 * sizeof(int)) = -1;                          //point to null
    rc = ixFileHandle.fileHandle.appendPage(metapage);                                      //append first
    if (rc == fail)
    {
        rbfm->closeFile(ixFileHandle.fileHandle);
        free(metapage);
        return fail;
    }

    rbfm->closeFile(ixFileHandle.fileHandle);
    return success;
}

RC IndexManager::getLeftMostChildOfNonLeafNode(const void *page)
{
    return *(int *)((char *)page);
}

RC IndexManager::getSlotNum(const void *page)
{
    return *(int *)((char *)page + PAGE_SIZE - 3 * sizeof(int));
}

RC IndexManager::updateSlotNum(void *page)
{
    *(int *)((char *)page + PAGE_SIZE - 3 * sizeof(int)) = *(int *)((char *)page + PAGE_SIZE - 3 * sizeof(int)) + 1;
    return success;
}

RC IndexManager::getFreeSpacePointer(const void *page)
{
    return *(int *)((char *)page + PAGE_SIZE - 2 * sizeof(int));
}

RC IndexManager::updateFreeSpacePointer(void *page, int offset)
{
    *(int *)((char *)page + PAGE_SIZE - 2 * sizeof(int)) = offset;
    return success;
}

RC IndexManager::getNodeType(const void *page)
{
    return *(int *)((char *)page + PAGE_SIZE - 1 * sizeof(int));
}

RC IndexManager::getNextPageForLeafNode(const void *page)
{
    return *(int *)((char *)page + PAGE_SIZE - 4 * sizeof(int));
}

RC IndexManager::destroyFile(const std::string &fileName)
{
    int rc = rbfm->destroyFile(fileName);
    if (rc == fail)
        return fail;
    return success;
}

RC IndexManager::openFile(const std::string &fileName, IXFileHandle &ixFileHandle)
{
    int rc = rbfm->openFile(fileName, ixFileHandle.fileHandle);
    if (rc == fail)
        return fail;
    return success;
}

RC IndexManager::closeFile(IXFileHandle &ixFileHandle)
{
    int rc = rbfm->closeFile(ixFileHandle.fileHandle);
    if (rc == fail)
        return fail;
    return success;
}

RC IndexManager::insertEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid)
{
    void *newchildentry = malloc(PAGE_SIZE);            //null pointer indicator + child page + key
    *(int *)((char *)newchildentry) = -1;               // null
    *(int *)((char *)newchildentry + sizeof(int)) = -1; //child page

    void *page = malloc(PAGE_SIZE);
    int rc = ixFileHandle.fileHandle.readPage(0, page);

    if (rc == fail)
    {
        //close file handle ?
        free(newchildentry);
        free(page);
        return fail;
    }

    int root_page_num = *(int *)((char *)page);

    rc = insert(ixFileHandle, attribute, key, rid, newchildentry, root_page_num);

    if (rc == fail)
    {
        std::cout << "insert entry failed" << std::endl;
        free(page);
        free(newchildentry);
        return fail;
    }

    free(page);
    free(newchildentry);
    return success;
}

RC IndexManager::insert(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid, void *newchildentry, int page_id)
{
    void *page = malloc(PAGE_SIZE);
    int rc = ixFileHandle.fileHandle.readPage(page_id, page);
    if (rc == fail)
    {
        std::cout << "Unable to read page in insert() function" << std::endl;
        free(page);
        return fail;
    }

    int nodeType = getNodeType(page);
    if (nodeType == NonLeafNodeType)
    {
        int childPageId = getSubtree(page, attribute, key);
        rc = insert(ixFileHandle, attribute, key, rid, newchildentry, childPageId);
        int newchildEntryNullIndicator = newChildNULLIndicatorInitialValue;
        memcpy(&newchildEntryNullIndicator, (char *)newchildentry, sizeof(int));
        if (newchildEntryNullIndicator == newChildNULLIndicatorInitialValue)
        {
            std::cout << "new child entry returned wrong" << std::endl;
            free(page);
            return fail;
        }
        if (newchildEntryNullIndicator == -1)
        {
            free(page);
            return success;
        }
        else
        {
            //            splitNonLeafNode();
        }
    }
    else if (nodeType == LeafNodeType)
    {
        rc = insertIntoLeafNodes(ixFileHandle, attribute, key, rid, newchildentry, page, page_id);
        free(page);
        return rc;
    }
    else
    {
        free(page);
        printf("Get Node Type failed!\n");
        return fail;
    }
}
RC IndexManager::insertIntoLeafNodes(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key,
                                     const RID &rid, void *newChildEntry, void *page, int pageNumber)
{

    int freeSpaceForThisPage = getFreeSpaceForLeafNodes(page);

    int EntryLen = -1;

    if (attribute.type == TypeVarChar)
    {
        memcpy(&EntryLen, key, sizeof(int));
        EntryLen += 3 * sizeof(int) + sizeof(short); // including KeyOffset, rid, and the real varChar
    }
    else
    {
        EntryLen = 2 * sizeof(int) + sizeof(short);
    }

    if (EntryLen == -1)
    {
        std::cout << "the passed in key's length is invalid" << std::endl;
    }

    int k = 2 * sizeof(int) + sizeof(short); //first part of entry
    if (EntryLen <= freeSpaceForThisPage)
    {
        int insertIndex = findInsertedPosInLeafPage(page, attribute, key);
        int insertPos = (insertIndex + 1) * k;

        int entryNum = getSlotNum(page);
        int movedEntryLen = (entryNum - (insertIndex + 1)) * k;
        //First step: move
        memmove((char *)page + insertPos + k, (char *)page + insertPos, movedEntryLen);
        //Second step: insert
        if (attribute.type == TypeInt || attribute.type == TypeReal)
        {
            memcpy((char *)page + insertPos, key, sizeof(int));
            // memcpy((char *)page + insertPos + sizeof(int), &rid, sizeof(rid));
            auto ridPageNum = rid.pageNum;
            auto ridSlotNum = rid.slotNum;
            memcpy((char *)page + insertPos + sizeof(int), &ridPageNum, sizeof(int));
            memcpy((char *)page + insertPos + 2 * sizeof(int), &ridSlotNum, sizeof(short));
        }
        else
        {
            int len = -1;
            memcpy(&len, key, sizeof(int));
            len += sizeof(int);
            int charKeyOffset = getFreeSpacePointer(page) - len;
            memcpy((char *)page + insertPos, &charKeyOffset, sizeof(int));
            // memcpy((char *)page + insertPos + sizeof(int), &rid, sizeof(rid));
            auto ridPageNum = rid.pageNum;
            auto ridSlotNum = rid.slotNum;
            memcpy((char *)page + insertPos + sizeof(int), &ridPageNum, sizeof(int));
            memcpy((char *)page + insertPos + 2 * sizeof(int), &ridSlotNum, sizeof(short));
            memcpy((char *)page + charKeyOffset, key, len);
            //update free space pointer
            updateFreeSpacePointer(page, charKeyOffset);
        }
        updateSlotNum(page);
        int rc = ixFileHandle.fileHandle.writePage(pageNumber, page);
        if (rc == fail)
        {
            std::cout << "write back after insertion into page failed" << std::endl;
            return rc;
        }
        return rc;
    }
    else
    {
        //        split
    }
    return success;
}

RC IndexManager::getFreeSpaceForLeafNodes(const void *page)
{
    int slotNumber = getSlotNum(page);
    int freeSpacePointer = getFreeSpacePointer(page);

    return freeSpacePointer - slotNumber * (2 * sizeof(int) + sizeof(short));
}
RC IndexManager::getSubtree(const void *page, const Attribute &attribute, const void *key)
{
    int res = -1;
    for (int i = 0; i < getSlotNum(page); i++)
    {
        if (compare(page, attribute, key, i, true) >= 0)
        {
            res = i;
            break;
        }
    }

    if (res == -1)
    {
        res = getLeftMostChildOfNonLeafNode(page);
    }
    else
    {
        res = getChildPageID(page, res);
    }
    return res;
}

RC IndexManager::findInsertedPosInLeafPage(const void *page, const Attribute &attribute, const void *key)
{
    int res = -1;
    for (int i = 0; i < getSlotNum(page); i++)
    {
        if (compare(page, attribute, key, i, false) >= 0)
        {
            res = i;
            break;
        }
    }
    return res;
}
RC IndexManager::compare(const void *page, const Attribute &attribute, const void *key, const int index, bool flag)
{
    int start_pos = 0, entry_len = 2 * sizeof(int) + sizeof(short);
    if (flag)
    { //flag : true : compare in non-leaf node, false: compare in leaf node
        start_pos = sizeof(int);
        entry_len = 2 * sizeof(int);
    }
    int type = attribute.type;

    if (type == TypeInt)
    {
        // int insertedKey = *(int *)((char *)key);
        int insertedKey = -1;
        memcpy(&insertedKey, (char *)key, sizeof(int));
        if (insertedKey == -1)
        {
            std::cout << "error in compare function, unable to get the correct INT key value" << std::endl;
        }
        int recordedKey = -1;
        memcpy(&recordedKey, (char *)page + start_pos + index * entry_len, sizeof(int));
        if (recordedKey == -1)
        {
            std::cout << "error in compare function, unable to get the correct INT recordKey value" << std::endl;
        }
        // int recordedKey = *(int *)((char *)page + start_pos + index * entry_len);
        return compareInt(insertedKey, recordedKey);
    }
    if (type == TypeReal)
    {
        // int insertedKey = *(int *)((char *)key);
        // int recordedKey = *(int *)((char *)page + start_pos + index * entry_len);
        float b = -1.0;
        float insertedKey = b;
        memcpy(&insertedKey, (char *)key, sizeof(float));
        if (abs(insertedKey - b) <= 1e-6)
        {
            std::cout << "error in compare function, unable to get the correct FLOAT key value" << std::endl;
        }
        float recordedKey = b;
        memcpy(&recordedKey, (char *)page + start_pos + index * entry_len, sizeof(float));
        if (abs(recordedKey - b) <= 1e-6)
        {
            std::cout << "error in compare function, unable to get the correct FLOAT recordKey value" << std::endl;
        }
        return compareReal(insertedKey, recordedKey);
    }
    if (type == TypeVarChar)
    {
        int insertedKeyLen = -1;
        memcpy(&insertedKeyLen, (char *)key, sizeof(int));
        std::string insertedKey = "";
        for (int i = 0; i < insertedKeyLen; i++)
        {
            insertedKey += *((char *)key + sizeof(int) + i);
        }

        int recordKeyLen = -1;
        int recordKeyOffset = -1;
        memcpy(&recordKeyOffset, ((char *)page + start_pos + index * entry_len), sizeof(int));
        memcpy(&recordKeyLen, (char *)page + recordKeyOffset, sizeof(int));
        std::string recordedKey = "";
        for (int i = 0; i < recordKeyLen; i++)
        {
            recordedKey += *((char *)page + recordKeyOffset + sizeof(int) + i);
        }
        return compareString(insertedKey, recordedKey);
    }
}

RC IndexManager::compareInt(const int insertedKey, const int recordedKey)
{
    return insertedKey - recordedKey;
}

RC IndexManager::compareReal(const float insertedKey, const float recordedKey)
{
    // return insertedKey - recordedKey;
    // since this function return a int, to resolve type convertion between float and int, rewrite to if
    if (insertedKey == recordedKey)
    {
        return 0;
    }
    else if (insertedKey < recordedKey)
    {
        return -1;
    }
    else
    {
        return 1;
    }
}

RC IndexManager::compareString(std::string insertedKey, std::string recordedKey)
{
    if (insertedKey == recordedKey)
    {
        return 0;
    }
    else if (insertedKey < recordedKey)
    {
        return -1;
    }
    else
    {
        return 1;
    }
}

RC IndexManager::getChildPageID(const void *page, int index)
{
    ///////////////////////////////////////
    int offset = (index + 1) * 2 * sizeof(int);

    ////////////////////////////////////////
    int rid = 0;
    memcpy(&rid, (char *)page + offset, sizeof(int));
    // int rid = *(int *)((char *)page + offset);
    if (rid <= 0)
    {
        printf("Child Page ID Error!\n");
    }
    return rid;
}
RC IndexManager::updateRootPage(IXFileHandle &ixFileHandle, const int pageNum)
{
    void *metapage = malloc(PAGE_SIZE);
    int rc = ixFileHandle.fileHandle.readPage(0, metapage);
    if (rc == fail)
    {
        free(metapage);
        return fail;
    }

    *(int *)((char *)metapage) = pageNum;
    rc = ixFileHandle.fileHandle.writePage(0, metapage);
    if (rc == fail)
    {
        free(metapage);
        return fail;
    }
    return success;
}
RC IndexManager::deleteEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid)
{
    return -1;
}

RC IndexManager::scan(IXFileHandle &ixFileHandle,
                      const Attribute &attribute,
                      const void *lowKey,
                      const void *highKey,
                      bool lowKeyInclusive,
                      bool highKeyInclusive,
                      IX_ScanIterator &ix_ScanIterator)
{
    return -1;
}

void IndexManager::printBtree(IXFileHandle &ixFileHandle, const Attribute &attribute) const
{
}

IX_ScanIterator::IX_ScanIterator()
{
}

IX_ScanIterator::~IX_ScanIterator()
{
}

RC IX_ScanIterator::getNextEntry(RID &rid, void *key)
{
    return -1;
}

RC IX_ScanIterator::close()
{
    return -1;
}

IXFileHandle::IXFileHandle()
{
    ixReadPageCounter = 0;
    ixWritePageCounter = 0;
    ixAppendPageCounter = 0;
}

IXFileHandle::~IXFileHandle()
{
}
RC IXFileHandle::readPage(PageNum pageNum, void *data)
{
    return fileHandle.readPage(pageNum, data);
}
RC IXFileHandle::writePage(PageNum pageNum, const void *data)
{
    return fileHandle.writePage(pageNum, data);
}
RC IXFileHandle::appendPage(const void *data)
{
    return fileHandle.appendPage(data);
}

unsigned IXFileHandle::getNumberOfPages()
{
    return fileHandle.getNumberOfPages();
}
RC IXFileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount)
{
    return fileHandle.collectCounterValues(readPageCount, writePageCount, appendPageCount);
}
