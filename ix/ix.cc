#include "ix.h"
#include <stdio.h>
#include <string.h>

const int success = 0;
const int fail = -1;
const int LeafNodeType = 0;
const int NonLeafNodeType = 1;
const int newChildNULLIndicatorInitialValue = -3;
const int entryLenInLeafNodes = sizeof(int) * 2 + sizeof(short);
const int entryLenInNonLeafNodes = sizeof(int) * 2;

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
    rc = initializeLeafNodes(metapage, ixFileHandle);
    if(rc == fail){
        free(metapage);
        rbfm -> closeFile(ixFileHandle.fileHandle);
        return fail;
    }

    free(metapage);
    rbfm -> closeFile(ixFileHandle.fileHandle);
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
            //insertIntoNonLeafNodes
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
RC IndexManager::insertIntoNonLeafNodes(IXFileHandle &ixFileHandle, const Attribute &attribute, void *newChildEntry, void *page, int pageNumber) {
    int freeSpaceForThisPage = getFreeSpaceForNonLeafNodes(page);
    int EntryLen = -1;
    if(attribute.type == TypeVarChar){
        memcpy(&EntryLen, (char *)newChildEntry + 2 * sizeof(int), sizeof(int));
        EntryLen += 3 * sizeof(int); //keyOffset, pageNum, the real varcChar
    }else{
        EntryLen = 2 * sizeof(int);
    }

    if(EntryLen == -1){
        std::cout << "the passed in key's length is invalid" << std::endl;
    }

    int k = 2 * sizeof(int); //first part of entry
    if (EntryLen <= freeSpaceForThisPage)
    {
        int insertIndex = findInsertedPosInNonLeafPage(page, attribute, (char *)newChildEntry + 2 * sizeof(int));
        int insertPos = (insertIndex + 1) * k + sizeof(int);

        int entryNum = getSlotNum(page);
        int movedEntryLen = (entryNum - (insertIndex + 1)) * k;
        //First step: move
        memmove((char *)page + insertPos + k, (char *)page + insertPos, movedEntryLen);
        //Second step: insert
        if (attribute.type == TypeInt || attribute.type == TypeReal)
        {
            //copy key from newChildEntry
            memcpy((char *)page + insertPos, (char *)newChildEntry + 2 * sizeof(int), sizeof(int));
            //copy childpage from newChildEntry
            memcpy((char *) page + insertPos + sizeof(int), (char *)newChildEntry + sizeof(int), sizeof(int));
        }
        else
        {
            int len = -1;
            memcpy(&len, (char *)newChildEntry + 2 * sizeof(int), sizeof(int));
            len += sizeof(int);
            int charKeyOffset = getFreeSpacePointer(page) - len;
            memcpy((char *)page + insertPos, &charKeyOffset, sizeof(int));
            memcpy((char *) page + insertPos + sizeof(int), (char *)newChildEntry + sizeof(int), sizeof(int));

            memcpy((char *)page + charKeyOffset, (char *)newChildEntry + 2 * sizeof(int), len);
            //update free space pointer
            updateFreeSpacePointer(page, charKeyOffset);
        }
        updateSlotNum(page);
        *(int *)((char *)newChildEntry) = -1; //set new child entry to null;

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
        void * key = malloc(PAGE_SIZE);
        memcpy((char *)key, (char *)newChildEntry + 2 * sizeof(int), PAGE_SIZE - 2 * sizeof(int));
        int childPage = *(int *)((char *)newChildEntry + sizeof(int));
        int rc = splitNonLeafNodes(ixFileHandle, page, attribute, key, childPage, newChildEntry, pageNumber);
        if(rc == fail){
            free(key);
            std::cout << "split leaf nodes failed" << std::endl;
            return rc;
        }

        free(key);
        return rc;
    }
}

RC IndexManager::splitNonLeafNodes(IXFileHandle &ixFileHandle, void *page, const Attribute &attribute, void * key, int childPage, void *newChildEntry, int pageNumber) {
    int slotNum = getSlotNum(page);
    int mid = slotNum / 2;
    int n = 0; //will not be used
    void * midKey = malloc(PAGE_SIZE);
    void * newPage = malloc(PAGE_SIZE);
    void *dummyEntry = malloc(PAGE_SIZE);            //null pointer indicator + child page + key
    *(int *)((char *)dummyEntry) = -1;               // null
    *(int *)((char *)dummyEntry + sizeof(int)) = -1; //child page


    initializeNonLeafNodes(newPage, ixFileHandle);
    int newPageId = ixFileHandle.fileHandle.getNumberOfPages() - 1;
    int rc = -1;

    //////////////
    //get real mid point of the page
    if(attribute.type == TypeVarChar){
        int halfLen = 0;
        void * key = malloc(PAGE_SIZE);
        for(int i = 0; i < slotNum; i++){
            getKey(page, key, i, attribute, false, n);
            int keyLen = *(int *)((char *)key) + sizeof(int); //real key len
            keyLen += sizeof(int) * 2; //charkeyoffset + pageNum
            if(halfLen + keyLen <= (PAGE_SIZE - 4 * sizeof(int)) / 2){
                halfLen += keyLen;
                mid = i;
            }else{
                break;
            }
        }
        free(key);
    }
    /////////////

    getKey(page, midKey, mid, attribute, false, n);

    //newchildentry set to mid key
    *(int *)((char *)newChildEntry) = 0; //null point indicator
    *(int *)((char *)newChildEntry + sizeof(int)) = newPageId; //child page should be the new page
    int len = -1;
    if(attribute.type == TypeInt || attribute.type == TypeReal){
        len = sizeof(int);
    }else{
        len = *(int *)((char *)midKey) + sizeof(int);
    }
    memcpy((char *)newChildEntry + 2 * sizeof(int), (char *)midKey, len); //copy mid key

    int val = compare(page, attribute, midKey, mid, true);

    //move mid + 1 nodes to new page
    moveNodesToNewPageInNonLeafNodes(page, newPage, mid + 1, attribute, ixFileHandle, pageNumber, newPageId);

    //update new page's left most pointer
    int offset = (mid + 1) * 2 * sizeof(int);
    int childpage = *(int *)((char *)page + offset);
    *(int *)((char *)newPage) = childpage;

    if(val > 0){
        insertIntoNonLeafNodes(ixFileHandle, attribute, dummyEntry, newPage, newPageId);
    }else if(val < 0){
        insertIntoNonLeafNodes(ixFileHandle, attribute, dummyEntry, page, pageNumber);
    }else{
        printf("same key in non leaf node in splitNonLeafNodes");
        free(midKey);
        free(newPage);
        free(dummyEntry);
        return fail;
    }

    if(pageNumber == getRootPage(ixFileHandle)){
        void * rootpage = malloc(PAGE_SIZE);
        *(int *)((char *)rootpage) = pageNumber;                                                //non leaf page header (used to point at the left most children's page)
        *(int *)((char *)rootpage + PAGE_SIZE - sizeof(int)) = NonLeafNodeType;                 //NodeType
        *(int *)((char *)rootpage + PAGE_SIZE - 2 * sizeof(int)) = PAGE_SIZE - 3 * sizeof(int); //freeSpacePointer
        *(int *)((char *)rootpage + PAGE_SIZE - 3 * sizeof(int)) = 0;                           //slot number
        rc = ixFileHandle.fileHandle.appendPage(rootpage);                                      //append first
        int newRootPageId = ixFileHandle.fileHandle.getNumberOfPages() - 1;
        if (rc == fail)
        {
            free(rootpage);
            free(midKey);
            free(newPage);
            free(dummyEntry);
            return fail;
        }else{
            insertIntoNonLeafNodes(ixFileHandle, attribute, newChildEntry, rootpage, newRootPageId);
            updateRootPage(ixFileHandle, newRootPageId);
            free(rootpage);
        }
    }

    free(midKey);
    free(newPage);
    free(dummyEntry);
    return success;
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
        int rc = splitLeafNodes(ixFileHandle, page, attribute, key, rid, newChildEntry, pageNumber);
        if(rc == fail){
            std::cout << "split leaf nodes failed" << std::endl;
            return rc;
        }
        return rc;
    }
}

RC IndexManager::splitLeafNodes(IXFileHandle &ixFileHandle, void *page, const Attribute &attribute, const void * key, const RID &rid, void *newChildEntry, int pageNumber) {
    int slotNum = getSlotNum(page);
    int mid = slotNum / 2;
    int n = 0; //will not be used
    void * midKey = malloc(PAGE_SIZE);
    void * newPage = malloc(PAGE_SIZE);
    void *dummyEntry = malloc(PAGE_SIZE);            //null pointer indicator + child page + key
    *(int *)((char *)dummyEntry) = -1;               // null
    *(int *)((char *)dummyEntry + sizeof(int)) = -1; //child page


    initializeLeafNodes(newPage, ixFileHandle);
    int newPageId = ixFileHandle.fileHandle.getNumberOfPages() - 1;
    int rc = -1;

    getKey(page, midKey, mid, attribute, false, n);

    int right = mid;
    for(; right < slotNum; right++){
        if(compare(page, attribute, midKey, right, false) != 0){
            break;
        }
    }

    int left = mid - 1;
    for(; left >= 0; left--){
        if(compare(page, attribute, midKey, left, false) != 0){
            break;
        }
    }
    if(right == slotNum && left == -1) {
        int val = compare(page, attribute, key, mid, false);
        if(val > 0){ //当前page 不动， key插入到newPage里
            rc = insertIntoLeafNodes(ixFileHandle, attribute, key, rid, dummyEntry, newPage, newPageId);
            if(rc == fail){
                free(dummyEntry);
                free(midKey);
                free(newPage);
                printf("splitLeafNodes failed! val > 0\n");
                return fail;
            }
        }else if(val < 0){ //当前page内容移动到newPage里，key插入到当前page
            moveNodesToNewPageInLeafNodes(page, newPage, 0, attribute, ixFileHandle, pageNumber, newPageId);
            insertIntoLeafNodes(ixFileHandle, attribute, key, rid, dummyEntry, page, pageNumber);
            if(rc == fail){
                free(dummyEntry);
                free(midKey);
                free(newPage);
                printf("splitLeafNodes failed! val < 0\n");
                return fail;
            }
        }else if(val == 0){ //去死吧

        }
        //update leaf directory
        updateLeafDirectory(page, newPage, newPageId);
        //copied up new child entry
        copyedUpNewChildEntry(newChildEntry, attribute, newPage);
        //write back
        ixFileHandle.writePage(pageNumber, page);
        ixFileHandle.writePage(newPageId, newPage);

        free(dummyEntry);
        free(midKey);
        free(newPage);
        return success;
    }

    if(right == slotNum && left != -1){
        //left + 1 全部移到新的page
        moveNodesToNewPageInLeafNodes(page, newPage, left + 1, attribute, ixFileHandle, pageNumber, newPageId);

        int val = compare(newPage, attribute, key, 0, false);
        if(val >= 0){ //key 插入新的page
            insertIntoLeafNodes(ixFileHandle, attribute, key, rid, dummyEntry, newPage, newPageId);
        }else{ //key 插入到原来的page
            insertIntoLeafNodes(ixFileHandle, attribute, key, rid, dummyEntry, page, pageNumber);
        }

        updateLeafDirectory(page, newPage, newPageId);
        copyedUpNewChildEntry(newChildEntry, attribute, newPage);
        ixFileHandle.writePage(pageNumber, page);
        ixFileHandle.writePage(newPageId, newPage);

        free(dummyEntry);
        free(midKey);
        free(newPage);
        return success;
    }

    if(right != slotNum && left == -1){
        //right 全部移到新的page
        moveNodesToNewPageInLeafNodes(page, newPage, right, attribute, ixFileHandle, pageNumber, newPageId);

        int val = compare(newPage, attribute, key, 0, false);
        if(val >= 0){ //key 插入新的page
            insertIntoLeafNodes(ixFileHandle, attribute, key, rid, dummyEntry, newPage, newPageId);
        }else{ //插入到原来的page
            insertIntoLeafNodes(ixFileHandle, attribute, key, rid, dummyEntry, page, pageNumber);
        }

        updateLeafDirectory(page, newPage, newPageId);
        copyedUpNewChildEntry(newChildEntry, attribute, newPage);
        ixFileHandle.writePage(pageNumber, page);
        ixFileHandle.writePage(newPageId, newPage);

        free(dummyEntry);
        free(midKey);
        free(newPage);
        return success;
    }

    //right != slotNum && left != -1
    if(right - mid < mid - left){
        //right 全部移到新的page
        moveNodesToNewPageInLeafNodes(page, newPage, right, attribute, ixFileHandle, pageNumber, newPageId);

        int val = compare(newPage, attribute, key, 0, false);
        if(val >= 0){ //key 插入新的page
            insertIntoLeafNodes(ixFileHandle, attribute, key, rid, dummyEntry, newPage, newPageId);
        }else{ //插入到原来的page
            insertIntoLeafNodes(ixFileHandle, attribute, key, rid, dummyEntry, page, pageNumber);
        }

        updateLeafDirectory(page, newPage, newPageId);
        copyedUpNewChildEntry(newChildEntry, attribute, newPage);
        ixFileHandle.writePage(pageNumber, page);
        ixFileHandle.writePage(newPageId, newPage);

        free(dummyEntry);
        free(midKey);
        free(newPage);
        return success;
    }
    //right - mid >= mid - left
    //left + 1 全部移到新的page
    moveNodesToNewPageInLeafNodes(page, newPage, left + 1, attribute, ixFileHandle, pageNumber, newPageId);

    int val = compare(newPage, attribute, key, 0, false);
    if(val >= 0){ //key 插入新的page
        insertIntoLeafNodes(ixFileHandle, attribute, key, rid, dummyEntry, newPage, newPageId);
    }else{ //key 插入到原来的page
        insertIntoLeafNodes(ixFileHandle, attribute, key, rid, dummyEntry, page, pageNumber);
    }

    updateLeafDirectory(page, newPage, newPageId);
    copyedUpNewChildEntry(newChildEntry, attribute, newPage);
    ixFileHandle.writePage(pageNumber, page);
    ixFileHandle.writePage(newPageId, newPage);

    free(dummyEntry);
    free(midKey);
    free(newPage);
    return success;
}
RC IndexManager::moveNodesToNewPageInNonLeafNodes(void *ori, void *des, int start_pos, const Attribute &attribute, IXFileHandle &ixFileHandle, const int ori_pageId, const int des_pageId) {
    int slotNum = getSlotNum(ori);
    void * key = malloc(PAGE_SIZE);
    int childPageNum = -1;
    int len = 0;
    void *dummyEntry = malloc(PAGE_SIZE);            //null pointer indicator + child page + key
    *(int *)((char *)dummyEntry) = 0;               // 有
    *(int *)((char *)dummyEntry + sizeof(int)) = -1; //child page

    void * tempPage = malloc(PAGE_SIZE);
    memcpy((char *)tempPage, (char *)ori, PAGE_SIZE);
    memset(ori, 0, PAGE_SIZE);
    memcpy((char *)ori, (char *)tempPage, sizeof(int));
    *(int *)((char *)ori + PAGE_SIZE - 3 * sizeof(int)) = 0;
    *(int *)((char *)ori + PAGE_SIZE - 2 * sizeof(int)) = PAGE_SIZE - 3 * sizeof(int);
    memcpy((char *)ori + PAGE_SIZE - 1 * sizeof(int), (char *)tempPage + PAGE_SIZE - 1 * sizeof(int), sizeof(int));
    for(int i = 0; i < start_pos - 1; i++){
        //getKey
        getKey(tempPage, key, i, attribute, true, len);
        //getchildPageNum
        int offset = (i + 1) * 2 * sizeof(int);
        childPageNum = *(int *)((char *)tempPage + offset);
        if(childPageNum < 0){
            printf("get child page num failed in moveNodesToNewPageInNonLeafNodes!\n");
            free(key);
            free(dummyEntry);
            free(tempPage);
            return fail;
        }
        *(int *)((char *)dummyEntry + sizeof(int)) = childPageNum;

        int length_of_key = 0;
        if(attribute.type == TypeReal || attribute.type == TypeInt){
            length_of_key = sizeof(int);
        }else{
            length_of_key = sizeof(int) + *(int *)((char *)key);
        }
        memcpy((char *)dummyEntry + 2 * sizeof(int), (char *)key, length_of_key);
        insertIntoNonLeafNodes(ixFileHandle, attribute, dummyEntry, ori, ori_pageId);
    }

    for(int i = start_pos; i < slotNum; i++){
        //getKey
        getKey(tempPage, key, i, attribute, true, len);
        //getchildPageNum
        int offset = (i + 1) * 2 * sizeof(int);
        childPageNum = *(int *)((char *)tempPage + offset);
        if(childPageNum < 0){
            printf("get child page num failed in moveNodesToNewPageInNonLeafNodes!\n");
            free(key);
            free(tempPage);
            free(dummyEntry);
            return fail;
        }
        *(int *)((char *)dummyEntry + sizeof(int)) = childPageNum;

        int length_of_key = 0;
        if(attribute.type == TypeReal || attribute.type == TypeInt){
            length_of_key = sizeof(int);
        }else{
            length_of_key = sizeof(int) + *(int *)((char *)key);
        }
        memcpy((char *)dummyEntry + 2 * sizeof(int), (char *)key, length_of_key);
        insertIntoNonLeafNodes(ixFileHandle, attribute, dummyEntry, des, des_pageId);
    }

    free(key);
    free(tempPage);
    free(dummyEntry);
    return success;
}

RC IndexManager::moveNodesToNewPageInLeafNodes(void *ori, void *des, int start_pos, const Attribute &attribute, IXFileHandle &ixFileHandle, const int ori_pageId, const int des_pageId) {
    int slotNum = getSlotNum(ori);
    void * key = malloc(PAGE_SIZE);
    RID rid;
    int len = 0;

    void * tempPage = malloc(PAGE_SIZE);
    memcpy((char *)tempPage, (char *)ori, PAGE_SIZE);
    memset(ori, 0, PAGE_SIZE);
    memcpy((char *)ori + PAGE_SIZE - 4 * sizeof(int), (char *)tempPage + PAGE_SIZE - 4 * sizeof(int), sizeof(int));
    *(int *)((char *)ori + PAGE_SIZE - 3 * sizeof(int)) = 0;
    *(int *)((char *)ori + PAGE_SIZE - 2 * sizeof(int)) = PAGE_SIZE - 4 * sizeof(int);
    memcpy((char *)ori + PAGE_SIZE - 1 * sizeof(int), (char *)tempPage + PAGE_SIZE - 1 * sizeof(int), sizeof(int));

    void *dummyEntry = malloc(PAGE_SIZE);            //null pointer indicator + child page + key
    *(int *)((char *)dummyEntry) = -1;               // null
    *(int *)((char *)dummyEntry + sizeof(int)) = -1; //child page

    for(int i = 0; i < start_pos; i++){
        getKey(tempPage, key, i, attribute, true, len);
        getRID(tempPage, rid, i);
        insertIntoLeafNodes(ixFileHandle, attribute, key, rid, dummyEntry, ori, ori_pageId);
    }
    for(int i = start_pos; i < slotNum; i++){
        getKey(tempPage, key, i, attribute, true, len);
        getRID(tempPage, rid, i);
        insertIntoLeafNodes(ixFileHandle, attribute, key, rid, dummyEntry, des, des_pageId);
    }

    free(key);
    free(tempPage);
    free(dummyEntry);
    return success;
}
RC IndexManager::copyedUpNewChildEntry(void *newChildEntry, const Attribute &attribute, const void *page) {
    *(int *)((char *)newChildEntry) = 0; //not null

    int child_page = getNextPageForLeafNode(page);
    if(child_page < 0){
        printf("copedUpNewChildEntry failed!\n");
        return fail;
    }
    *(int *)((char *)newChildEntry + sizeof(int)) = getNextPageForLeafNode(page);

    if(attribute.type == TypeInt || attribute.type == TypeReal){
        memcpy((char *)newChildEntry + 2 * sizeof(int), (char *) page, sizeof(int));
    }else{
        int charKeyOffset = *(int *)((char *)page);
        int len = *(int *)((char *)page + charKeyOffset);
        len += sizeof(int);
        memcpy((char *)newChildEntry + 2 * sizeof(int), (char *)page + charKeyOffset, len);
    }

    return success;
}
RC IndexManager::updateLeafDirectory(void * ori, void * des, const int des_pageId){
    int ori_pointer = *(int *)((char *) ori + PAGE_SIZE - 4 * sizeof(int));
    if(ori_pointer == -1){
        *(int *)((char *) ori + PAGE_SIZE - 4 * sizeof(int)) = des_pageId;
    }else{
        *(int *)((char *) ori + PAGE_SIZE - 4 * sizeof(int)) = des_pageId;
        *(int *)((char *) des + PAGE_SIZE - 4 * sizeof(int)) = ori_pointer;
    }
    return success;
}
RC IndexManager::getKey(const void *page, void *key, int index, const Attribute &attribute, bool flagForUpdate, int &len) {
    int nodeType = getNodeType(page);
    int offset = -1;
    if(nodeType == LeafNodeType){
        offset = index * entryLenInLeafNodes;
    }else{
        offset = sizeof(int) + index * entryLenInNonLeafNodes;
    }

    if(attribute.type == TypeInt){
        memcpy((char *) key, (char *) page + offset, sizeof(int));
    }else if(attribute.type == TypeReal){
        memcpy((char *) key, (char *)page + offset, sizeof(int));
    }else{
        int charKeyOffset = *(int *)((char *)page + offset);
        int length = *(int *)((char *)page + charKeyOffset);
        length += sizeof(int);
        if(flagForUpdate){
            len += length;
        }
        memcpy((char *) key, (char *) page + charKeyOffset, length);
    }
    return success;
}

RC IndexManager::getRID(const void *page, RID &rid, int index) {
    int offset = (index) * entryLenInLeafNodes + sizeof(int);
    rid.pageNum = *(int *)((char *) page + offset);
    rid.slotNum = *(short *)((char *) page + offset + sizeof(int));
    if(rid.pageNum < 0 || rid.slotNum < 0){
        printf("get RID failed!\n");
        return fail;
    }
    return success;
}

RC IndexManager::getFreeSpaceForLeafNodes(const void *page)
{
    int slotNumber = getSlotNum(page);
    int freeSpacePointer = getFreeSpacePointer(page);

    return freeSpacePointer - slotNumber * (2 * sizeof(int) + sizeof(short));
}

RC IndexManager::getFreeSpaceForNonLeafNodes(const void *page)
{
    int slotNumber = getSlotNum(page);
    int freeSpacePointer = getFreeSpacePointer(page);

    return freeSpacePointer - slotNumber * 2 * sizeof(int) - sizeof(int);
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

RC IndexManager::findInsertedPosInNonLeafPage(const void *page, const Attribute &attribute, const void *key) {
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

RC IndexManager::getRootPage(IXFileHandle &ixFileHandle) {
    void *metapage = malloc(PAGE_SIZE);
    int rc = ixFileHandle.fileHandle.readPage(0, metapage);
    if (rc == fail)
    {
        free(metapage);
        return fail;
    }

    int pageNum = *(int *)((char *)metapage);
    if(pageNum < 0){
        free(metapage);
        printf("get root page fail");
        return fail;
    }

    free(metapage);
    return pageNum;
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

RC IndexManager::initializeLeafNodes(void *page, IXFileHandle &ixFileHandle) {
    *(int *)((char *)page + PAGE_SIZE - sizeof(int)) = LeafNodeType;                    //NodeType
    *(int *)((char *)page + PAGE_SIZE - 2 * sizeof(int)) = PAGE_SIZE - 4 * sizeof(int); //freeSpacePointer
    *(int *)((char *)page + PAGE_SIZE - 3 * sizeof(int)) = 0;                           //slot number
    *(int *)((char *)page + PAGE_SIZE - 4 * sizeof(int)) = -1;                          //point to null
    int rc = ixFileHandle.fileHandle.appendPage(page);                                      //append first

    return rc;
}

RC IndexManager::initializeNonLeafNodes(void *page, IXFileHandle &ixFileHandle) {
    *(int *)((char *)page) = -1; //left most child page number
    *(int *)((char *)page + PAGE_SIZE - sizeof(int)) = NonLeafNodeType;                    //NodeType
    *(int *)((char *)page + PAGE_SIZE - 2 * sizeof(int)) = PAGE_SIZE - 3 * sizeof(int); //freeSpacePointer
    *(int *)((char *)page + PAGE_SIZE - 3 * sizeof(int)) = 0;                           //slot number
    int rc = ixFileHandle.fileHandle.appendPage(page);                                      //append first

    return rc;
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
void IndexManager::printLeafKey(const void *page, const Attribute &attribute) {
    std::cout << "\"";
    if (attribute.type == TypeInt ) {
        int keyVal = -1;
        memcpy(&keyVal, page, sizeof(int));
        std::cout << keyVal << ":[";
    }
    else if (attribute.type == TypeReal) {
        float keyVal = -1.0;
        memcpy(&keyVal, page, sizeof(float));
        std::cout << keyVal << ":[";
    }
    else {
        int keyLen = -1;
        memcpy(&keyLen, page, sizeof(int));
        std::string keyVal = "";
        for (int i = 0; i < keyLen; i++) {
            keyVal += *((char *)page + sizeof(int) + i);
        }
        std::cout << keyVal << ":[";
    }
    return ;
}
void IndexManager::printLeafNodes(const void *page, const Attribute &attribute)  {
    int slotNum = getSlotNum(page);

    std::vector<RID> ridVec;
    ridVec.clear();
    RID rid;
    int len = 0;
    void *key = malloc(PAGE_SIZE);
    // Temp is used to store the key for last iteration
    void *prev = malloc(PAGE_SIZE);
    std::cout << "{\"keys\": [";

    for (int i = 0; i < slotNum; i++) {
        getKey(page, key, i, attribute, true, len);
        // Initialize temp for the first iteration
        if (i == 0) {
            memcpy((char *)prev, (char *)key, PAGE_SIZE);
        }
        // if the current key is the same as the last iteration, push this RID into the vector
        if (memcmp(page, key, PAGE_SIZE) == 0) {
            getRID(page, rid, i);
            ridVec.push_back(rid);
        }
        // pop all the rid in vector to the console
        // Copy key to temp for next comparison
        else {
            printLeafKey(prev, attribute);
            for (auto item : ridVec) {
               std:: cout <<"(" << item.pageNum << "," << item.slotNum << "), ";
            }
            std::cout << "]\",";
            ridVec.clear();
            memcpy((char *)prev, (char *)key, PAGE_SIZE);
        }
    }
    printLeafKey(key, attribute);
    for (auto item : ridVec) {
        std::cout <<"(" << item.pageNum << "," << item.slotNum << "), ";
    }
    std::cout << "]\",";
    std::cout << "]}," << std::endl;

    free(key);
    free(prev);
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
