#ifndef _ix_h_
#define _ix_h_

#include <vector>
#include <string>

#include "../rbf/rbfm.h"
#include "../rbf/pfm.h"

#define IX_EOF (-1) // end of the index scan

class IX_ScanIterator;

class IXFileHandle;

class IndexManager
{

public:
    static IndexManager &instance();

    // Create an index file.
    RC createFile(const std::string &fileName);

    //add metapage(0), a non leaf page(1), and a leaf page(2)
    RC initialize(const std::string &fileName);

    RC getLeftMostChildOfNonLeafNode(const void *page);

    RC getSlotNum(const void *page);

    RC getFreeSpacePointer(const void *page);

    RC updateFreeSpacePointer(void *page, int offset);

    RC updateSlotNum(void *page);

    RC getNodeType(const void *page);

    RC updateRootPage(IXFileHandle &ixFileHandle, const int pageNum);

    RC getRootPage(IXFileHandle &ixFileHandle);

    // Delete an index file.
    RC destroyFile(const std::string &fileName);

    // Open an index and return an ixFileHandle.
    RC openFile(const std::string &fileName, IXFileHandle &ixFileHandle);

    // Close an ixFileHandle for an index.
    RC closeFile(IXFileHandle &ixFileHandle);

    // Insert an entry into the given index that is indicated by the given ixFileHandle.
    RC insertEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid);

    //recurively insert, used in insertEntry
    RC insert(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid, void *newchildentry, int page_id);

    //choose subtree
    RC getSubtree(const void *page, const Attribute &attribute, const void *key);

    RC findInsertedPosInLeafPage(const void *page, const Attribute &attribute, const void *key);

    RC findInsertedPosInNonLeafPage(const void *page, const Attribute &attribute, const void *key);

    RC getNextPageForLeafNode(const void *page);

    RC getChildPageID(const void *page, const int index);

    RC compare(const void *page, const Attribute &attribute, const void *key, const int index, bool flag);

    RC compareInt(const int insertedKey, const int recordedKey);

    RC compareReal(const float insertedKey, const float recordedKey);

    RC compareString(std::string insertedKey, std::string recordedKey);

    RC insertIntoLeafNodes(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid, void *newChildEntry, void *page, int pageNumber);

    RC insertIntoNonLeafNodes(IXFileHandle &ixFileHandle, const Attribute &attribute, void *newChildEntry, void *page, int pageNumber);

    RC splitNonLeafNodes(IXFileHandle &ixFileHandle, void *page, const Attribute &attribute, void *key, int childPage, void *newChildEntry, int pageNumber);

    RC splitLeafNodes(IXFileHandle &ixFileHandle, void *page, const Attribute &attribute, const void *key, const RID &rid, void *newChildEntry, int pageNumber);

    RC updateLeafDirectory(void *ori, void *des, const int des_pageId);

    RC copyedUpNewChildEntry(void *newChildEntry, const Attribute &attribute, const void *page);

    RC moveNodesToNewPageInLeafNodes(void *ori, void *des, int start_pos, const Attribute &attribute, IXFileHandle &ixFileHandle, const int ori_pageId, const int des_pageId);

    RC moveNodesToNewPageInNonLeafNodes(void *ori, void *des, int start_pos, const Attribute &attribute, IXFileHandle &ixFileHandle, const int ori_pageId, const int des_pageId);

    RC initializeLeafNodes(void *page, IXFileHandle &ixFileHandle);

    RC initializeNonLeafNodes(void *page, IXFileHandle &ixFileHandle);

    RC getKey(const void *page, void *key, int index, const Attribute &attribute, bool flagForUpdate, int &len);

    RC getRID(const void *page, RID &rid, int index);

    RC getFreeSpaceForLeafNodes(const void *page);

    RC getFreeSpaceForNonLeafNodes(const void *page);

    // Delete an entry from the given index that is indicated by the given ixFileHandle.
    RC deleteEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid);

    // Initialize and IX_ScanIterator to support a range search
    RC scan(IXFileHandle &ixFileHandle,
            const Attribute &attribute,
            const void *lowKey,
            const void *highKey,
            bool lowKeyInclusive,
            bool highKeyInclusive,
            IX_ScanIterator &ix_ScanIterator);

    // Print the B+ tree in pre-order (in a JSON record format)
    void printBtree(IXFileHandle &ixFileHandle, const Attribute &attribute);

    void printLeafNodes(void *page, const Attribute &attribute, std::string &space);

    void printLeafKey(const void *page, const Attribute &attribute);

    void printBtree_rec(IXFileHandle &ixfileHandle, std::string &space, int pageNum, const Attribute &attr);

    void printNonLeafNodes(IXFileHandle &ixFileHandle, const void *page, const Attribute &attribute, std::string &space);

    void printNonLeafNodesKey(const void *page, const Attribute &attribute, std::string &space);

    void printNonLeafNodesChild(IXFileHandle &ixFileHandle, const void *page, const Attribute &attribute, std::string &space);

private:
    RecordBasedFileManager *rbfm;

protected:
    IndexManager() = default;                                // Prevent construction
    ~IndexManager() = default;                               // Prevent unwanted destruction
    IndexManager(const IndexManager &) = default;            // Prevent construction by copying
    IndexManager &operator=(const IndexManager &) = default; // Prevent assignment
};

class IX_ScanIterator
{
public:
    // Constructor
    IX_ScanIterator();

    // Destructor
    ~IX_ScanIterator();

    IXFileHandle ixFileHandle;
    Attribute attribute;
    const void *lowKey;
    const void *highKey;
    bool lowKeyInclusive;
    bool highKeyInclusive;

    IndexManager *im;

    void findStartPointForScan();

    void findLeafNodes(IXFileHandle ixFileHandle, void *page, const void *lowKey, bool lowKeyInclusive);

    // Get next matching entry
    RC getNextEntry(RID &rid, void *key);

    // Terminate index scan
    RC close();
};

class IXFileHandle
{
public:
    // variables to keep counter for each operation
    unsigned ixReadPageCounter;
    unsigned ixWritePageCounter;
    unsigned ixAppendPageCounter;

    // Constructor
    IXFileHandle();

    // Destructor
    ~IXFileHandle();

    // Put the current counter values of associated PF FileHandles into variables
    RC collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount);

    RC readPage(PageNum pageNum, void *data);
    RC writePage(PageNum pageNum, const void *data);
    RC appendPage(const void *data);
    unsigned getNumberOfPages();

    FileHandle fileHandle;
};

#endif
