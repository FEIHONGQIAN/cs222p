
#include "qe.h"
#include <stdio.h>
#include <string.h>

int success = 0, fail = -1;
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

            getContentInRecord(data, content, left_attr_index, recordSize);
            left_attr = *(int *)((char *)content);
            memset(content, 0, PAGE_SIZE);

            if(!cond.bRhsIsAttr){
                right_attr = *(int *)((char *)cond.rhsValue.data);
            }else{
                getContentInRecord(data, content, right_attr_index, recordSize);
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

            getContentInRecord(data, content, left_attr_index, recordSize);
            left_attr = *(float *)((char *)content);
            memset(content, 0, PAGE_SIZE);

            if(!cond.bRhsIsAttr){
                right_attr = *(float *)((char *)cond.rhsValue.data);
            }else{
                getContentInRecord(data, content, right_attr_index, recordSize);
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

            getContentInRecord(data, content, left_attr_index, recordSize);
            appendString(left_attr, content, 0, recordSize);
            memset(content, 0, PAGE_SIZE);

            if(!cond.bRhsIsAttr){
                int len = *(int *)((char *)cond.rhsValue.data);
                appendString(right_attr, cond.rhsValue.data, sizeof(int), len);
            }else{
                getContentInRecord(data, content, right_attr_index, recordSize);
                appendString(right_attr, content, 0, recordSize);
            }
            free(content);

            if(processWithTypeVarChar(left_attr, cond.op, right_attr) == true){
                return success;
            }
        }
    }
}

RC Iterator::getContentInRecord(void *data, void *content, int index, int &recordSize) {
    std::vector<Attribute> attrsForIter;
    getAttributes(attrsForIter);

    void *buffer = malloc(PAGE_SIZE);
    rbfm->transformData(attrsForIter,data, buffer);
    short start = -1, end = -1;
    short fieldCount = *(short *)((char *)buffer);

    if(index == 0){
        start = sizeof(short) + fieldCount * sizeof(short);
        end = *(short *)((char *)buffer + sizeof(short));
    }else{
        start = *(short *)((char *)buffer + sizeof(short) + (index - 1) * sizeof(short));
        end = *(short *)((char *)buffer + sizeof(short) + (index) * sizeof(short));
    }

    memcpy((char *)content, (char *)buffer + start, end - start);
    recordSize = end - start;
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
        s += *((char *)record + start_pos + i);
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

}

void Project::getAttributes(std::vector<Attribute> &attrs) const {
    attrs.clear();
    attrs = requiredAttributes;
}
