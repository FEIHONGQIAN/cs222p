#include "ix.h"

const int success = 0;
const int fail = -1;

IndexManager &IndexManager::instance() {
    static IndexManager _index_manager = IndexManager();
    return _index_manager;
}

RC IndexManager::createFile(const std::string &fileName) {
    int rc = rbfm -> createFile(fileName);
    if(rc == fail) return fail;
    return success;
}

RC IndexManager::destroyFile(const std::string &fileName) {
    int rc = rbfm -> destroyFile(fileName);
    if(rc == fail) return fail;
    return success;
}

RC IndexManager::openFile(const std::string &fileName, IXFileHandle &ixFileHandle) {
    int rc = rbfm -> openFile(fileName, ixFileHandle.fileHandle);
    if(rc == fail) return fail;
    return success;
}

RC IndexManager::closeFile(IXFileHandle &ixFileHandle) {
    int rc = rbfm -> closeFile(ixFileHandle.fileHandle);
    if(rc == fail) return fail;
    return success;
}

RC IndexManager::insertEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid) {
    return -1;
}

RC IndexManager::deleteEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid) {
    return -1;
}

RC IndexManager::scan(IXFileHandle &ixFileHandle,
                      const Attribute &attribute,
                      const void *lowKey,
                      const void *highKey,
                      bool lowKeyInclusive,
                      bool highKeyInclusive,
                      IX_ScanIterator &ix_ScanIterator) {
    return -1;
}

void IndexManager::printBtree(IXFileHandle &ixFileHandle, const Attribute &attribute) const {
}

IX_ScanIterator::IX_ScanIterator() {
}

IX_ScanIterator::~IX_ScanIterator() {
}

RC IX_ScanIterator::getNextEntry(RID &rid, void *key) {
    return -1;
}

RC IX_ScanIterator::close() {
    return -1;
}

IXFileHandle::IXFileHandle() {
    ixReadPageCounter = 0;
    ixWritePageCounter = 0;
    ixAppendPageCounter = 0;
}

IXFileHandle::~IXFileHandle() {

}
RC IXFileHandle::readPage(PageNum pageNum, void * data){
    return fileHandle.readPage(pageNum, data);
}
RC IXFileHandle::writePage(PageNum pageNum, const void * data){
    return fileHandle.writePage(pageNum, data);
}
RC IXFileHandle::appendPage(const void * data){
    return fileHandle.appendPage(data);
}

unsigned IXFileHandle::getNumberOfPages(){
    return fileHandle.getNumberOfPages();
}
RC IXFileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount) {
    return fileHandle.collectCounterValues(readPageCount, writePageCount, appendPageCount);
}

