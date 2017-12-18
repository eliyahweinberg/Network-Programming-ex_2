//
//  main.c
//  ex_2
//
//  Created by Eliyah Weinberg on 11.12.2017.
//  Copyright © 2017 Eliyah Weinberg. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define CONECTION_CLOSE

#define DEFAULT_PORT 80
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define USAGE "Usage: client [-h] [-d <time- interval>] <URL> \n"
#define GET 0
#define HEAD 1
#define HTTP 2
#define HOST 3
#define IF_MODIFIED 4
#define SEPARATOR 5
#define CCLOSE 6
#define TIMEBUF 128
#define READBUF 512
#define SUCESS 0
#define WRONG_USAGE 1
#define FAILURE -1
#define DEFAULT -1
#define TRUE 'y'

/*Represents http request data*/
struct request_args {
    char header_h;
    char header_d;
    char timebuf[TIMEBUF];
    int port;
    char* host;
    char* path;
};
typedef struct request_args request_args_t;

//----------------------------------------------------------------------------//
//---------------------FUNCTION DECLARATION-----------------------------------//
//----------------------------------------------------------------------------//

/* Initilizes request_args_t (client_args_t* should be allocated)
 * headers set to 'n' on default
 * port set to '-1'
 \param new is struct need to be initilized
 * Returns 0 on sucess -1 on failure */
int init_request_args(request_args_t* new);

/* Parsing given request to arguments
 * request_args_t* should be allocated and initilized
 \param request is for parsed command
 \param argc is number of received arguments
 \param argv[] is arguments data
 * Returns 0 on sucess -1 on wrong input 1 on wrong usage */
int parse_request(request_args_t* request, int argc, const char * argv[]);

/*Parsing time string to RFC1123FMT format
 \ param time is required time string
 \ param timebuf is buffer for formated time
 * timebuf should be allocated
 * Returns 0 on sucess -1 on wrong input 1 on wrong usage */
int parse_time(const char* time_s, char* timebuf);

/*Parsing uri string to arguments format
 \ param request is for parsed command
 \ param string is request to be parsed
 * Returns 0 on sucess -1 on wrong input 1 on wrong usage*/
int parse_uri(request_args_t* request,const char* string);

/*Connecting to socket specified by host and port
 \ param request is connection data
 * Returns socket file descriptor on sucess -1 on failure */
int connect_to_server(request_args_t* request);

/*Creating http 1.1 request by given parameters
 \param request is required parameters
 * Returns allocated string with request on sucess NULL on failure */
char* build_http_request(request_args_t* request);

/*Checking if in memory block enought space to concatenate data
 \ param allocated is allocated memory block
 \ param c_size size of current use of allocated memory block
 \ param a_size size of current memory allocation
 \ param req_size size is lenght of data to writen
 * Retuns 0 on sucess -1 on failure */
int is_enough_allocted(char** req, char** ptr, int* a_size,
                       int c_size, int req_size);

/*String concatination dest pointer is set to the end of the copied data
 * Returns number of chars written
 */
int strconcat(char** dest, const char* source, int lenght);

/*Dealocating all memory used for parsed request*/
void dealloc_request(request_args_t* request);

//----------------------------------------------------------------------------//
//------------------------------M A I N---------------------------------------//
//----------------------------------------------------------------------------//
int main(int argc, const char * argv[]) {
    request_args_t* req;
    int command_status;
    int sock_fd;
    int request_length;
    int sent = 0, rc = 1, sent_offset = 0, total_read = 0;
    int stdout_offset, wc = 1;
    char* request = NULL;
    char buffer[READBUF];
    if( !(req = (request_args_t*)malloc(sizeof(request_args_t))) ) {
        perror("client_args_t allocation failed");
        exit(FAILURE);
    }
    init_request_args(req);
    
    command_status = parse_request(req, argc, argv);

    if (command_status == WRONG_USAGE) {
        printf(USAGE);
        dealloc_request(req);
        exit(SUCESS);
    } else if (command_status == FAILURE){
        printf("wrong input \n");
        dealloc_request(req);
        exit(FAILURE);
    }
    
    request = build_http_request(req);
    if (!request) {
        dealloc_request(req);
        exit(FAILURE);
    }
    
    sock_fd = connect_to_server(req);
    if (sock_fd == FAILURE){
        free(request);
        dealloc_request(req);
        exit(FAILURE);
    }
    request_length = (int)strlen(request);
    printf("HTTP request =\n%s\nLEN = %d\n", request, request_length);
    
    /* sending request */
    while (sent_offset < request_length){
        sent = write(sock_fd, request+sent_offset, request_length-sent_offset);
        sent_offset += sent;
    }
    
    /*receiving responce*/
    memset(buffer, '\0', sizeof(buffer));
    while (rc > 0) {
        stdout_offset = 0;
        rc = read(sock_fd, buffer, sizeof(buffer));
        if (rc == FAILURE)
            perror("reading from socket error");
        
        total_read += rc;
        
        //printing to stdout
        while (stdout_offset < rc ) {
            wc = write(STDOUT_FILENO, buffer+stdout_offset, rc-stdout_offset);
            stdout_offset += wc;
        }
        
        
    }
    

    printf("\n Total received response bytes: %d\n",total_read);
    
    
    close(sock_fd);
    free(request);
    dealloc_request(req);
    return 0;
}

//----------------------------------------------------------------------------//
int init_request_args(request_args_t* new){
    new->header_d = '\0';
    new->header_h = '\0';
    new->port = DEFAULT;
    new->host = NULL;
    new->path = NULL;
    return SUCESS;
}

//----------------------------------------------------------------------------//
int parse_request(request_args_t* request, int argc,const char * argv[]){
    if (argc < 2 || (argc == 2 && argv[1][0] != 'h'))
       return FAILURE;
    int i;
    int time_flag;
    int uri_flag;
    const char* char_ptr;

    for (i=1; i<argc; i++) {
        char_ptr = argv[i];// checking stirng of current argument
        //argument is flags
        if (*char_ptr == '-'){
            char_ptr++;
            if (*char_ptr == 'h')
                request->header_h = TRUE;
            else if (*char_ptr == 'd'){
                request->header_d = TRUE;
                i++;
                if (i >= argc)
                    return WRONG_USAGE;
                
                time_flag = parse_time(argv[i], request->timebuf);
                if (time_flag != SUCESS)
                    return time_flag;
                
            }
            else
                return FAILURE;
        }
        //argument is URL
        else if (!request->host){
            uri_flag = parse_uri(request, argv[i]);
            if (uri_flag != SUCESS)
                return FAILURE;
        }
        
        else
            return WRONG_USAGE;
    }
    
    return SUCESS;
}

//----------------------------------------------------------------------------//
int parse_time(const char* time_s, char* timebuf){
    time_t now;
    int day, hour, min;
    int assignment_flag;
    
    assignment_flag = sscanf(time_s, "%d:%d:%d", &day, &hour, &min);
    if (assignment_flag != 3 /*|| day<0 || hour <0 || min < 0*/)
        return FAILURE;
    
    now = time(NULL);
    now=now-(day*24*3600+hour*3600+min*60);//where day, hour and min are the values
                                                            //from the input
    strftime(timebuf, TIMEBUF, RFC1123FMT, gmtime(&now));
    
#ifdef DEBUG_C
    printf("%s \n", timebuf);
#endif

    return SUCESS;
}

//----------------------------------------------------------------------------//
int parse_uri(request_args_t* request,const char* string){
    const char *http = "http://";
    
    char *ptr;
    int i,j;
    for(i=0; i<7 && string[i] == http[i]; i++);
    if (i != 7) //URI prefix not 'http://'
        return FAILURE;
    string += i;
    
    for (i=0; string[i] && string[i] != '/'; i++);
    if (string[i] != '/') //URI not ends with '/'
        return FAILURE;
    
    
    //allocating host adress memory
    request->host = (char*)malloc(sizeof(char)*(i+1));
    if (!request->host){
        perror("host adrees memory alloc failed");
        return FAILURE;
    }
    
    for (j=0; j < i; j++)
        request->host[j] = string [j];
    request->host[i] = '\0';
    
    if((ptr = strchr(request->host, ':'))){  //required different port
        ptr++;
        request->port = atoi(ptr);
        if (request->port == 0)
            return FAILURE;
    }
    
    string += i;
    if (*string)
        request->path = strdup(string);
    if (!request->path) {
        perror("path adrees memory alloc failed");
        return FAILURE;
    }
    
#ifdef DEBUG_C
    printf("host: %s \n", request->host);
    if (request->path)
        printf("path: %s\n", request->path);
    printf("port: %d \n", request->port);
#endif
    
    return SUCESS;
}

//----------------------------------------------------------------------------//
void dealloc_request(request_args_t* request){
    if (request->host)
        free(request->host);
    if (request->path)
        free(request->path);
    free(request);
}

//----------------------------------------------------------------------------//
int connect_to_server(request_args_t* request){
    int sock_fd;
    struct sockaddr_in serv_addr;
    struct hostent* server;
    char *host = NULL;
    
    /*creating FD for new socket*/
    sock_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0){
        perror("socket FD creation failed");
        return FAILURE;
    }
    
    if (request->port != DEFAULT) { //removing host number for gethostbyname()
        host = strdup(request->host);
        char *ptr = strchr(host, ':');
        *ptr = '\0';
    }
    else
        host = request->host;
    
    server = gethostbyname(host);
    if (!server){
        herror("unable to get host");
        if (request->port != DEFAULT)
            free(host);
        return FAILURE;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons((request->port == DEFAULT ?
                                    DEFAULT_PORT : request->port));
    bcopy((char*)server->h_addr,
          (char*)&serv_addr.sin_addr.s_addr, server->h_length);
    
    /*establishing connection*/
    if (connect(sock_fd,
                (const struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("unable establish connection");
        if (request->port != DEFAULT)
            free(host);
        return FAILURE;
    }
    
#ifdef DEBUG_C
    printf("connection established \n");
#endif
    
    if (request->port != DEFAULT)
        free(host);
    
    return sock_fd;
}

//----------------------------------------------------------------------------//
char* build_http_request(request_args_t* request){
    int request_len = 0;
    int a_block_lenght = 256;
    int index;
    char* request_s;
    char* ptr;
    int required_size;
    const char *headers[] ={"Get ", "HEAD ", " HTTP/1.1", "Host: ",
        "If-Modified-Since: ", "\r\n", "Connection: close"};
    
    request_s = (char*)malloc(sizeof(char)*a_block_lenght);
    if (!request_s){
        perror("HTTP request malloc failed");
        return NULL;
    }
    ptr = request_s;

    if(request->header_h)   //Head header required
        index = HEAD;
    
    else                    //Get header required
        index = GET;
    
    request_len += strconcat(&ptr, headers[index],(int)strlen(headers[index]));
    
    /*checking if enought memory allocated*/
    required_size = (int)strlen(request->path)
                    + (int)strlen(headers[HTTP])
                    + (int)strlen(headers[SEPARATOR])*2
                    + (int)strlen(headers[HOST])
                    + (int)strlen(request->host);
    if(is_enough_allocted(&request_s, &ptr, &a_block_lenght,
                           request_len, required_size)!=SUCESS )
        return NULL;
    /*writing host an path headers*/
    request_len += strconcat(&ptr, request->path, (int)strlen(request->path));
    request_len += strconcat(&ptr, headers[HTTP], (int)strlen(headers[HTTP]));
    request_len += strconcat(&ptr, headers[SEPARATOR],
                             (int)strlen(headers[SEPARATOR]));
    request_len += strconcat(&ptr, headers[HOST], (int)strlen(headers[HOST]));
    request_len += strconcat(&ptr, request->host, (int)strlen(request->host));
    request_len += strconcat(&ptr, headers[SEPARATOR],
                             (int)strlen(headers[SEPARATOR]));
    
    /*writing 'if modified sience' header*/
    if (request->header_d){
        required_size = (int)strlen(headers[IF_MODIFIED])
                        + (int)strlen(request->timebuf)
                        + (int)strlen(headers[SEPARATOR]);
        if(is_enough_allocted(&request_s, &ptr, &a_block_lenght,
                              request_len, required_size)!=SUCESS )
            return NULL;
        request_len += strconcat(&ptr, headers[IF_MODIFIED],
                                 (int)strlen(headers[IF_MODIFIED]));
        request_len += strconcat(&ptr, request->timebuf,
                                 (int)strlen(request->timebuf));
        request_len += strconcat(&ptr, headers[SEPARATOR],
                                 (int)strlen(headers[SEPARATOR]));
    }
    
#ifdef CONECTION_CLOSE
    /*writing connection header*/
    required_size = (int)strlen(headers[CCLOSE])
                    + (int)strlen(headers[SEPARATOR]);
    if(is_enough_allocted(&request_s, &ptr, &a_block_lenght,
                          request_len, required_size)!=SUCESS )
        return NULL;
    request_len += strconcat(&ptr, headers[CCLOSE],
                             (int)strlen(headers[CCLOSE]));
    request_len += strconcat(&ptr, headers[SEPARATOR],
                             (int)strlen(headers[SEPARATOR]));
#endif
    
    if(is_enough_allocted(&request_s, &ptr, &a_block_lenght,
                          request_len,
                          (int)strlen(headers[SEPARATOR]))!=SUCESS )
        return NULL;
    
    strcpy(ptr, headers[SEPARATOR]);
    
    return request_s;
}


//----------------------------------------------------------------------------//
int is_enough_allocted(char** req, char** ptr, int* a_size,
                       int c_size, int req_size){
    int allocated = *a_size;
    while (c_size+req_size >= allocated)
        allocated *= 2;
    
    if (allocated != *a_size){
        *a_size = allocated;
        *req = (char*)realloc(*req, sizeof(char)*allocated);
        if (!*req) {
            perror("realloc failed");
            return FAILURE;
        }
        *ptr = *req+c_size;
    }
    return SUCESS;
}

//----------------------------------------------------------------------------//
int strconcat(char** dest,const char* source, int lenght){
    strncpy(*dest, source, lenght);
    *dest += lenght;
    return lenght;
}




