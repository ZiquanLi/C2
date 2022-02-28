////////////////////////////////////////////////////////////////////////////////
//IPC RECEIVER
//
//
//
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <sys/neutrino.h>

#include "ipc_common.h"

#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/mman.h>
#include <process.h>
#include <string.h>
#include <unistd.h>

typedef union
{
	uint16_t msg_type;
	struct _pulse pulse;
	cksum_header_t cksum_hdr;
} msg_buf_t;

typedef union
{
	uint16_t type;
	struct _pulse pulse;
	get_shmem_msg_t get_shmem;
	changed_shmem_msg_t changed_shmem;
} recv_buf_t;

void ipcMessagePassingReceive(int fd);
int create_shared_memory(unsigned nbytes, int client_pid, void **ptr, shm_handle_t *handle);
void ipcSharedMemoryReceive(int fd);
void ipcPipeReceive(int fd);

int main(int argc, char* argv[])
{
    int fd = -1;
    int longindex;
    int ch;
    char method;
    static struct option longopts[] = {
        { "help", no_argument,  NULL, 'h' },
		{ "messages", no_argument,  NULL, 'm' },
        { "queue", no_argument,  NULL, 'q' },
        { "pipe",   no_argument, NULL, 'p' },
        { "shm",   no_argument, NULL, 's' },
        { "file",   required_argument, NULL, 'f' },
        { NULL, 0, NULL, 0 }
    };

    while ((ch = getopt_long(argc, argv, "hmqpsf:", longopts, &longindex)) != -1) {

        switch (ch) {
        case 'f':
                if ((fd = open(optarg, O_WRONLY)) == -1)
                	printf("      Unable to open the file '%s'\n", optarg);
                //the target file chosen by receiver should be empty
                if(getFileSize(fd)!=0)
                	printf("The file should be empty!");
                break;
        case 'm':
        		if(method!=0){
        			printf ("You can only choose one kind of method every time\n");
        			return EXIT_FAILURE;
        		}
        		method='m';
                break;
        case 'p':
				if(method!=0){
					printf ("You can only choose one kind of method every time\n");
					return EXIT_FAILURE;
				}
				method='p';
				break;
        case 's':
				if(method!=0){
					printf ("You can only choose one kind of method every time\n");
					return EXIT_FAILURE;
				}
				method='s';
				break;
        //other situation
        case 'q':
        case 'h':
        default:
    	  		printf ("Usage: my_program [-m] for file transfer by message passing\n");
    	  		printf ("                  [-p] for file transfer by named pipe\n");
    	  		printf ("                  [-s] for file transfer by shared memory,");
    	  		printf ("                  [-f file_path] for choosing the target file\n");
    	  		printf ("                  [-h] for this help command\n");
    	  		printf ("                  note: sender and receiver should use the same method!\n");
                return EXIT_FAILURE;
        }
    }

    if(fd==-1){
    	printf("the file to receive information has not been provided or error happens!");
    	return EXIT_FAILURE;
    }
    switch (method){
    	case 'm':
    			ipcMessagePassingReceive(fd);
    			break;
    	case 'p':
    			ipcPipeReceive(fd);
    			break;
    	case 's':
    			ipcSharedMemoryReceive(fd);
    			break;
    	default:
    			return EXIT_FAILURE;
    }
	return EXIT_SUCCESS;
}


void ipcMessagePassingReceive(int fd){
	int rcvid;
	name_attach_t* attach;
	msg_buf_t msg;
	int status;
	int reply_status;
	char* data;

	attach = name_attach(NULL, CKSUM_SERVER_NAME, 0);
	if (attach == NULL)
	{
		perror("name_attach");
		exit(EXIT_FAILURE);
	}

	while (1)
	{
		printf("Waiting for a message...\n");
		rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), NULL);
		if (rcvid == -1)
		{
			perror("MsgReceive");
			exit(EXIT_FAILURE);
		}
		else if (rcvid > 0)
		{ //msg
			switch (msg.msg_type)
			{
			case CKSUM_IOV_MSG_TYPE:
				printf("Received a checksum request msg, header says the data is %d bytes\n",
						msg.cksum_hdr.data_size);
				data = malloc(msg.cksum_hdr.data_size);
				if (data == NULL)
				{
					if (MsgError(rcvid, ENOMEM ) == -1)
					{
						perror("MsgError");
						exit(EXIT_FAILURE);
					}
				}
				else
				{
					status = MsgRead(rcvid, data, msg.cksum_hdr.data_size, sizeof(cksum_header_t));
					if (status == -1)
					{
						perror("MsgRead");
						exit(EXIT_FAILURE);
					}


					int write_size = write(fd, data, msg.cksum_hdr.data_size);
				    if( write_size == -1 ) {
				        perror( "write error" );
						exit(EXIT_FAILURE);
				    }

					free(data);

					status = MsgReply(rcvid, EOK, &reply_status, sizeof(reply_status));
					if (status == -1)
					{
						perror("MsgReply");
						exit(EXIT_FAILURE);
					}
				}

				break;
			default:
				if (MsgError(rcvid, ENOSYS) == -1)
				{
					perror("MsgError");
				}
				break;
			}
		}
		else
		{
			printf("Receive returned an unexpected value: %d\n", rcvid);
		}
	}
}

void ipcPipeReceive(int fd){
    int fifofd;
    // FIFO file path
	int read_size=0;
    char * myfifo = "myfifo";
    // Creating the named file(FIFO)
    // mkfifo(<pathname>,<permission>)
    mkfifo(myfifo, 0666);
    char str1[80];
    fifofd = open(myfifo,O_RDONLY);
    while (read_size!=0)
    {    
		read_size = read(fifofd, str1, 80);
        write(fd, str1, read_size);
		sleep(1);
    }
	close(fd);
	free(str1);
}
/* create a secured shared-memory object, updating a handle to it. */

int create_shared_memory(unsigned nbytes, int client_pid, void **ptr, shm_handle_t *handle) {
	int fd;
	int ret;

	/* create an anonymous shared memory object */
	fd = shm_open(SHM_ANON, O_RDWR|O_CREAT, 0600);
	if( fd == -1 ) {
		perror("shm_open");
		return -1;
	}

	/* allocate the memory for the object */
	ret = ftruncate(fd, nbytes);
	if( ret == -1 ) {
		perror("ftruncate");
		close(fd);
		return -1;
	}

	/* get a local mapping to the object */
	*ptr = mmap(NULL, nbytes, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if(*ptr == MAP_FAILED) {
		perror("mmap");
		close(fd);
		return -1;
	}

	printf("fd is %d, client_pid is %d\n", fd, client_pid);

	/* get a handle for the client to map the object */
	ret = shm_create_handle( fd, client_pid, O_RDWR, handle, 0);

	if( ret == -1 ) {
		perror("shmem_create_handle");
		close(fd);
		munmap( *ptr, nbytes );
		return -1;
	}

	/* we no longer need the fd, so cleanup */
	(void)close(fd);

	return 0;
}

void ipcSharedMemoryReceive(int fd){
	int rcvid;
	recv_buf_t rbuf;
	int status;
	name_attach_t *att;
	struct _msg_info msg_info;
	int client_scoid = 0; // no client yet
	char *shmem_ptr;
	unsigned shmem_memory_size;
	get_shmem_resp_t get_resp;
	changed_shmem_resp_t changed_resp;
	char *resp;
	unsigned resp_len;

	// register our name
	att = name_attach(NULL, SHMEM_SERVER_NAME, 0);

	if (att == NULL) {
		perror("name_attach");
		exit(EXIT_FAILURE);
	}

	while (1) {
		rcvid = MsgReceive(att->chid, &rbuf, sizeof(rbuf), &msg_info );
		if (rcvid == -1) {
			perror("MsgReceive");
			exit(EXIT_FAILURE);
		} else {
			// we got a message
			switch (rbuf.type) {
			case GET_SHMEM_MSG_TYPE:
				shmem_memory_size = rbuf.get_shmem.shared_mem_bytes;
				if( shmem_memory_size > 64*1024 ) {
					MsgError(rcvid, EINVAL);
					continue;
				}
				status = create_shared_memory(shmem_memory_size,  msg_info.pid,  (void *)&shmem_ptr, &get_resp.mem_handle);
				if( status != 0 ) {
					MsgError(rcvid, errno);
					continue;
				}
				client_scoid = msg_info.scoid;

				status = MsgReply(rcvid, EOK, &get_resp, sizeof(get_resp));
				if (status == -1) {
					perror("MsgReply");
					(void)MsgError(rcvid, errno);
				}
				break;

			case CHANGED_SHMEM_MSG_TYPE:
			{
				if(msg_info.scoid != client_scoid) {
					// only the current client may tell us to update/change the memory
					(void)MsgError(rcvid, EPERM);
					continue;
				}

				const unsigned offset = rbuf.changed_shmem.offset;
				const unsigned nbytes = rbuf.changed_shmem.length;
				if( (nbytes > shmem_memory_size) || (offset > shmem_memory_size) || ((nbytes + offset) > shmem_memory_size)) {
					// oh no you don't
					MsgError(rcvid, EBADMSG);
					continue;
				}

				//printf("Got from client:\n");

				int write_size = write(fd, shmem_ptr+offset, nbytes);
			    if( write_size == -1 ) {
			        perror( "write error" );
					exit(EXIT_FAILURE);
			    }
				//write(STDOUT_FILENO, shmem_ptr+offset, nbytes);
				//write(STDOUT_FILENO, "\n", +1 );

				changed_resp.offset = nbytes+0; // 2nd page for answer, 0 offset into page is arbitrary
				resp = "0";
				resp_len = strlen(resp);
				memcpy(shmem_ptr+changed_resp.offset, resp, resp_len);
				changed_resp.length = resp_len;
				printf("before change response");

				status = MsgReply(rcvid, EOK, &changed_resp, sizeof(changed_resp));

				if (status == -1) {
					// reply failed... try to unblock client with the error, just in case we still can
					perror("MsgReply");
					(void)MsgError(rcvid, errno);
				}
				printf("after change response");
				break;
			}

			case RELEASE_SHMEM_MSG_TYPE:
				printf("Just enter the release");
				if(msg_info.scoid != client_scoid) {
					// only the current client may tell us to release the memory
					(void)MsgError(rcvid, EPERM);
					continue;
				}
				client_scoid = 0;
				printf("before munmap");
				munmap(shmem_ptr, shmem_memory_size);
				status = MsgReply(rcvid, EOK, NULL, 0);
				if (status == -1) {
					// reply failed... try to unblock client with the error, just in case we still can
					perror("MsgReply");
					(void)MsgError(rcvid, errno);
				}
				printf("after munmap");
				break;

			default:
				// unknown message type, unblock client with an error
				if (MsgError(rcvid, ENOSYS) == -1) {
					perror("MsgError");
				}
				break;
			}
		}
	}
}


