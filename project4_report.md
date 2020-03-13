## 1. Basic information

- Team #: 6
- Github Repo Link: https://github.com/UCI-Chenli-teaching/cs222p-winter20-team-6
- Student 1 UCI NetID: songh9
- Student 1 Name: Han Song
- Student 2 UCI NetID (if applicable): feihongq
- Student 2 Name (if applicable): Feihong Qian

## 2. Catalog information about Index

##### Show your catalog information about an index (tables, columns).

We have a table called "Index" to store all the index catalog information about index. When createCatalag() is called, this table will be created.
Column including tableNames, IndexNames, FileNames(where is the index file).
When the catalog table is created, the catalog table "Tables" and "Columns" will also be updated.

## 3. Block Nested Loop Join (If you have implemented this feature)

##### Describe how your block nested loop join works (especially, how you manage the given buffers.)

For BNL Join, we use numOfPages * PAGE_SIZE / bufSize to get how many records we need to put in one block. We get the value for the record and use a map (whose key is the value, value is a vector of pointer pointing to the record position) to store it. Then for a certain block, traverse the right table and find matching from the map. After one iteration, clear the map and read next block. Reset the iterator for right table and repeat until all left table records are used.

## 4. Index Nested Loop Join (If you have implemented this feature)

##### Describe how your index nested loop join works.

Similar to BNL Join, this time, traverse the left table and searching index files for the right table for a match. After finding the match, retrieve the data in the right table and do the join.

## 5. Grace Hash Join (If you have implemented this feature)

##### Describe how your grace hash join works (especially, in-memory structure).

No.

## 6. Aggregation

##### Describe how your aggregation (basic, group-based hash) works.

We only implement basic aggregation. We do aggregation for the desired attribute.

By calling getnext() to traverse all the record and get the desired value (max, min, count, average, sum).
Then append a one byte nullfieldIndicator to return.

## 7. Implementation Detail

##### Have you added your own source file (.cc or .h)?

No.

##### Have you implemented any optional features? Then, describe them here.

No.

##### Other implementation details:
For RM part, we modify the insertTuple, deleteTuple, updateTuple by add insertEntries, deleteEntries and updateEntries
functions respectively. In insertEntries function called by insertTuple function, we simply open the index file and add
the retrieved entry(key, RID) into it. In deleteEntries function called by deleteTuple function, we open the index file(if it exists)
and delete the given entry(key, RID). In updateEntries function called by updateTuple function, we first open the index
file, then delete the old entry and finally add the new entry into the index file.
## 6. Other (optional)

##### Freely use this section to tell us about things that are related to the project 4, but not related to the other sections (optional)

CreateIndex:
When we create a new index for a certain attribute. We traverse all the records in record table and sync to the index table.
InsertRecord:
When a new record is inserted. besides what we have done in project2. For each attribute for this record, we need to know if the index for this attribute exists, if so. need to add it to the index file as well.
