
#include "qe.h"
#include <stdio.h>
#include <string.h>

Filter::Filter(Iterator *input, const Condition &condition) {
}
// ... the rest of your implementations go here
RC Filter::getNextTuple(void *data) {

}

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
                memcpy(&c, projectData + index / CHAR_BIT, sizeof(unsigned char));
                int nullBit = c & (unsigned) 1 << (unsigned) (7 - index % CHAR_BIT);
                if (nullBit == 1) {
                    nullFieldsIndicator[outerIndex / CHAR_BIT] |= (unsigned) 1 << (unsigned) (7 - outerIndex % CHAR_BIT);
                    break;
                }
                else {
//                    nullFieldsIndicator[outerIndex / CHAR_BIT] |= (unsigned) 1 << (unsigned) (7 - outerIndex % CHAR_BIT);
                    if (attrAll.type == TypeReal || attrAll.type == TypeInt) {
                        memcpy(data + nullFieldBytes + outerOffset, projectData + attrOffset , sizeof(int));
                        outerOffset += sizeof(int);
                        attrOffset += sizeof(int);
                    }
                    else {
                        int varCharLen = 0;
                        memcpy(&varCharLen, projectData + attrOffset, sizeof(int));
                        memcpy(data + nullFieldBytes + outerOffset, projectData + attrOffset, sizeof(int) + varCharLen);
                        outerOffset += varCharLen + sizeof(int);
                        attrOffset += varCharLen + sizeof(int);
                    }

                }
            }
            else {
                unsigned char c;
                memcpy(&c, projectData + index / CHAR_BIT, sizeof(unsigned char));
                int nullBit = c & (unsigned) 1 << (unsigned) (7 - index % CHAR_BIT);
                if (nullBit == 1) {
                }
                else {
                    if (attrAll.type == TypeInt || attrAll.type == TypeReal) {
                        attrOffset += sizeof(int);
                    }
                    else {
                        int varCharLen = 0;
                        memcpy(&varCharLen, projectData + attrOffset, sizeof(int));
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
