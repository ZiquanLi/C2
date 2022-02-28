////////////////////////////////////////////////////////////////////////////////
//IPC SENDER
//Message passing and shared memory are written according to the example provided by QNX training
//
//
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/neutrino.h>
#include <sys/netmgr.h>     // #define for ND_LOCAL_NODE is in here
#include "ipc_common.h"
#include <sys/iofunc.h>
#include <sys/dispatch.h>

#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string.h>

int m_buff_size = 4096;
int p_buff_size = 4096;
void ipcMessagePassingSend(int fd);
void ipcPipeSend(int fd);
void ipcSharedMemorySend(int fd, int buffer_size);

int main(int argc, char* argv[])
{
    int fd = -1;
    int longindex;
    int ch;
    //shared memory buffer size for sender
    int buffer_size;
    char method='0';
    static struct option longopts[] = {
        { "help", no_argument,  NULL, 'h' },
        { "message", no_argument,  NULL, 'm' },
        { "queue", no_argument,  NULL, 'q' },
        { "pipe",   no_argument, NULL, 'p' },
        { "shm",   optional_argument, NULL, 's' },
        { "file",   required_argument, NULL, 'f' },
        { NULL, 0, NULL, 0 }
    };
    //Haven't dealt with the situation when the user choose -m and -s at the same time
    while ((ch = getopt_long(argc, argv, "hmqps::f:", longopts, &longindex)) != -1) {

        switch (ch) {
        case 'f':
                if ((fd = open(optarg, O_RDONLY)) == -1)
                    printf("      Unable to open the file '%s'\n", optarg);
                break;
        case 'm':
        		if(method!='0'){
        			printf ("You can only choose one kind of method every time\n");
        			return EXIT_FAILURE;
        		}
        		method='m';
                break;
        case 'p':
				if(method!='0'){
					printf ("You can only choose one kind of method every time\n");
					return EXIT_FAILURE;
				}
				method='p';
				break;
        case 's':
				if(method!='0'){
					printf ("You can only choose one kind of method every time\n");
					return EXIT_FAILURE;
				}
				if(optarg==NULL){
					buffer_size = 4;
				}else{
					buffer_size = atoi(optarg);
				}
				method='s';
				break;
		//other situation
        case 'q':
        case 'h':
        default:
        	  	printf ("Usage: my_program [-m] for file transfer by message passing\n");
        	    printf ("                  [-p] for file transfer by named pipe\n");
        	    printf ("                  [-s [buffer_size]] for file transfer by shared memory,");
        	    printf ("                  buffer_size is optional (unit:k) \n");
    	  		printf ("                  [-f file_path] for choosing the target file\n");
        	    printf ("                  [-h] for this help command\n");
        	    printf ("                  note: sender and receiver should use the same method!\n");
        	    printf ("                  note: Launch the receiver before sender!\n");
                return EXIT_FAILURE;
        }
    }
    if(fd==-1){
    	printf("the file to send has not been provided or error happens!\n");
    	return EXIT_FAILURE;
    }
    switch (method){
    	case 'm':
    			ipcMessagePassingSend(fd);
    			break;
    	case 'p':
    			ipcPipeSend(fd);
    			break;
    	case 's':
    			ipcSharedMemorySend( fd, buffer_size*1024);//convert k to btyes
    			break;
    	default:
    			return EXIT_FAILURE;
    }
	return EXIT_SUCCESS;
}

void ipcMessagePassingSend(int fd){
    char* buffer;
	int file_size;
	int read_size;

	buffer = (char*)malloc(m_buff_size+1);

	if(buffer == NULL){
		perror("allocate buffer error");
		exit(EXIT_FAILURE);
	}

	file_size = getFileSize(fd);
	printf ("getFileSize outputs the size of sent file: %d\n",file_size);
	read_size = read(fd, buffer,file_size);
	printf ("read_size: %d\n",read_size);
    if( read_size == -1 ) {
        perror( "read error" );
		exit(EXIT_FAILURE);
    }

    //now:buffer contains the file, and first "read_size(this is a number)" bytes of the buffer are useful

	int coid; //Connection ID to server
	fileTransfer_header_t hdr; //msg header will specify how many bytes of data will follow
	int reply_status;
	int status; //status return value
	iov_t siov[2]; //create a 2 part iov


	// locate the server
	coid = name_open(FileTransfer_SERVER_NAME, 0);
	if (coid == -1)
	{
		perror("name_open");
		exit(EXIT_FAILURE);
	}


	// build the header
	hdr.msg_type = FilTransfer_IOV_MSG_TYPE;
	hdr.data_size = read_size;

	// setup the message as a two part iov, first the header then the data
	SETIOV (&siov[0], &hdr, sizeof hdr);
	SETIOV (&siov[1], buffer, hdr.data_size);

	// and send the message off to the server
	status = MsgSendvs(coid, siov, 2, &reply_status, sizeof reply_status);
	if (status == -1)
	{
		perror("MsgSend");
		exit(EXIT_FAILURE);
	}

	printf("reply status=%d from server\n", reply_status);
	printf("MsgSend return status: %d\n", status);
	free(buffer);
}

void ipcPipeSend(int fd){
    int fifofd;
    // FIFO file path
    int fileSize=getFileSize(fd);
	int read_size=0;
	int write_siez=0;
	int sent_size=0;
	char * myfifo = "myfifo";
    // Creating the named file(FIFO)
    // mkfifo(<pathname>, <permission>)
    mkfifo(myfifo, 0666);
    char arr1[80];
    fifofd = open(myfifo, O_WRONLY);
	
    while (sent_size<fileSize)
    {
        read_size=read(fd, arr1, sizeof(arr1));
        write_siez=write(fifofd, arr1, read_size);
		sent_size+=write_siez;
    }
	close(fd);
}

void ipcSharedMemorySend(int fd, int buffer_size){
    char* buffer;
	int file_size;
	int file_to_read_size;
	int read_size;

	buffer = (char*)malloc(buffer_size);

	if(buffer == NULL){
		perror("allocate buffer error");
		exit(EXIT_FAILURE);
	}

	file_size = getFileSize(fd);
	printf ("getFileSize outputs the size of sent file: %d\n",file_size);
	if(buffer_size>=file_size)
		file_to_read_size=file_size;
	else
		file_to_read_size=buffer_size;

	read_size = read(fd, buffer,file_to_read_size);
	printf ("read_size: %d\n",read_size);
    if( read_size == -1 ) {
        perror( "read error" );
		exit(EXIT_FAILURE);
    }

    //now:buffer contains the file, and first "read_size(this is a number)" bytes of the buffer are useful



	int coid;
	int status;
	//unsigned len;
	int mem_fd;
	char *mem_ptr;
	get_shmem_msg_t get_msg;
	get_shmem_resp_t get_resp;
	release_shmem_msg_t release_msg;
	changed_shmem_msg_t changed_msg;
	changed_shmem_resp_t changed_resp;

	/* find our server */
	coid = name_open(SHMEM_SERVER_NAME, 0);
	printf ("coid: %d\n",coid);
	if (coid == -1)
	{
		perror("name_open");
		exit(EXIT_FAILURE);
	}

	get_msg.type = GET_SHMEM_MSG_TYPE;
	get_msg.shared_mem_bytes = buffer_size+4;  // share two pages, first for data to server, second for data from server

	/* send a get message to the server to get a shared memory handle from the server */
	status = MsgSend(coid, &get_msg, sizeof(get_msg), &get_resp, sizeof(get_resp));
	if (status == -1)
	{
		perror("Get shmem MsgSend");
		exit(EXIT_FAILURE);
	}

	mem_fd = shm_open_handle(get_resp.mem_handle, O_RDWR);
	if( mem_fd == -1 ) {
		perror("shm_open_handle");
		exit(EXIT_FAILURE);
	}

	mem_ptr = mmap(NULL, buffer_size+4, PROT_READ|PROT_WRITE, MAP_SHARED, mem_fd, 0);
	if(mem_ptr == MAP_FAILED) {
		perror("mmap");
		exit(EXIT_FAILURE);
	}

	/* once mapped, we don't need the fd anymore */
	close(mem_fd);

	/* put some data into the shared memory object */
	memcpy(mem_ptr, buffer, read_size);

	/* build the update message */
	changed_msg.type = CHANGED_SHMEM_MSG_TYPE;
	changed_msg.offset = 0; // arbitrarily do this at byte 0
	changed_msg.length = read_size;

	status = MsgSend( coid, &changed_msg, sizeof(changed_msg), &changed_resp, sizeof(changed_resp));
	if( status == -1 ) {
		perror("Change shmem MsgSend");
		exit(EXIT_FAILURE);
	}

	printf("Got from server: \n");
	write(STDOUT_FILENO, mem_ptr+changed_resp.offset, changed_resp.length);
	write(STDOUT_FILENO, "\n", 1);

	/* we're done, so cleanup -- unmap the memory and tell the server we're done */
	(void)munmap(mem_ptr, buffer_size+4);
	release_msg.type = RELEASE_SHMEM_MSG_TYPE;
	status = MsgSend( coid, &release_msg, sizeof(release_msg), NULL, 0);
	if( status == -1 ) {
		perror("MsgSend");
		exit(EXIT_FAILURE);
	}
}

