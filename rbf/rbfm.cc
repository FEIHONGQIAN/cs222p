#include "rbfm.h"

RecordBasedFileManager &RecordBasedFileManager::instance() {
    static RecordBasedFileManager _rbf_manager = RecordBasedFileManager();
    return _rbf_manager;
}

RecordBasedFileManager::RecordBasedFileManager() = default;

RecordBasedFileManager::~RecordBasedFileManager() = default;

RecordBasedFileManager::RecordBasedFileManager(const RecordBasedFileManager &) = default;

RecordBasedFileManager &RecordBasedFileManager::operator=(const RecordBasedFileManager &) = default;

RC RecordBasedFileManager::createFile(const std::string &fileName) {
    // return -1;
    if (PagedFileManager::instance().createFile(fileName) != 0) {
        return -1;
    }
    else {
        return 0;
    }

}

RC RecordBasedFileManager::destroyFile(const std::string &fileName) {
    // return -1;
    if (PagedFileManager::instance().destroyFile(fileName) != 0) {
        return -1;
    }
    else {
        return 0;
    }
}

RC RecordBasedFileManager::openFile(const std::string &fileName, FileHandle &fileHandle) {
    // return -1;
    if (PagedFileManager::instance().openFile(fileName, fileHandle) != 0) {
        return -1;
    }
    else {
        return 0;
    }
}

RC RecordBasedFileManager::closeFile(FileHandle &fileHandle) {
    // return -1;
    if (PagedFileManager::instance().closeFile(fileHandle) != 0) {
        return -1;
    }
    else {
        return 0;
    }
}

RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                        const void *data, RID &rid) {
    return -1;
}

RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                      const RID &rid, void *data) {
    return -1;
}

RC RecordBasedFileManager::deleteRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                        const RID &rid) {
    return -1;
}
RC RecordBasedFileManager::getActualByteForNullsIndicator(int fieldCount) {
        return ceil((double) fieldCount / CHAR_BIT);

}
RC RecordBasedFileManager::printRecord(const std::vector<Attribute> &recordDescriptor, const void *data) {
    // return -1;
    bool nullBit = false;

    int fieldCount = recordDescriptor.size();
    int nullFieldsIndicatorActualSize = getActualByteForNullsIndicator(fieldCount);

    int offset = 0;

    auto *nullFieldsIndicator = (unsigned char *)malloc(sizeof(char) * nullFieldsIndicatorActualSize);
    memset(nullFieldsIndicator, 0, nullFieldsIndicatorActualSize);
    memcpy((char *) nullFieldsIndicator, data, nullFieldsIndicatorActualSize);
    offset += nullFieldsIndicatorActualSize;

    int leftShift = 7;
    // std::cout << "gogo" << std::endl;
    for (int i = 0; i < recordDescriptor.size(); i++) {
        Attribute attr = recordDescriptor[i];

        if (nullFieldsIndicator[i / 8] & ((unsigned) 1 << (unsigned) leftShift)) {
            std::cout << attr.name << ": NULL" << "\t";
        }
        else {
            void *buffer;
            switch (attr.type)
            {
            case TypeInt:
                
                buffer = malloc(sizeof(int));
                memcpy(buffer, (char *)data + offset, sizeof(int));
                offset += sizeof(int);
                std::cout << attr.name << ": " << (*(int *)buffer) << "\t";
                break;
            case TypeReal:
                buffer = malloc(sizeof(float));
                memcpy(buffer, (char *)data + offset, sizeof(float));
                offset += sizeof(float);
                std::cout << attr.name << ": " << (*(float *)buffer) << "\t";
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
    }
    std::cout << std::endl;
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



