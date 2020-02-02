#include "rm.h"
const int success = 0;
const int fail = -1;
RelationManager &RelationManager::instance() {
    static RelationManager _relation_manager = RelationManager();
    return _relation_manager;
}

RelationManager::RelationManager(){
    rbfm = & RecordBasedFileManager::instance();
}

RelationManager::~RelationManager() = default;

RelationManager::RelationManager(const RelationManager &) = default;

RelationManager &RelationManager::operator=(const RelationManager &) = default;

RC RelationManager::createCatalog() {
    int rc = rbfm -> createFile("Tables");
    if(rc == fail) return fail;
    rc = rbfm -> createFile("Columns");
    if(rc == fail) return fail;

    return success;
}

RC RelationManager::deleteCatalog() {
    int rc = rbfm -> destroyFile("Tables");
    if(rc == fail) return fail;
    rc = rbfm -> destroyFile("Columns");
    if(rc == fail) return fail;

    return success;
}

RC RelationManager::createTable(const std::string &tableName, const std::vector<Attribute> &attrs) {
    //Step 1: add table of tables;

    FileHandle fileHandle;
    int rc = PagedFileManager::instance().openFile("Tables", fileHandle);
    if(rc == fail) return fail;

    int table_counter = fileHandle.slotCounter;
    addTableOfTables(fileHandle, table_counter, tableName);

    rc = PagedFileManager::instance().closeFile(fileHandle);
    if(rc == fail) return fail;

    //Step 2: add table of columns;
    FileHandle fileHandle2;

    rc = PagedFileManager::instance().openFile("Columns", fileHandle2);
    if(rc == fail) return fail;

    addTableOfColumns(fileHandle2, table_counter, tableName, attrs);

    rc = PagedFileManager::instance().closeFile(fileHandle2);
    if(rc == fail) return fail;

    return success;
}
void RelationManager::addTableOfColumns(FileHandle &fileHandle, const int table_counter, const std::string &tableName, const std::vector<Attribute> &attrs){
    std::vector<Attribute> columnDescriptor;
    createColumnDescriptor(columnDescriptor);

    void *column = malloc(PAGE_SIZE);
    int nullFieldsIndicatorActualSize = ceil((double) columnDescriptor.size() / CHAR_BIT);
    auto *nullsIndicator = (unsigned char *) malloc(nullFieldsIndicatorActualSize);
    memset(nullsIndicator, 0, nullFieldsIndicatorActualSize);

    for(int i = 0; i < attrs.size(); i++){
        RID rid;
        int recordSize = 0;

        prepareColumnRecord(columnDescriptor.size(), nullsIndicator, table_counter + 1, tableName.size(), tableName,  attrs[i].name.size(), attrs[i].name,
        attrs[i].type, attrs[i].length, i + 1, 1, column, &recordSize);

        rbfm -> insertRecord(fileHandle, columnDescriptor, column, rid);

        memset(column, 0, PAGE_SIZE);
    }

    free(column);
    free(nullsIndicator);
}
void RelationManager::addTableOfTables(FileHandle &fileHandle, const int table_counter, const std::string &tableName){
    std::vector<Attribute> tableDescriptor;
    createTableDescriptor(tableDescriptor);
    RID rid;
    int recordSize = 0;
    void *table = malloc(PAGE_SIZE);

    int nullFieldsIndicatorActualSize = ceil((double) tableDescriptor.size() / CHAR_BIT);
    auto *nullsIndicator = (unsigned char *) malloc(nullFieldsIndicatorActualSize);
    memset(nullsIndicator, 0, nullFieldsIndicatorActualSize);

    prepareTableRecord(tableDescriptor.size(), nullsIndicator, table_counter + 1, tableName.size(), tableName, tableName.size(), tableName, 1, table, &recordSize);
    rbfm -> insertRecord(fileHandle, tableDescriptor, table, rid);

    free(table);
    free(nullsIndicator);
}
RC RelationManager::deleteTable(const std::string &tableName) {
    //Step 1 : delete table in table
    FileHandle fileHandle;
    int rc = PagedFileManager::instance().openFile("Tables", fileHandle);
    if(rc == fail) return fail;
    std::vector<Attribute> tableDescriptor;
    createTableDescriptor(tableDescriptor);

    deleteRecordInTableOrColumn(tableName, fileHandle, tableDescriptor);
    printf("here");
    rc = PagedFileManager::instance().closeFile(fileHandle);
    if(rc == fail) return fail;

    //Step 2 : delete columns in table
    FileHandle fileHandle2;

    rc = PagedFileManager::instance().openFile("Columns", fileHandle2);
    if(rc == fail) return fail;

    std::vector<Attribute> columnDescriptor;
    createTableDescriptor(columnDescriptor);
    deleteRecordInTableOrColumn(tableName, fileHandle2, columnDescriptor);

    rc = PagedFileManager::instance().closeFile(fileHandle2);
    if(rc == fail) return fail;

    return success;
}

RC RelationManager::deleteRecordInTableOrColumn(const std::string &tableName, FileHandle &fileHandle, std::vector<Attribute> descriptor){
    void *currentPage = malloc(PAGE_SIZE);
    void *table = malloc(PAGE_SIZE);
    void *table_name = malloc(PAGE_SIZE);
    void *copied = malloc(PAGE_SIZE);

    int pageNum = fileHandle.getNumberOfPages();
    RID rid;
    for(int i = 0; i < pageNum; i++){
        fileHandle.readPage(i, currentPage);
        printf("fail");
        for(int j = 0; j < rbfm -> getSlotNumber(currentPage); j++){
            std::cout << j << std::endl;
            rid.pageNum = i;
            rid.slotNum = j + 1;
            int rc = rbfm -> readRecord(fileHandle, descriptor, rid, table);
            std::cout << rc << std::endl;
            if(rc == fail) {
                free(currentPage);
                free(table);
                free(table_name);
                free(copied);
                return fail;
            }

            int start = *(short *)((char *)table + sizeof(short));
            int len = *(short *)((char *)table + 2 * sizeof(short)) - *(short *)((char *)table + sizeof(short));
            std::cout << start << std::endl;
            std::cout << len << std::endl;
            //fieldCount + offset + table_id + table_name + file_name + version)
            memcpy((char *) table_name, (char *)table + start, len);
            memcpy((char *) copied, (char *) &tableName, tableName.size());
            printf("check point");
            if(memcmp(table_name, copied, tableName.size()) == 0){
                printf("here");
                rc = rbfm -> deleteRecord(fileHandle, descriptor, rid);
                if(rc == fail) {
                    free(currentPage);
                    free(table);
                    free(table_name);
                    free(copied);
                    return fail;
                }
            }

            memset((char *) table, 0, PAGE_SIZE);
            memset((char *) table_name, 0, PAGE_SIZE);
            memset((char *) copied, 0, PAGE_SIZE);
        }
        memset((char *)currentPage, 0, PAGE_SIZE);
    }

    free(currentPage);
    free(table);
    free(table_name);
    free(copied);
    return success;
}

RC RelationManager::getAttributes(const std::string &tableName, std::vector<Attribute> &attrs) {
    return -1;
}

RC RelationManager::insertTuple(const std::string &tableName, const void *data, RID &rid) {
    return -1;
}

RC RelationManager::deleteTuple(const std::string &tableName, const RID &rid) {
    return -1;
}

RC RelationManager::updateTuple(const std::string &tableName, const void *data, const RID &rid) {
    return -1;
}

RC RelationManager::readTuple(const std::string &tableName, const RID &rid, void *data) {
    return -1;
}

RC RelationManager::printTuple(const std::vector<Attribute> &attrs, const void *data) {
    return -1;
}

RC RelationManager::readAttribute(const std::string &tableName, const RID &rid, const std::string &attributeName,
                                  void *data) {
    return -1;
}

RC RelationManager::scan(const std::string &tableName,
                         const std::string &conditionAttribute,
                         const CompOp compOp,
                         const void *value,
                         const std::vector<std::string> &attributeNames,
                         RM_ScanIterator &rm_ScanIterator) {
    return -1;
}

// Extra credit work
RC RelationManager::dropAttribute(const std::string &tableName, const std::string &attributeName) {
    return -1;
}

// Extra credit work
RC RelationManager::addAttribute(const std::string &tableName, const Attribute &attr) {
    return -1;
}

RC RelationManager::createTableDescriptor(std::vector<Attribute> &descriptor) {

    Attribute attr;
    attr.name = "table_id";
    attr.type = TypeInt;
    attr.length = (AttrLength) 4;
    descriptor.push_back(attr);

    attr.name = "table_name";
    attr.type = TypeVarChar;
    attr.length = (AttrLength) 50;
    descriptor.push_back(attr);

    attr.name = "file_name";
    attr.type = TypeVarChar;
    attr.length = (AttrLength) 50;
    descriptor.push_back(attr);

    attr.name = "table_version";
    attr.type = TypeInt;
    attr.length = (AttrLength) 4;
    descriptor.push_back(attr);

    return success;
}

RC RelationManager::createColumnDescriptor(std::vector<Attribute> &descriptor) {

    Attribute attr;
    attr.name = "table_id";
    attr.type = TypeInt;
    attr.length = (AttrLength) 4;
    descriptor.push_back(attr);

    attr.name = "table_name";
    attr.type = TypeVarChar;
    attr.length = (AttrLength) 50;
    descriptor.push_back(attr);

    attr.name = "column_name";
    attr.type = TypeVarChar;
    attr.length = (AttrLength) 50;
    descriptor.push_back(attr);

    attr.name = "column_type";
    attr.type = TypeInt;
    attr.length = (AttrLength) 4;
    descriptor.push_back(attr);

    attr.name = "column_length";
    attr.type = TypeInt;
    attr.length = (AttrLength) 4;
    descriptor.push_back(attr);

    attr.name = "column_position";
    attr.type = TypeInt;
    attr.length = (AttrLength) 4;
    descriptor.push_back(attr);

    attr.name = "table_version";
    attr.type = TypeInt;
    attr.length = (AttrLength) 4;
    descriptor.push_back(attr);

    return success;
}

void RelationManager::prepareTableRecord(int fieldCount, unsigned char *nullFieldsIndicator, const int table_id, const int table_name_length, const std::string &table_name,
                   const int file_name_length, const std::string &file_name, const int table_version, void *buffer, int *recordSize) {
    int offset = 0;

    // Null-indicators
    bool nullBit = false;
    int nullFieldsIndicatorActualSize = ceil((double) fieldCount / CHAR_BIT);;

    // Null-indicator for the fields
    memcpy((char *) buffer + offset, nullFieldsIndicator, nullFieldsIndicatorActualSize);
    offset += nullFieldsIndicatorActualSize;

    nullBit = nullFieldsIndicator[0] & (unsigned) 1 << (unsigned) 7;
    if(!nullBit){
        memcpy((char *) buffer + offset, &table_id, sizeof(int));
        offset += sizeof(int);
    }

    nullBit = nullFieldsIndicator[0] & (unsigned) 1 << (unsigned) 6;
    if (!nullBit) {
        memcpy((char *) buffer + offset, &table_name_length, sizeof(int));
        offset += sizeof(int);
        memcpy((char *) buffer + offset, table_name.c_str(), table_name_length);
        offset += table_name_length;
    }

    nullBit = nullFieldsIndicator[0] & (unsigned) 1 << (unsigned) 5;
    if (!nullBit) {
        memcpy((char *) buffer + offset, &file_name_length, sizeof(int));
        offset += sizeof(int);
        memcpy((char *) buffer + offset, file_name.c_str(), file_name_length);
        offset += file_name_length;
    }

    nullBit = nullFieldsIndicator[0] & (unsigned) 1 << (unsigned) 4;
    if (!nullBit) {
        memcpy((char *) buffer + offset, &table_version, sizeof(int));
        offset += sizeof(int);
    }
    *recordSize = offset;
}

void RelationManager::prepareColumnRecord(int fieldCount, unsigned char *nullFieldsIndicator, const int table_id, const int table_name_length, const std::string &table_name,
        const int column_name_length, const std::string &column_name, const int column_type, const int column_length, const int column_position, const int table_version, void *buffer, int *recordSize)
        {
    int offset = 0;

    // Null-indicators
    bool nullBit = false;
    int nullFieldsIndicatorActualSize = ceil((double) fieldCount / CHAR_BIT);;

    // Null-indicator for the fields
    memcpy((char *) buffer + offset, nullFieldsIndicator, nullFieldsIndicatorActualSize);
    offset += nullFieldsIndicatorActualSize;

    nullBit = nullFieldsIndicator[0] & (unsigned) 1 << (unsigned) 7;
    if(!nullBit){
        memcpy((char *) buffer + offset, &table_id, sizeof(int));
        offset += sizeof(int);
    }

    nullBit = nullFieldsIndicator[0] & (unsigned) 1 << (unsigned) 6;
    if (!nullBit) {
        memcpy((char *) buffer + offset, &table_name_length, sizeof(int));
        offset += sizeof(int);
        memcpy((char *) buffer + offset, table_name.c_str(), table_name_length);
        offset += table_name_length;
    }

    nullBit = nullFieldsIndicator[0] & (unsigned) 1 << (unsigned) 5;
    if (!nullBit) {
        memcpy((char *) buffer + offset, &column_name_length, sizeof(int));
        offset += sizeof(int);
        memcpy((char *) buffer + offset, column_name.c_str(), column_name_length);
        offset += column_name_length;
    }

    nullBit = nullFieldsIndicator[0] & (unsigned) 1 << (unsigned) 4;
    if (!nullBit) {
        memcpy((char *) buffer + offset, &column_type, sizeof(int));
        offset += sizeof(int);
    }

    nullBit = nullFieldsIndicator[0] & (unsigned) 1 << (unsigned) 3;
    if (!nullBit) {
        memcpy((char *) buffer + offset, &column_length, sizeof(int));
        offset += sizeof(int);
    }

    nullBit = nullFieldsIndicator[0] & (unsigned) 1 << (unsigned) 2;
    if (!nullBit) {
        memcpy((char *) buffer + offset, &column_position, sizeof(int));
        offset += sizeof(int);
    }

    nullBit = nullFieldsIndicator[0] & (unsigned) 1 << (unsigned) 1;
    if (!nullBit) {
        memcpy((char *) buffer + offset, &table_version, sizeof(int));
        offset += sizeof(int);
    }

    *recordSize = offset;
}

