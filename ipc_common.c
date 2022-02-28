#include "ipc_common.h"

int getFileSize(int field_descriptor){

    int rc;
    struct stat buf;
    rc = fstat( field_descriptor, &buf );
    if( rc == -1 ) {
        perror( "fstat");
    }
    return buf.st_size;
}
