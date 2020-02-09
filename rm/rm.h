#ifndef _rm_h_
#define _rm_h_

#include <string>
#include <vector>

#include "../rbf/rbfm.h"

#define RM_EOF (-1) // end of a scan operator

// RM_ScanIterator is an iterator to go through tuples
class RM_ScanIterator
{
public:
    RBFM_ScanIterator rbfmScanIterator;
    FileHandle fileHandle;
    RecordBasedFileManager *rbfm;
    RM_ScanIterator();

    ~RM_ScanIterator() = default;
    RC getTotalslot(const std::string &tableName);
    // "data" follows the same format as RelationManager::insertTuple()
    RC getNextTuple(RID &rid, void *data);

    RC close();
};

// Relation Manager
class RelationManager
{
public:
    static RelationManager &instance();

    RC createCatalog();

    RC deleteCatalog();

    RC createCatalogTables(std::string &tableName);

    RC createCatalogColumns(std::string &tableName);

    RC createTable(const std::string &tableName, const std::vector<Attribute> &attrs);

    RC deleteTable(const std::string &tableName);

    RC getAttributes(const std::string &tableName, std::vector<Attribute> &attrs);

    RC insertTuple(const std::string &tableName, const void *data, RID &rid);

    RC deleteTuple(const std::string &tableName, const RID &rid);

    RC updateTuple(const std::string &tableName, const void *data, const RID &rid);

    RC readTuple(const std::string &tableName, const RID &rid, void *data);

    // Print a tuple that is passed to this utility method.
    // The format is the same as printRecord().
    RC printTuple(const std::vector<Attribute> &attrs, const void *data);

    RC readAttribute(const std::string &tableName, const RID &rid, const std::string &attributeName, void *data);

    // Scan returns an iterator to allow the caller to go through the results one by one.
    // Do not store entire results in the scan iterator.
    RC scan(const std::string &tableName,
            const std::string &conditionAttribute,
            const CompOp compOp,                            // comparison type such as "<" and "="
            const void *value,                              // used in the comparison
            const std::vector<std::string> &attributeNames, // a list of projected attributes
            RM_ScanIterator &rm_ScanIterator);

    void addTableOfTables(FileHandle &fileHandle, const int table_counter, const std::string &tableName);
    void addTableOfColumns(FileHandle &fileHandle, const int table_counter, const std::string &tableName, const std::vector<Attribute> &attrs);
    RC createTableDescriptor(std::vector<Attribute> &descriptor);
    RC createColumnDescriptor(std::vector<Attribute> &descriptor);
    void prepareTableRecord(int fieldCount, unsigned char *nullFieldsIndicator, const int table_id, const int table_name_length, const std::string &table_name,
                            const int file_name_length, const std::string &file_name, const int table_version, void *buffer, int *recordSize);
    void prepareColumnRecord(int fieldCount, unsigned char *nullFieldsIndicator, const int table_id, const int column_name_length,
                             const std::string &column_name, const int column_type, const int column_length, const int column_position, const int table_name_length, const std::string &table_name, const int table_version, void *buffer, int *recordSize);

    RC deleteRecordInTableOrColumn(const std::string &tableName, FileHandle &fileHandle, std::vector<Attribute> descriptor, int flag);
    RC appendString(std::string &s, const void *record, int start_pos, int len);
    RC filterAttributeFromColumnRecord(const void *column, std::vector<Attribute> &attrs);

    // Extra credit work (10 points)
    RC addAttribute(const std::string &tableName, const Attribute &attr);

    RC dropAttribute(const std::string &tableName, const std::string &attributeName);

private:
    RecordBasedFileManager *rbfm;

protected:
    RelationManager();                                   // Prevent construction
    ~RelationManager();                                  // Prevent unwanted destruction
    RelationManager(const RelationManager &);            // Prevent construction by copying
    RelationManager &operator=(const RelationManager &); // Prevent assignment
};

#endif