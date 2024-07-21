#include "catalog.h"
#include "query.h"

/*
 * Inserts a record into the specified relation.
 *
 * Returns:
 * 	OK on success
 * 	an error code otherwise
 */

const Status QU_Insert(const string &relation,
					   const int attrCnt,
					   const attrInfo attrList[])
{
	Status status;
	AttrDesc *attrDesc;
	int relAttrCount;
	int recLength = 0;
	Record record;
	RID rid;
	char* attrVal;

	InsertFileScan result(relation, status);
	if (status != OK)
	{
		return status;
	}
    // getting the metadata for the relation from the catalog
	status = attrCat->getRelInfo(relation, relAttrCount, attrDesc);
	if (status != OK)
	{
		return status;
	}
	// Checking if the attribute count matches
	if (relAttrCount != attrCnt) {
		 return BADCATPARM;
	}
    // Calculating the total length of the record
	for (int i = 0; i < relAttrCount; i++)
	{
		recLength += attrDesc[i].attrLen;
	}
    // Allocating memory for the record data
	record.length = recLength;
	record.data = (char *)malloc(recLength);

	for (int i = 0; i < attrCnt; i++)
	{
		for (int j = 0; j < relAttrCount; j++)
		{
			if (strcmp(attrList[i].attrName, attrDesc[j].attrName) == 0)
			{
				int type = attrList[i].attrType;
				
                // Converting the attribute value based on its type
				if (type == INTEGER)
				{
					int val = atoi((char*) attrList[i].attrValue);
					attrVal = (char*) &val;
				}
				else if (type == FLOAT)
				{
					float val = atof((char*) attrList[i].attrValue);
					attrVal = (char*) &val;
				}
				else
				{
					attrVal = (char*) attrList[i].attrValue;
				}
                // Copying the converted value into the correct position in the record data
				memcpy((char *) record.data + attrDesc[j].attrOffset, attrVal, attrDesc[j].attrLen);
			}
		}
	}

	status = result.insertRecord(record, rid);
	return status;
}
