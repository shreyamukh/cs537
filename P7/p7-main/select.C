#include "catalog.h"
#include "query.h"


// forward declaration
const Status ScanSelect(const string & result, 
			const int projCnt, 
			const AttrDesc projNames[],
			const AttrDesc *attrDesc, 
			const Operator op, 
			const char *filter,
			const int reclen);

/*
 * Selects records from the specified relation.
 *
 * Returns:
 * 	OK on success
 * 	an error code otherwise
 */

const Status QU_Select(const string &result,
                       const int projCnt,
                       const attrInfo projNames[],
                       const attrInfo *attr,
                       const Operator op,
                       const char *attrValue)
{
// Qu_Select sets up things and then calls ScanSelect to do the actual work
    cout << "Doing QU_Select " << endl;

    AttrDesc AttrDescArray[projCnt];
    AttrDesc *attrDesc = NULL; // Pointer to hold address of conditional attribute description

    Status status;
    for (int i = 0; i < projCnt; i++) {
        status = attrCat->getInfo(projNames[i].relName, projNames[i].attrName, AttrDescArray[i]);
        if (status != OK) {
            return status;
        }
    }

    //get info if not null  
    if (attr != NULL) {
        attrDesc = new AttrDesc;
        status = attrCat->getInfo(attr->relName, attr->attrName, *attrDesc);
        if (status != OK) {
            return status;
        }
    }

    // Calculate the total length of the record to be projected
    int reclen = 0;
    for (int i = 0; i < projCnt; i++) {
        reclen += AttrDescArray[i].attrLen;
    }

    return ScanSelect(result, projCnt, AttrDescArray, attrDesc, op, attrValue, reclen);
    
       
}

const Status ScanSelect(const string & result, 
#include "stdio.h"
#include "stdlib.h"
			const int projCnt, 
			const AttrDesc projNames[],
			const AttrDesc *attrDesc, 
			const Operator op, 
			const char *filter,
			const int reclen)
{
    cout << "Doing HeapFileScan Selection using ScanSelect()" << endl;

	// Step 1: Have a temporary record for output table
    char *outputData = (char *)malloc(reclen);
    Record outputRecord;
    outputRecord.length = reclen;
    outputRecord.data = outputData;

    // Step 2: Open "result" as an InsertFileScan object
    Status status;
    InsertFileScan resultRel(result, status);
    if (status != OK) {
        return status;
    }

    // Step 3: Open current table (to be scanned) as a HeapFileScan object
    HeapFileScan heapfilescan(projNames[0].relName, status);
    if (status != OK) {
        return status;
    }

    int intValue;
    float floatValue;
    // Step 4: Check if an unconditional scan is required
    // Step 5: Check attrType: INTEGER, FLOAT, STRING
    if (attrDesc == NULL) {
        status = heapfilescan.startScan(0, 0, STRING, NULL, EQ); // Unconditional scan
    } else {
        switch (attrDesc->attrType) {
            case INTEGER:
                intValue = atoi(filter);
                status = heapfilescan.startScan(attrDesc->attrOffset, attrDesc->attrLen, INTEGER, (char*)&intValue, op);
                break;
            case FLOAT:
                floatValue = atof(filter);
                status = heapfilescan.startScan(attrDesc->attrOffset, attrDesc->attrLen, FLOAT, (char*)&floatValue, op);
                break;
            case STRING:
                status = heapfilescan.startScan(attrDesc->attrOffset, attrDesc->attrLen,STRING, filter, op); 
                break;
        }
    }
    if (status != OK) {
        return status;
    }

    // Step 6: Scan the current table
    RID rid;
    Record rec;
    while (heapfilescan.scanNext(rid) == OK) {
        status = heapfilescan.getRecord(rec);
        if (status != OK) {
			return status;
        }

        // Step 7: If find a record, then copy stuff over to the temporary record (memcpy)
        int outOffset = 0;
        for (int i = 0; i < projCnt; i++) {
            
            memcpy(outputData + outOffset, (char *)rec.data + projNames[i].attrOffset, projNames[i].attrLen);
            outOffset += projNames[i].attrLen;
        }

        // Step 8: Insert into the output table
        RID outputRID;
        status = resultRel.insertRecord(outputRecord, outputRID);
        if (status != OK) {
			return status;
        }
    }

    status = heapfilescan.endScan();
    if (status != OK){
		return status;
	}

    return OK;

}
