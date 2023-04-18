#include <stdio.h>
#include <errno.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]){
	
    int wf;
    const char *buf_sf;
    ssize_t nr;
    wf = open(argv[1], O_RDWR | O_CREAT, 0664);
    printf("\n");
    buf_sf = argv[2];
    if(wf == -1){
        perror("return Error in open(): ");
	return 1;
    }else{
        nr = write(wf, buf_sf, strlen(buf_sf));
        if(nr == -1){
            perror("return Errot write");
	    return 1;
        }
    }
    openlog(NULL, 0, LOG_USER);
    syslog(LOG_DEBUG, "Writing %s to %s \n", buf_sf, argv[1]);
    syslog(LOG_ERR, "Error has occured");
}
