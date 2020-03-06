#include "rm.h"
#include <stdio.h>
#include <string.h>

const int success = 0;
const int fail = -1;

RelationManager &RelationManager::instance()
{
    static RelationManager _relation_manager = RelationManager();
    return _relation_manager;
}

RelationManager::RelationManager()
{
    rbfm = &RecordBasedFileManager::instance();
}

RelationManager::~RelationManager() = default;

RelationManager::RelationManager(const RelationManager &) = default;

RelationManager &RelationManager::operator=(const RelationManager &) = default;

RC RelationManager::createCatalog()
{
    int rc = rbfm->createFile("Tables");
    if (rc == fail)
        return fail;
    rc = rbfm->createFile("Columns");
    if (rc == fail)
        return fail;

    std::string table = "Tables";
    std::string column = "Columns";
    rc = createCatalogTables(table);
    if (rc == fail)
        return fail;
    rc = createCatalogColumns(column);
    if (rc == fail)
        return fail;
    return success;
}

RC RelationManager::deleteCatalog()
{
    int rc = rbfm->destroyFile("Tables");
    if (rc == fail)
        return fail;
    rc = rbfm->destroyFile("Columns");
    if (rc == fail)
        return fail;

    return success;
}

RC RelationManager::createTable(const std::string &tableName, const std::vector<Attribute> &attrs)
{
    //Step 1: add table of tables;
    FileHandle fileHandle;
    int rc = rbfm->openFile("Tables", fileHandle);
    if (rc == fail)
        return fail;

    if (tableName != "Tables" && tableName != "Columns")
    {
        rc = rbfm->createFile(tableName);
        if (rc == fail)
            return fail;
    }

    int table_counter = fileHandle.slotCounter;

    addTableOfTables(fileHandle, table_counter, tableName);

    rc = rbfm->closeFile(fileHandle);
    if (rc == fail)
        return fail;

    //Step 2: add table of columns;
    FileHandle fileHandle2;

    rc = rbfm->openFile("Columns", fileHandle2);
    if (rc == fail)
        return fail;

    addTableOfColumns(fileHandle2, table_counter, tableName, attrs);

    rc = rbfm->closeFile(fileHandle2);
    if (rc == fail)
        return fail;

    return success;
}

void RelationManager::addTableOfColumns(FileHandle &fileHandle, const int table_counter, const std::string &tableName,
                                        const std::vector<Attribute> &attrs)
{
    std::vector<Attribute> columnDescriptor;
    createColumnDescriptor(columnDescriptor);

    void *column = malloc(PAGE_SIZE);
    int nullFieldsIndicatorActualSize = ceil((double)columnDescriptor.size() / CHAR_BIT);
    auto *nullsIndicator = (unsigned char *)malloc(nullFieldsIndicatorActualSize);
    memset(nullsIndicator, 0, nullFieldsIndicatorActualSize);

    for (int i = 0; i < (int)attrs.size(); i++)
    {
        RID rid;
        int recordSize = 0;

        prepareColumnRecord(columnDescriptor.size(), nullsIndicator, table_counter + 1,
                            attrs[i].name.size(), attrs[i].name,
                            attrs[i].type, attrs[i].length, i + 1, tableName.size(), tableName, 1, column, &recordSize);

        rbfm->insertRecord(fileHandle, columnDescriptor, column, rid);

        memset(column, 0, PAGE_SIZE);
    }

    free(column);
    free(nullsIndicator);
}

void RelationManager::addTableOfTables(FileHandle &fileHandle, const int table_counter, const std::string &tableName)
{
    std::vector<Attribute> tableDescriptor;
    createTableDescriptor(tableDescriptor);
    RID rid;
    int recordSize = 0;
    void *table = malloc(PAGE_SIZE);

    int nullFieldsIndicatorActualSize = ceil((double)tableDescriptor.size() / CHAR_BIT);
    auto *nullsIndicator = (unsigned char *)malloc(nullFieldsIndicatorActualSize);
    memset(nullsIndicator, 0, nullFieldsIndicatorActualSize);

    prepareTableRecord(tableDescriptor.size(), nullsIndicator, table_counter + 1, tableName.size(), tableName,
                       tableName.size(), tableName, 1, table, &recordSize);

    rbfm->insertRecord(fileHandle, tableDescriptor, table, rid);

    free(table);
    free(nullsIndicator);
}

RC RelationManager::deleteTable(const std::string &tableName)
{
    if (tableName == "Tables" || tableName == "Columns")
        return fail;
    rbfm->destroyFile(tableName);

    //Step 1 : delete table in table
    FileHandle fileHandle;
    int rc = rbfm->openFile("Tables", fileHandle);
    if (rc == fail)
        return fail;
    std::vector<Attribute> tableDescriptor;
    createTableDescriptor(tableDescriptor);

    deleteRecordInTableOrColumn(tableName, fileHandle, tableDescriptor, 0);
    rc = rbfm->closeFile(fileHandle);
    if (rc == fail)
        return fail;

    //Step 2 : delete columns in table
    FileHandle fileHandle2;

    rc = rbfm->openFile("Columns", fileHandle2);
    if (rc == fail)
        return fail;

    std::vector<Attribute> columnDescriptor;
    createTableDescriptor(columnDescriptor);
    deleteRecordInTableOrColumn(tableName, fileHandle2, columnDescriptor, 1);

    rc = rbfm->closeFile(fileHandle2);
    if (rc == fail)
        return fail;

    return success;
}

RC RelationManager::deleteRecordInTableOrColumn(const std::string &tableName, FileHandle &fileHandle,
                                                std::vector<Attribute> descriptor, int flag)
{
    void *currentPage = malloc(PAGE_SIZE);
    void *table = malloc(PAGE_SIZE);
    void *table_name = malloc(PAGE_SIZE);

    int pageNum = fileHandle.getNumberOfPages();
    RID rid;

    for (int i = 0; i < pageNum; i++)
    {
        fileHandle.readPage(i, currentPage);
        for (int j = 0; j < rbfm->getSlotNumber(currentPage); j++)
        {
            rid.pageNum = i;
            rid.slotNum = j + 1;
            int rc = 0;

            int len1 = *(short *)((char *)currentPage + PAGE_SIZE - 2 * sizeof(int) - (j + 1) * 2 * sizeof(short));
            int start1 = *(short *)((char *)currentPage + PAGE_SIZE - 2 * sizeof(int) - (j + 1) * 2 * sizeof(short) +
                                    sizeof(short));
            memcpy((char *)table, (char *)currentPage + start1, len1);

            int index = 0;
            if (flag == 0)
            {
                index = 1;
            }
            else
            {
                index = 5;
            }
            int start = *(short *)((char *)table + index * sizeof(short));
            int len = *(short *)((char *)table + (index + 1) * sizeof(short)) - *(short *)((char *)table + index * sizeof(short));

            memcpy((char *)table_name, (char *)table + start, len);

            std::string s;
            rc = appendString(s, table_name, 0, len);
            if (rc == fail)
            {
                free(currentPage);
                free(table);
                free(table_name);
                return fail;
            }

            if (s == tableName)
            {
                rc = rbfm->deleteRecord(fileHandle, descriptor, rid);
                if (rc == fail)
                {
                    free(currentPage);
                    free(table);
                    free(table_name);
                    return fail;
                }
            }

            memset((char *)table, 0, PAGE_SIZE);
            memset((char *)table_name, 0, PAGE_SIZE);
        }
        memset((char *)currentPage, 0, PAGE_SIZE);
    }

    free(currentPage);
    free(table);
    free(table_name);
    return success;
}

RC RelationManager::getAttributes(const std::string &tableName, std::vector<Attribute> &attrs)
{
    int rc = 0;
    bool flag = false;
    void *currentPage = malloc(PAGE_SIZE);
    void *column = malloc(PAGE_SIZE);
    void *table_name = malloc(PAGE_SIZE);

    FileHandle fileHandle;
    rbfm->openFile("Columns", fileHandle);

    int pageNum = fileHandle.getNumberOfPages();
    for (int i = 0; i < pageNum; i++)
    { //for each page
        fileHandle.readPage(i, currentPage);
        for (int j = 0; j < rbfm->getSlotNumber(currentPage); j++)
        { //read each record
            int column_len = *(short *)((char *)currentPage + PAGE_SIZE - 2 * sizeof(int) -
                                        (j + 1) * 2 * sizeof(short));
            int column_start = *(short *)((char *)currentPage + PAGE_SIZE - 2 * sizeof(int) -
                                          (j + 1) * 2 * sizeof(short) + sizeof(short));
            memcpy((char *)column, (char *)currentPage + column_start, column_len); //copy the column record

            int start = *(short *)((char *)column + 5 * sizeof(short)); //table_name_start_pos
            int len = *(short *)((char *)column + 6 * sizeof(short)) -
                      *(short *)((char *)column + 5 * sizeof(short)); //table_name_len

            memcpy((char *)table_name, (char *)column + start, len); //copy the table_name
            std::string recordedTableName;
            rc = appendString(recordedTableName, table_name, 0, len);
            if (rc == fail)
            {
                free(currentPage);
                free(column);
                free(table_name);
                rbfm->closeFile(fileHandle);
                return fail;
            }
            //Format: fieldCount + offset + table_id, table_name, column_name, column_type, column_length, column_position, table_version
            //use the recorded table_name of each column record to match the given table_nam. if so, get the attribute of this table.
            if (recordedTableName == tableName)
            {
                rc = filterAttributeFromColumnRecord(column, attrs);
                if (rc == fail)
                {
                    free(currentPage);
                    free(column);
                    free(table_name);
                    rbfm->closeFile(fileHandle);

                    return fail;
                }
                flag = true;
            }

            memset((char *)column, 0, PAGE_SIZE);
            memset((char *)table_name, 0, PAGE_SIZE);
        }
        memset((char *)currentPage, 0, PAGE_SIZE);
    }

    free(currentPage);
    free(column);
    free(table_name);
    rbfm->closeFile(fileHandle);

    if (flag)
        return success;
    return fail;
}

RC RelationManager::filterAttributeFromColumnRecord(const void *column, std::vector<Attribute> &attrs)
{
    Attribute attr;
    int start_pos = *(short *)((char *)column + 1 * sizeof(short)); //start pos of each attribute (we only need column_name, column_type, column_length)
    int len = *(short *)((char *)column + 2 * sizeof(short)) - start_pos;

    //get the name of the attribute
    std::string name;
    int rc = appendString(name, column, start_pos, len);
    if (rc == fail)
    {
        return fail;
    }
    attr.name = name;

    //get the type of the attribute
    start_pos += len;
    len = sizeof(int);
    void *type_enum_address = malloc(sizeof(AttrType));
    memcpy((char *)type_enum_address, (char *)column + start_pos, sizeof(AttrType));
    int type_enum = *(AttrType *)((char *)type_enum_address);
    free(type_enum_address);

    switch (type_enum)
    {
        case 0:
            attr.type = TypeInt;
            break;
        case 1:
            attr.type = TypeReal;
            break;
        default:
            attr.type = TypeVarChar;
    }

    //get the length of the attribute
    start_pos += len;
    auto *attr_length_addr = malloc(sizeof(AttrLength));
    memcpy((char *)attr_length_addr, (char *)column + start_pos, sizeof(AttrLength));
    attr.length = *(AttrLength *)((char *)attr_length_addr);
    free(attr_length_addr);

    attrs.push_back(attr);
    return success;
}

RC RelationManager::appendString(std::string &s, const void *record, int start_pos, int len)
{
    for (int i = 0; i < len; i++)
    {
        s += *((char *)record + start_pos + i);
    }
    return success;
}

RC RelationManager::insertTuple(const std::string &tableName, const void *data, RID &rid)
{
    if (tableName == "Tables" || tableName == "Columns")
        return fail;
    FileHandle fileHandle;
    int rc = 0;
    rc = rbfm->openFile(tableName, fileHandle);
    if (rc == fail)
        return fail;
    std::vector<Attribute> recordDescriptor;

    getAttributes(tableName, recordDescriptor);
    rc = rbfm->insertRecord(fileHandle, recordDescriptor, data, rid);
    if (rc == fail)
    {
        rc = rbfm->closeFile(fileHandle);
        return fail;
    }
    rc = rbfm->closeFile(fileHandle);
    if (rc == fail)
        return fail;
    return success;
}

RC RelationManager::deleteTuple(const std::string &tableName, const RID &rid)
{
    if (tableName == "Tables" || tableName == "Columns")
        return fail;
    FileHandle fileHandle;
    int rc = 0;
    rc = rbfm->openFile(tableName, fileHandle);
    if (rc == fail)
    {
        return fail;
    }
    std::vector<Attribute> recordDescriptor;
    getAttributes(tableName, recordDescriptor);
    rc = rbfm->deleteRecord(fileHandle, recordDescriptor, rid);
    if (rc == fail)
    {
        rc = rbfm->closeFile(fileHandle);
        return fail;
    }
    rc = rbfm->closeFile(fileHandle);
    if (rc == fail)
        return fail;
    return success;
}

RC RelationManager::updateTuple(const std::string &tableName, const void *data, const RID &rid)
{
    if (tableName == "Tables" || tableName == "Columns")
        return fail;

    FileHandle fileHandle;
    int rc = 0;
    rc = rbfm->openFile(tableName, fileHandle);
    if (rc == fail)
        return fail;
    std::vector<Attribute> recordDescriptor;
    getAttributes(tableName, recordDescriptor);
    rc = rbfm->updateRecord(fileHandle, recordDescriptor, data, rid);
    if (rc == fail)
    {
        rc = rbfm->closeFile(fileHandle);
        return fail;
    }
    rc = rbfm->closeFile(fileHandle);
    if (rc == fail)
        return fail;
    return success;
}

RC RelationManager::readTuple(const std::string &tableName, const RID &rid, void *data)
{
    FileHandle fileHandle;
    int rc = 0;
    rc = rbfm->openFile(tableName, fileHandle);
    if (rc == fail)
        return fail;
    std::vector<Attribute> recordDescriptor;
    getAttributes(tableName, recordDescriptor);
    rc = rbfm->readRecord(fileHandle, recordDescriptor, rid, data);
    if (rc == fail)
    {
        rc = rbfm->closeFile(fileHandle);
        return fail;
    }
    rc = rbfm->closeFile(fileHandle);
    if (rc == fail)
        return fail;
    return success;
}

RC RelationManager::printTuple(const std::vector<Attribute> &attrs, const void *data)
{
    int rc = rbfm->printRecord(attrs, data);
    if (rc == fail)
        return fail;
    return success;
}

RC RelationManager::readAttribute(const std::string &tableName, const RID &rid, const std::string &attributeName,
                                  void *data)
{
    FileHandle fileHandle;
    int rc = 0;
    rc = rbfm->openFile(tableName, fileHandle);
    if (rc == fail)
        return fail;
    std::vector<Attribute> recordDescriptor;
    getAttributes(tableName, recordDescriptor);
    rc = rbfm->readAttribute(fileHandle, recordDescriptor, rid, attributeName, data);
    if (rc == fail)
    {
        rbfm->closeFile(fileHandle);

        return fail;
    }
    rbfm->closeFile(fileHandle);
    return success;
}
RC RM_ScanIterator::getTotalslot(const std::string &tableName)
{
    FileHandle fileHandle;
    int rc = 0;
    rc = rbfm->openFile(tableName, fileHandle);
    if (rc == fail)
    {
        rbfm->closeFile(fileHandle);
        return fail;
    }
    rc = rbfmScanIterator.getTotalSlotNumber(fileHandle);
    rbfm->closeFile(fileHandle);
    return rc;
}
RC RelationManager::scan(const std::string &tableName,
                         const std::string &conditionAttribute,
                         const CompOp compOp,
                         const void *value,
                         const std::vector<std::string> &attributeNames,
                         RM_ScanIterator &rm_ScanIterator)
{
    int rc = 0;
    rc = rbfm->openFile(tableName, rm_ScanIterator.fileHandle);
    if (rc == fail)
        return fail;
    std::vector<Attribute> recordDescriptor;
    getAttributes(tableName, recordDescriptor);

    rc = rbfm->scan(rm_ScanIterator.fileHandle, recordDescriptor, conditionAttribute, compOp, value, attributeNames, rm_ScanIterator.rbfmScanIterator);
    if (rc == fail)
        return fail;
    return success;
}

// Extra credit work
RC RelationManager::dropAttribute(const std::string &tableName, const std::string &attributeName)
{
    return -1;
}

// Extra credit work
RC RelationManager::addAttribute(const std::string &tableName, const Attribute &attr)
{
    return -1;
}

RC RM_ScanIterator::getNextTuple(RID &rid, void *data)
{
    return rbfmScanIterator.getNextRecord(rid, data);
}
RM_ScanIterator::RM_ScanIterator()
{
    rbfm = &RecordBasedFileManager::instance();
}

RC RM_ScanIterator::close()
{
    rbfmScanIterator.close();
    rbfm->closeFile(fileHandle);
    return 0;
}

RC RelationManager::createTableDescriptor(std::vector<Attribute> &descriptor)
{

    Attribute attr;
    attr.name = "table_id";
    attr.type = TypeInt;
    attr.length = (AttrLength)4;
    descriptor.push_back(attr);

    attr.name = "table_name";
    attr.type = TypeVarChar;
    attr.length = (AttrLength)50;
    descriptor.push_back(attr);

    attr.name = "file_name";
    attr.type = TypeVarChar;
    attr.length = (AttrLength)50;
    descriptor.push_back(attr);

    attr.name = "table_version";
    attr.type = TypeInt;
    attr.length = (AttrLength)4;
    descriptor.push_back(attr);

    return success;
}

RC RelationManager::createColumnDescriptor(std::vector<Attribute> &descriptor)
{

    Attribute attr;
    attr.name = "table_id";
    attr.type = TypeInt;
    attr.length = (AttrLength)4;
    descriptor.push_back(attr);

    attr.name = "column_name";
    attr.type = TypeVarChar;
    attr.length = (AttrLength)50;
    descriptor.push_back(attr);

    attr.name = "column_type";
    attr.type = TypeInt;
    attr.length = (AttrLength)4;
    descriptor.push_back(attr);

    attr.name = "column_length";
    attr.type = TypeInt;
    attr.length = (AttrLength)4;
    descriptor.push_back(attr);

    attr.name = "column_position";
    attr.type = TypeInt;
    attr.length = (AttrLength)4;
    descriptor.push_back(attr);

    attr.name = "table_name";
    attr.type = TypeVarChar;
    attr.length = (AttrLength)50;
    descriptor.push_back(attr);

    attr.name = "table_version";
    attr.type = TypeInt;
    attr.length = (AttrLength)4;
    descriptor.push_back(attr);

    return success;
}

RC RelationManager::createCatalogTables(std::string &tableName)
{
    std::vector<Attribute> attrs;
    Attribute attr;
    attr.name = "table-id";
    attr.type = TypeInt;
    attr.length = 4;
    attrs.push_back(attr);

    attr.name = "table-name";
    attr.type = TypeVarChar;
    attr.length = (AttrLength)50;
    attrs.push_back(attr);

    attr.name = "file-name";
    attr.type = TypeVarChar;
    attr.length = (AttrLength)50;
    attrs.push_back(attr);

    return createTable(tableName, attrs);
}
RC RelationManager::createCatalogColumns(std::string &tableName)
{
    std::vector<Attribute> attrs;
    Attribute attr;

    attr.name = "table-id";
    attr.type = TypeInt;
    attr.length = (AttrLength)4;
    attrs.push_back(attr);

    attr.name = "column-name";
    attr.type = TypeVarChar;
    attr.length = (AttrLength)50;
    attrs.push_back(attr);

    attr.name = "column-type";
    attr.type = TypeInt;
    attr.length = 4;
    attrs.push_back(attr);

    attr.name = "column-length";
    attr.type = TypeInt;
    attr.length = 4;
    attrs.push_back(attr);

    attr.name = "column-position";
    attr.type = TypeInt;
    attr.length = 4;
    attrs.push_back(attr);

    return createTable(tableName, attrs);
}

void RelationManager::prepareTableRecord(int fieldCount, unsigned char *nullFieldsIndicator, const int table_id,
                                         const int table_name_length, const std::string &table_name,
                                         const int file_name_length, const std::string &file_name,
                                         const int table_version, void *buffer, int *recordSize)
{
    int offset = 0;

    // Null-indicators
    bool nullBit = false;
    int nullFieldsIndicatorActualSize = ceil((double)fieldCount / CHAR_BIT);
    ;

    // Null-indicator for the fields
    memcpy((char *)buffer + offset, nullFieldsIndicator, nullFieldsIndicatorActualSize);
    offset += nullFieldsIndicatorActualSize;

    nullBit = nullFieldsIndicator[0] & (unsigned)1 << (unsigned)7;
    if (!nullBit)
    {
        memcpy((char *)buffer + offset, &table_id, sizeof(int));
        offset += sizeof(int);
    }

    nullBit = nullFieldsIndicator[0] & (unsigned)1 << (unsigned)6;
    if (!nullBit)
    {
        memcpy((char *)buffer + offset, &table_name_length, sizeof(int));
        offset += sizeof(int);
        memcpy((char *)buffer + offset, table_name.c_str(), table_name_length);
        offset += table_name_length;
    }

    nullBit = nullFieldsIndicator[0] & (unsigned)1 << (unsigned)5;
    if (!nullBit)
    {
        memcpy((char *)buffer + offset, &file_name_length, sizeof(int));
        offset += sizeof(int);
        memcpy((char *)buffer + offset, file_name.c_str(), file_name_length);
        offset += file_name_length;
    }

    nullBit = nullFieldsIndicator[0] & (unsigned)1 << (unsigned)4;
    if (!nullBit)
    {
        memcpy((char *)buffer + offset, &table_version, sizeof(int));
        offset += sizeof(int);
    }
    *recordSize = offset;
}

void RelationManager::prepareColumnRecord(int fieldCount, unsigned char *nullFieldsIndicator, const int table_id,
                                          const int column_name_length, const std::string &column_name,
                                          const int column_type, const int column_length, const int column_position,
                                          const int table_name_length, const std::string &table_name,
                                          const int table_version, void *buffer, int *recordSize)
{
    int offset = 0;

    // Null-indicators
    bool nullBit = false;
    int nullFieldsIndicatorActualSize = ceil((double)fieldCount / CHAR_BIT);
    ;

    // Null-indicator for the fields
    memcpy((char *)buffer + offset, nullFieldsIndicator, nullFieldsIndicatorActualSize);
    offset += nullFieldsIndicatorActualSize;

    nullBit = nullFieldsIndicator[0] & (unsigned)1 << (unsigned)7;
    if (!nullBit)
    {
        memcpy((char *)buffer + offset, &table_id, sizeof(int));
        offset += sizeof(int);
    }

    nullBit = nullFieldsIndicator[0] & (unsigned)1 << (unsigned)6;
    if (!nullBit)
    {
        memcpy((char *)buffer + offset, &column_name_length, sizeof(int));
        offset += sizeof(int);
        memcpy((char *)buffer + offset, column_name.c_str(), column_name_length);
        offset += column_name_length;
    }

    nullBit = nullFieldsIndicator[0] & (unsigned)1 << (unsigned)5;
    if (!nullBit)
    {
        memcpy((char *)buffer + offset, &column_type, sizeof(int));
        offset += sizeof(int);
    }

    nullBit = nullFieldsIndicator[0] & (unsigned)1 << (unsigned)4;
    if (!nullBit)
    {
        memcpy((char *)buffer + offset, &column_length, sizeof(int));
        offset += sizeof(int);
    }

    nullBit = nullFieldsIndicator[0] & (unsigned)1 << (unsigned)3;
    if (!nullBit)
    {
        memcpy((char *)buffer + offset, &column_position, sizeof(int));
        offset += sizeof(int);
    }

    nullBit = nullFieldsIndicator[0] & (unsigned)1 << (unsigned)2;
    if (!nullBit)
    {
        memcpy((char *)buffer + offset, &table_name_length, sizeof(int));
        offset += sizeof(int);
        memcpy((char *)buffer + offset, table_name.c_str(), table_name_length);
        offset += table_name_length;
    }

    nullBit = nullFieldsIndicator[0] & (unsigned)1 << (unsigned)1;
    if (!nullBit)
    {
        memcpy((char *)buffer + offset, &table_version, sizeof(int));
        offset += sizeof(int);
    }

    *recordSize = offset;
}