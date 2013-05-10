/*
 * proxy.c - CS:APP Web proxy
 *
 * TEAM MEMBERS:
 *     Andrew Carnegie, ac00@cs.cmu.edu 
 *     Harry Q. Bovik, bovik@cs.cmu.edu
 * 
 * IMPORTANT: Give a high level description of your code here. You
 * must also provide a header comment at the beginning of each
 * function that describes what that function does.
 */ 

 //Src Sited: Following functions contain codes from CS:APP Ch11, 12
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <stdio.h>
#include "sthread.h"
#include "csapp.h"

/* Number of threads in pool for part 2 of project */
#define NTHREADS 16
#define P_LOG "proxy.log"
 


/*
 * Function prototypes
 */
void fetch(int connfd, struct sockaddr_in clientaddr);
int parse_uri(char *uri, char *target_addr, char *path, int  *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
void echo(int connfd);
void *thread(void *targ);
void log_write(char * uri, int resp_bytes, struct sockaddr_in clientaddr);

// //global variables
FILE *log_file;

struct thread_info 
{   
   struct sockaddr_in clientaddr;
   int thread_id;
   int connfd;
   struct thread_info * next;
};

struct thread_info * job_list;

/* 
 * main - Main routine for the proxy program 
 */
 
int main(int argc, char **argv)
{

    /* Check arguments */

    //we need to create an error message, formated like the E2 example. 
    //Content length is the length of the response/error msg
    int client_fd, listen_fd, proxy_port, server_port, clientlen, n;
    char host[MAXLINE], buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], pathname[MAXLINE];
  
    struct thread_info * t_info;
    struct sockaddr_in temp_client_addr; 
    int i = 0;
    sthread_t threads [NTHREADS];
	
    smutex_init(&log_tex);
	smutex_init(&client_tex);
    //use similar concept as page_free_list ()
    //to hand out threads
    //When a thread is finished w/ a request
    //1. Broadcast() done
    //2. Add this thread to free_thread_list()


    if(argc !=2)
    {
         fprintf(stderr, "Usage: %s <port>\n", argv[0]);
         exit(0);
    }
    proxy_port=atoi(argv[1]);
    //1. Open and ret listening descriptor for "server"-side proxy
    listen_fd=Open_listenfd(proxy_port);
    // log_file = Fopen(P_LOG, "a");
    clientlen=sizeof(temp_client_addr);
    //instantiate 16 threads

    while(1)
    {
        t_info = Malloc(sizeof(struct thread_info));
        t_info->connfd = Accept(listen_fd, (SA *) &(t_info->clientaddr), (socklen_t *) &clientlen);
        t_info->next=NULL;
        if (i < NTHREADS)
        {
            printf("the i is: %d\n", i);
            t_info->thread_id=i;
            sthread_t per_thread;
            threads[i]=per_thread;
            sthread_create(&threads[i], thread, t_info);
            i++;
        }
        else
        {
            //add t_info into the queue
            enqueue(&t_info);
        }
    }
}

void enqueue(struct thread_info ** t_info)
{
    //If job_list is EMPTY
    if (!job_list)
    {
        job_list=*t_info;
    }
    else
    {
        //temp info is the cursor, iterating through the loop
        struct thread_info * temp_info=job_list;

        //this loop is here to find the tail
        while (temp_info->next!=NULL)
        {
            temp_info=temp_info->next;
        }
        temp_info->next=*t_info;
       
    }
}

struct thread_info* dequeue(void)
{
    //If job_list is EMPTY
    if (!job_list)
    {
        return NULL;
    }
    else
    {
        struct thread_info *temp = job_list;
        job_list = temp->next;
        return temp;
    }
}

//this is the start_routine that we pass to thread_create
//targ is the argument to thread_start, in our case, it is the connfd
//we will call fetch in here

//thread will need a while(1) loop
//the top of which must dequeue the queue, and take that struct thread_info as it's new job
void *thread(void *vargp)
{
    //a flag for checking when we're running the thread for the first time
    int first_time = 1;
    struct sockaddr_in clientaddr;
    int thread_id;
    int connfd;

    while(1)
    {
        if(first_time)
        {
            clientaddr= ((struct thread_info *)vargp)->clientaddr;
            thread_id= ((struct thread_info *)vargp)->thread_id;
            connfd = ((struct thread_info *)vargp)->connfd;
            Free(vargp);

            first_time = 0;
			printf("The thread_id is %d\n",thread_id);
			fetch(connfd, clientaddr);
        }
        else
        {
            struct thread_info *temp = dequeue();
			if (temp)
			{
				clientaddr= temp->clientaddr;
				thread_id= temp->thread_id;
				connfd = temp->connfd;
				Free(temp);
				
				printf("$$$The thread_id is %d\n",thread_id);
				fetch(connfd, clientaddr);
			}
        }
    }
    
    return NULL;
}

void log_write(char * uri, int resp_bytes, struct sockaddr_in clientaddr)
{   
    smutex_lock(&log_tex);

    //URL is host, and we now know size
    //date time is determined by the call to format_log_entry
    //we need to get the browser/client IP
    char log_entry[MAXLINE];
    //write log_entry into the file, and next line if the format function doesn't add one

    format_log_entry(log_entry, &clientaddr, uri, resp_bytes);
    printf("log_entry: %s\n", log_entry);

    log_file=Fopen(P_LOG, "a+");
                
    // printf("log_file: %d\n", log_file);
    Fwrite(log_entry, 1, strlen(log_entry), log_file);
                
    Fclose(log_file);
    smutex_unlock(&log_tex);
}

void fetch(int connfd, struct sockaddr_in clientaddr)
{
    int client_fd, server_port, clientlen, n, m;
    char host[MAXLINE], buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], pathname[MAXLINE];    
    char request[MAXLINE]="";
    rio_t rio_p, rio_s;
    Rio_readinitb(&rio_p, connfd);
	
    if ((n = Rio_readlineb_w(&rio_p, buf, MAXLINE)) != 0)
    {
        // printf("buf: %s\n", buf);
        sscanf(buf, "%s %s %s", method, uri, version);
        if (strcasecmp(method, "GET"))
        {
			printf("***!GET: call echo()\n");
            printf("buf: %s\n", buf);
            echo(connfd);
        }
        else if (parse_uri(uri, host, pathname, &server_port) == -1)
        {
			 printf("***parse uri failed: call echo()\n");
             printf("buf: %s\n", buf);
             echo(connfd);
        }   
        else
        {
            // printf("parse URI passed\n");
            m=n;
            // printf("about to strncat\n");
            memmove(request, buf, m); // strcat first m chars of buf
            // printf("1st line: request:\n%s\n",request);
            while ((n= Rio_readlineb_w(&rio_p, buf, MAXLINE)) != 0)
            {
                if (strcmp(buf, "\r\n"))
                {
                    if (strncmp(buf, "Connection", 10) && strncmp(buf, "Accept-Encoding",15) && strncmp(buf, "Proxy-Connection",16 ))
                    {
                        memmove(request+m, buf, n); 
                        m+=n;
                    } 
                }
                else
                {
                    break;
                }
            }
            //client mode starts
            //"client" proxy sends GET to web server
			//write lock/unlock version for open_cli
            client_fd = Open_clientfd_ts(host, server_port);
                
            Rio_readinitb(&rio_s, client_fd);
            memmove(request+m, "\r\n\r\n", 4);
            m+=2;    
            printf("request:\n%s\n",request);
            Rio_writen_w(client_fd, request, m);
            //client mode ends
              
            ssize_t resp_bytes=0;
            ssize_t temp_bytes=0;
            while ((temp_bytes=Rio_readlineb_w(&rio_s, buf, MAXLINE)) > 0 )
            {  
                Rio_writen_w(connfd, buf, temp_bytes); //check ret val. 
				resp_bytes+=temp_bytes;     
            }
            printf("resp_bytes: %d\n", resp_bytes);
            
            log_write(uri, resp_bytes, clientaddr);
			
            printf("about to close client_fd\n");
            Close(client_fd);
        }
        printf("about to close conn_fd\n");
        Close(connfd);
    }
}

void echo(int connfd)
{
    size_t n;
    char buf[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, connfd);
   
    printf("server received %d bytes\n",  n);    
     //declare static string to replace buf
    static char err_msg[]="HTTP/1.0 400 Bad Request\nContent-Length: 162\nContent-Type: text/html\n\n";
    n= sizeof(err_msg);
    Rio_writen_w(connfd, err_msg, n);
    static char err_msg2[]= "<html><head>\n<title>400 Bad Request</title>\n</head><body>\n<h1>Bad Request</h1>\n<p>Your browser sent a request that this server could not\nunderstand.<br />\n</p>\n</body></html>\n";
    n= sizeof(err_msg2);
    Rio_writen_w(connfd, err_msg2, n);
}


/*
 * parse_uri - URI parser
 * 
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, int *port)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0) {
	hostname[0] = '\0';
	return -1;
    }
   
    /* Extract the host name */
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);

    hostname[len] = '\0'; 
    /* Extract the port number */
    *port = 80; /* default */
    if (*hostend == ':')   
	*port = atoi(hostend + 1);
    
    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
	pathname[0] = '\0';
    }
    else {
	pathbegin++;	
	strcpy(pathname, pathbegin);
    }

    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring. 
 * 
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, 
		      char *uri, int size)
{
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    /* 
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 13, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;


    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s %d\n", time_str, a, b, c, d, uri, size);
}


