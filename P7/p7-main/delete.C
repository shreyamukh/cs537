#include "catalog.h"
#include "query.h"


/*
 * Deletes records from a specified relation.
 *
 * Returns:
 * 	OK on success
 * 	an error code otherwise
 */

const Status QU_Delete(const string & relation, 
		       const string & attrName, 
		       const Operator op,
		       const Datatype type, 
		       const char *attrValue)
{
    Status status;
    RID rid;
    AttrDesc attr;
    HeapFileScan *heapScanner;

    // Check if relation name is empty
    if (relation.empty())
    {
        return BADCATPARM; // error code
    }

    // If the attribute name is empty, delete all records in the relation
    if (attrName.empty())
    {

        // Create a heapScanner object for the relation
        heapScanner = new HeapFileScan(relation, status);

        if (status != OK)
            return status; // error code

        // Start the scan without any conditions
        heapScanner->startScan(0, 0, STRING, NULL, EQ);

        // Iterate over the records and delete them
        while ((status = heapScanner->scanNext(rid)) != FILEEOF)
        {
            status = heapScanner->deleteRecord();
            if (status != OK)
                return status; // error code
        }
    }

    else
    {
        // Create a heapScanner object for the relation
        heapScanner = new HeapFileScan(relation, status);
        if (status != OK)
            return status; // error code

        // Get information about the specified attribute
        status = attrCat->getInfo(relation, attrName, attr);

        if (status != OK)
            return status; // error code

        // Start the scan based on the attribute data type and condition
        int intValue;
        float floatValue;

        switch (type)
        {
            case INTEGER:
                intValue = atoi(attrValue);
                status = heapScanner->startScan(attr.attrOffset, attr.attrLen, type, (char *)&intValue, op);
                break;

            case FLOAT:
                floatValue = atof(attrValue);
                status = heapScanner->startScan(attr.attrOffset, attr.attrLen, type, (char *)&floatValue, op);
                break;

            default: // STRING
                status = heapScanner->startScan(attr.attrOffset, attr.attrLen, type, attrValue, op);
                break;
        }

        if (status != OK)
            return status; // error code

        // Iterate over the records that satisfy the condition and delete them
        while ((status = heapScanner->scanNext(rid)) == OK)
        {
            status = heapScanner->deleteRecord();
            if (status != OK)
                return status; //  error code
        }
    }

    // Check if the scan ended with FILEEOF status
    if (status != FILEEOF)
        return status; // error code

    // End the scan and clean up the heapScanner object
    heapScanner->endScan();
    delete heapScanner;

    return OK; // success!
}


