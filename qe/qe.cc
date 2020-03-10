
#include "qe.h"
#include <stdio.h>
#include <string.h>

int success = 0, fail = -1, bufSize = 200;
Filter::Filter(Iterator *input, const Condition &condition) {
    this -> input = input;
    this -> cond = condition;
    getAttributes(attrs);
}

void Filter::getAttributes(std::vector<Attribute> &attrs) const {
    input -> getAttributes(attrs);
}
RC Filter::getNextTuple(void *data) {
    bool flag = true;
    while(flag){
        int rc = input -> getNextTuple(data);
        if(rc == fail){
            return fail;
        }

        int left_attr_index = -1, right_attr_index = -1;
        for(int i = 0; i < attrs.size(); i++){
            if(attrs[i].name == cond.lhsAttr){
                left_attr_index = i;
            }

            if(cond.bRhsIsAttr){
                if(attrs[i].name == cond.rhsAttr){
                    right_attr_index = i;
                }
            }
        }

        if(left_attr_index == -1){
            std::cout << "It will not happen" << std::endl;
            return fail;
        }
        if(attrs[left_attr_index].type == TypeInt){
            int left_attr = -1;
            int right_attr = -1;
            void * content = malloc(PAGE_SIZE);
            int recordSize = 0;

            getContentInRecord(data, content, left_attr_index, recordSize, attrs);
            left_attr = *(int *)((char *)content);
            memset(content, 0, PAGE_SIZE);

            if(!cond.bRhsIsAttr){
                right_attr = *(int *)((char *)cond.rhsValue.data);
            }else{
                getContentInRecord(data, content, right_attr_index, recordSize, attrs);
                right_attr = *(int *)((char *)content);
            }
            free(content);

            if(processWithTypeInt(left_attr, cond.op, right_attr) == true){
                return success;
            }
        }else if(attrs[left_attr_index].type == TypeReal){
            float left_attr = -1;
            float right_attr = -1;
            void * content = malloc(PAGE_SIZE);
            int recordSize = 0;

            getContentInRecord(data, content, left_attr_index, recordSize, attrs);
            left_attr = *(float *)((char *)content);
            memset(content, 0, PAGE_SIZE);

            if(!cond.bRhsIsAttr){
                right_attr = *(float *)((char *)cond.rhsValue.data);
            }else{
                getContentInRecord(data, content, right_attr_index, recordSize, attrs);
                right_attr = *(float *)((char *)content);
            }
            free(content);

            if(processWithTypeReal(left_attr, cond.op, right_attr) == true){
                return success;
            }
        }else{
            std::string left_attr, right_attr;
            void * content = malloc(PAGE_SIZE);
            int recordSize = 0;

            getContentInRecord(data, content, left_attr_index, recordSize, attrs);
            appendString(left_attr, content, 0, recordSize);
            memset(content, 0, PAGE_SIZE);

            if(!cond.bRhsIsAttr){
                int len = *(int *)((char *)cond.rhsValue.data);
                appendString(right_attr, cond.rhsValue.data, sizeof(int), len);
            }else{
                getContentInRecord(data, content, right_attr_index, recordSize, attrs);
                appendString(right_attr, content, 0, recordSize);
            }
            free(content);

            if(processWithTypeVarChar(left_attr, cond.op, right_attr) == true){
                return success;
            }
        }
    }
}

RC Iterator::getContentInRecord(void *data, void *content, int index, int &recordSize, std::vector<Attribute> attrsForIter) {
    void *buffer = malloc(PAGE_SIZE);
    rbfm->transformData(attrsForIter,data, buffer);
    short start = -1, end = -1;
    short fieldCount = *(short *)((char *)buffer);

    if(index == 0){
        start = (fieldCount + 1) * sizeof(short);
        end = *(short *)((char *)buffer + sizeof(short));
    }else{
        start = *(short *)((char *)buffer + sizeof(short) + (index - 1) * sizeof(short));
        end = *(short *)((char *)buffer + sizeof(short) + (index) * sizeof(short));
    }

    memcpy((char *)content, (char *)buffer + start, end - start);

    free(buffer);
    recordSize = end - start;
    return success;
}

bool Iterator::processWithTypeInt(int left, CompOp compOp, int right) {
    switch (compOp)
    {
        case EQ_OP:
            return left == right;
            break;
        case LT_OP:
            return left < right;
            break;
        case LE_OP:
            return left <= right;
            break;
        case GT_OP:
            return left > right;
            break;
        case GE_OP:
            return left >= right;
            break;
        case NE_OP:
            return left != right;
            break;
        default:
            std::cout << "TypeInt should not enter this statement" << std::endl;
            break;
    }
    return false;
}
bool Iterator::processWithTypeReal(float left, CompOp compOp, float right) {
    switch (compOp)
    {
        case EQ_OP:
            return left == right;
            break;
        case LT_OP:
            return left < right;
            break;
        case LE_OP:
            return left <= right;
            break;
        case GT_OP:
            return left > right;
            break;
        case GE_OP:
            return left >= right;
            break;
        case NE_OP:
            return left != right;
            break;
        default:
            std::cout << "TypeInt should not enter this statement" << std::endl;
            break;
    }
    return false;
}
bool Iterator::processWithTypeVarChar(std::string left, CompOp compOp, std::string right){
    int isStringLarger = left.compare(right);
    switch (compOp)
    {
        case EQ_OP:
            return isStringLarger == 0;
            break;
        case LT_OP:
            return isStringLarger < 0;
            break;
        case LE_OP:
            return isStringLarger <= 0;
            break;
        case GT_OP:
            return isStringLarger > 0;
            break;
        case GE_OP:
            return isStringLarger >= 0;
            break;
        case NE_OP:
            return isStringLarger != 0;
            break;
        default:
            std::cout << "TypeVarChar should not enter this statement" << std::endl;
            break;
    }
    return false;
}

RC Iterator::appendString(std::string &s, const void *record, int start_pos, int len)
{
    for (int i = 0; i < len; i++)
    {
        s += std::to_string(*((char *)record + start_pos + i));
    }
    return success;
}
//struct Condition {
//    std::string lhsAttr;        // left-hand side attribute
//    CompOp op;                  // comparison operator
//    bool bRhsIsAttr;            // TRUE if right-hand side is an attribute and not a value; FALSE, otherwise.
//    std::string rhsAttr;        // right-hand side attribute if bRhsIsAttr = TRUE
//    Value rhsValue;             // right-hand side value if bRhsIsAttr = FALSE
//};
// ... the rest of your implementations go here


Project::Project(Iterator *input, const std::vector<std::string> &attrNames) {
    this->input = input;
    input->getAttributes(allAttributes);
    for (auto& attr : allAttributes) {
        for(auto& attr2 : attrNames) {
            if (attr.name == attr2) {
                requiredAttributes.push_back(attr);
                break;
            }
        }
    }

}
RC Project::getNextTuple(void *data) {
    auto *projectData = malloc(PAGE_SIZE);
    int rc = input->getNextTuple(projectData);
    if (rc == -1) {
        free(projectData);
        return -1;
    }
//    auto *returnedData = malloc(PAGE_SIZE);
    int nullFieldLength = requiredAttributes.size();
    int nullFieldBytes = ceil((double)nullFieldLength / CHAR_BIT);
    auto *nullFieldsIndicator = (unsigned char *)malloc(nullFieldBytes);
    memset(nullFieldsIndicator, 0, nullFieldBytes);
    int outerIndex = 0;
    int outerOffset = 0;
    for (auto& attr : requiredAttributes) {
        int attrOffset = ceil((double)allAttributes.size() / CHAR_BIT);
        int index = 0;
        for (auto& attrAll : allAttributes) {
            if (attrAll.name == attr.name) {
                unsigned char c;
                memcpy(&c, (char *)projectData + index / CHAR_BIT, sizeof(unsigned char));
                int nullBit = c & (unsigned) 1 << (unsigned) (7 - index % CHAR_BIT);
                if (nullBit == 1) {
                    nullFieldsIndicator[outerIndex / CHAR_BIT] |= (unsigned) 1 << (unsigned) (7 - outerIndex % CHAR_BIT);
                    break;
                }
                else {
//                    nullFieldsIndicator[outerIndex / CHAR_BIT] |= (unsigned) 1 << (unsigned) (7 - outerIndex % CHAR_BIT);
                    if (attrAll.type == TypeReal || attrAll.type == TypeInt) {
                        memcpy((char *)data + nullFieldBytes + outerOffset, (char *)projectData + attrOffset , sizeof(int));
                        outerOffset += sizeof(int);
                        attrOffset += sizeof(int);
                    }
                    else {
                        int varCharLen = 0;
                        memcpy(&varCharLen, (char *)projectData + attrOffset, sizeof(int));
                        memcpy((char *)data + nullFieldBytes + outerOffset, (char *)projectData + attrOffset, sizeof(int) + varCharLen);
                        outerOffset += varCharLen + sizeof(int);
                        attrOffset += varCharLen + sizeof(int);
                    }

                }
            }
            else {
                unsigned char c;
                memcpy(&c, (char *)projectData + index / CHAR_BIT, sizeof(unsigned char));
                int nullBit = c & (unsigned) 1 << (unsigned) (7 - index % CHAR_BIT);
                if (nullBit == 1) {
                }
                else {
                    if (attrAll.type == TypeInt || attrAll.type == TypeReal) {
                        attrOffset += sizeof(int);
                    }
                    else {
                        int varCharLen = 0;
                        memcpy(&varCharLen, (char *)projectData + attrOffset, sizeof(int));
                        attrOffset += varCharLen + sizeof(int);
                    }
                }

            }

                index++;
        }
        outerIndex++;
    }
    memcpy(data,nullFieldsIndicator, nullFieldBytes );
//    free(returnedData);
    free(nullFieldsIndicator);
    free(projectData);
    return success;
}

void Project::getAttributes(std::vector<Attribute> &attrs) const {
    attrs.clear();
    attrs = requiredAttributes;
}

BNLJoin::BNLJoin(Iterator *leftIn, TableScan *rightIn, const Condition &condition, const unsigned numPages){
    this->leftIn = leftIn;
    this->rightIn = rightIn;
    this->condition = condition;
    this->numPages = numPages;
    this->flag = false;
    this->copiedRightIn = this->rightIn;
    this->left_attr_index = -1;
    this->right_attr_index = -1;
    this->outputBufferPointer = 0;

    setNumRecords();
    this -> leftIn -> getAttributes(attrsLeft);
    this -> rightIn -> getAttributes(attrsRight);
    setAttrIndex();
    initializeBlock();
}
void BNLJoin::getAttributes(std::vector<Attribute> &attrs) const {

}
RC BNLJoin::freeBlock() {
    if (attrsLeft[left_attr_index].type == TypeInt) {
        for (auto iter = intMap.begin(); iter != intMap.end(); iter++) {
            for (auto iterVec = iter->second.begin(); iterVec != iter->second.end(); iterVec++) {
                free(*iterVec);
            }
            iter->second.clear();
        }
        intMap.clear();
    }
    else if (attrsLeft[left_attr_index].type == TypeReal){
        for (auto iter = realMap.begin(); iter != realMap.end(); iter++) {
            for (auto iterVec = iter->second.begin(); iterVec != iter->second.end(); iterVec++) {
                free(*iterVec);
            }
            iter->second.clear();

        }
        realMap.clear();
    }
    else {
        for (auto iter = varCharMap.begin(); iter != varCharMap.end(); iter++) {
            for (auto iterVec = iter->second.begin(); iterVec != iter->second.end();iterVec++) {
                free(*iterVec);
            }
            iter->second.clear();
        }
        varCharMap.clear();
    }
}
void BNLJoin::combine(void *leftData, void *rightData, void *data) {
    int totalNullFieldIndicatorSize = attrsLeft.size() + attrsRight.size();
    int totalNullFieldINdicatorByte = ceil((double) totalNullFieldIndicatorSize / CHAR_BIT);
    memset(data, 0, totalNullFieldINdicatorByte);
    int leftOffset = attrsLeft.size();
    memcpy(data, leftData,ceil((double)attrsLeft.size() / CHAR_BIT));
    for (int i = 0; i < attrsRight.size(); i++) {
//        std::cout << "afsergehere" << std::endl;
        bool nullBit = *((unsigned char *)rightData + i / CHAR_BIT) & (unsigned)1 << (unsigned)(7 - i % CHAR_BIT);

        if (nullBit) {
            *((unsigned char *)data + (leftOffset + i ) / CHAR_BIT) |= (unsigned)1 << (unsigned)(7 - i % CHAR_BIT);
        }
        else {
            ///////////////
        }
    }
    auto *contents = malloc(PAGE_SIZE);
    int totalRecordSize = 0;
    for (int i = 0; i != attrsLeft.size(); i++) {
        int recordSize = 0;
        getContentInRecord(leftData, contents, i, recordSize, attrsLeft);
        totalRecordSize += recordSize;
    }
    memcpy((char *)data + totalNullFieldINdicatorByte, (char *)leftData + (int)ceil((double) attrsLeft.size() / CHAR_BIT), totalRecordSize);
    memcpy((char *)data + totalNullFieldINdicatorByte + totalRecordSize, (char *)rightData +(int)ceil((double) attrsRight.size() / CHAR_BIT), PAGE_SIZE - totalNullFieldINdicatorByte - totalRecordSize);


}
RC BNLJoin::match(void *rightAttr, void *rightData, int rightRecordSize) {
    if (attrsRight[right_attr_index].type == TypeInt) {
        int rightVal = *(int *)(rightAttr);
        for (auto leftData : intMap[rightVal]) {
            if (leftData == NULL) {
                std::cout << "map store NULL" << std::endl;
            }
            else {
                auto *data = malloc(PAGE_SIZE);
                combine(leftData, rightData, data);
                outputBuffer.push_back(data);
            }
        }

    }
    else if (attrsRight[right_attr_index].type == TypeReal) {
        float rightVal = *(float *)(rightAttr);
        for (auto leftData : realMap[rightVal]) {
            if (leftData == NULL) {
                std::cout << "map store NULL" << std::endl;
            }
            else {
                auto *data = malloc(PAGE_SIZE);
                combine(leftData, rightData, data);
                outputBuffer.push_back(data);
            }
        }
    }
    else  {
        std::string s = "";
        appendString(s, rightAttr, 0, rightRecordSize);
        for (auto leftData : varCharMap[s]) {
            if (leftData == NULL) {
                std::cout << "map store NULL" << std::endl;
            }
            else {
                auto *data = malloc(PAGE_SIZE);
                combine(leftData, rightData, data);
                outputBuffer.push_back(data);
            }
        }
    }
    if (outputBuffer.size() > 0) {
        return success;
    }
    return fail;
}
RC BNLJoin::getNextTuple(void *data) {
    if (outputBuffer.size() > outputBufferPointer) {
        memcpy(data, outputBuffer[outputBufferPointer], PAGE_SIZE);
        outputBufferPointer++;
        return success;
    }
    else {
        for(auto iter = outputBuffer.begin(); iter != outputBuffer.end(); iter++) {
            free(*iter);
        }
        outputBuffer.clear();
        outputBufferPointer = 0;
    }
    bool flagForMatch = false;
    while(!flagForMatch){
        void * right_data = malloc(PAGE_SIZE);
        if(rightIn -> getNextTuple(right_data) == fail){ //2.2
            rightIn->setIterator();
            freeBlock();
            if(flag){ //2.3
                free(right_data);
                return QE_EOF;
            }else{ //2.4
                initializeBlock();
            }
        }else{ //2.1
            void * right_attr = malloc(PAGE_SIZE);
            int recordSize = 0;
            getContentInRecord(right_data, right_attr, right_attr_index, recordSize, attrsRight);
            if(recordSize == 0){
                free(right_attr);
                continue;
            }
            if (match(right_attr, right_data, recordSize) == success)
            {
                flagForMatch = true;
            }
        }
        free(right_data);
    }
    memcpy(data, outputBuffer[outputBufferPointer], bufSize);
    outputBufferPointer++;
    return success;
}
void BNLJoin::setNumRecords() {
    this->numRecords = this->numPages * PAGE_SIZE / 100;
}
void BNLJoin::setAttrIndex() {
    for(int i = 0; i < attrsLeft.size(); i++){
        if(attrsLeft[i].name == condition.lhsAttr){
            this -> left_attr_index = i;
            break;
        }
    }
    for(int i = 0; i < attrsRight.size(); i++){
        if(attrsRight[i].name == condition.rhsAttr){
            this -> right_attr_index = i;
            break;
        }
    }
}
RC BNLJoin::initializeBlock() {
    void * data = malloc(PAGE_SIZE);
    int k = 0;

    while(leftIn -> getNextTuple(data) != fail && k < numRecords){
        if(attrsLeft[left_attr_index].type == TypeInt){
            void * attr = malloc(PAGE_SIZE);
            void * copy = malloc(PAGE_SIZE);
            memcpy(copy, data, PAGE_SIZE);
            int recordSize = 0;
            getContentInRecord(data, attr, left_attr_index, recordSize, attrsLeft);
            if(recordSize == 0){
                free(attr);
                continue;
            }

            int val = *(int *)((char *)attr);
            intMap[val].push_back(copy);
            free(attr);
        }else if(attrsLeft[left_attr_index].type == TypeReal){
            void * attr = malloc(PAGE_SIZE);
            void * copy = malloc(PAGE_SIZE);
            memcpy(copy, data, PAGE_SIZE);
            int recordSize = 0;
            getContentInRecord(data, attr, left_attr_index, recordSize, attrsLeft);
            if(recordSize == 0){
                free(attr);
                continue;
            }

            float val = *(float *)((char *)attr);
            realMap[val].push_back(copy);
            free(attr);
        }else{
            void * attr = malloc(PAGE_SIZE);
            void * copy = malloc(PAGE_SIZE);
            memcpy(copy, data, PAGE_SIZE);

            int recordSize = 0;
            getContentInRecord(data, attr, left_attr_index, recordSize, attrsLeft);
            if(recordSize == 0){
                free(attr);
                continue;
            }

            std::string val;
            appendString(val, attr, 0, recordSize);
            varCharMap[val].push_back(copy);
            free(attr);
        }
        k++;
        memset(data, 0, PAGE_SIZE);
    }

    if(leftIn -> getNextTuple(data) == fail){
        this -> flag = true;
    }
    free(data);

    if(k == 0) return fail;
    return success;
}

