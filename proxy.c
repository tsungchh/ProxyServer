/*
 * I. Proxy Part:
 * 1. Proxy wait to accpet client connection
 * 2. Read client request line and header
 * 3. Check cache
 * 4. If not, forward request line and header to sever.
 * 5. Read the server's response
 * 6. Write server's response into client file.
 *
 * II. Cache Part:
 * 1. Cache is a single link list: cachehead->cache->cache->NULL
 * 2. Cache tag is calculate by crc 32 from some online opensource
 * 3. Newest Cache save is the front
 * 4. When reach max size start deleting from the tail (LRU)
 *
 * III. Concurrent Part:
 * 1. Use multithread to process client's request.
 * 2. Cache variables shared between thread
 * 3. Might be better to apply multiplexing i/o
 */
#include "csapp.h"
#include <stdio.h>
#include "proxy.h"

static const char *user_agent = "User-Agent: Mozilla/5.0 (X11; Linux x86_64;\
                                rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept12 = "Accept: text/html,application/xhtml+xml,\
                            application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding = "Accept-Encoding: gzip, deflate\r\n";
static const char *Connection = "Connection: close\r\n";
static const char *Proxy_connection = "Proxy-Connection: close\r\n";

static pthread_mutex_t mutex;
static unsigned int total_cache_size;
static Cache *cachehead;
/* CRC32 tag encode, used for hashing */
static unsigned long crc32_tab[] = {
    0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
    0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
    0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
    0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
    0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
    0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
    0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
    0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
    0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
    0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
    0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
    0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
    0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
    0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
    0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
    0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
    0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
    0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
    0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
    0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
    0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
    0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
    0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
    0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
    0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
    0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
    0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
    0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
    0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
    0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
    0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
    0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
    0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
    0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
    0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
    0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
    0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
    0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
    0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
    0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
    0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
    0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
    0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
    0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
    0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
    0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
    0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
    0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
    0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
    0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
    0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
    0x2d02ef8dL
};

void init_cache() {
    cachehead = (Cache*)Malloc(sizeof(Cache));
    cachehead->next_cache = NULL;
    cachehead->content_size = 0;
    cachehead->tag = 0;
    total_cache_size = 0;
}

unsigned long create_tag(char *request, unsigned long request_size) {
   unsigned int i, crc32val = 0;
   for (i = 0;  i < request_size;  i++) {
      crc32val = crc32_tab[(crc32val ^ request[i]) & 0xff] ^ (crc32val >> 8);
   }
   return crc32val;
}

void add_cache(char *request, char *response_content, long int content_size) {
    
    Cache *cache;
    cache = (Cache*)Calloc(1, sizeof(Cache));
    cache->content = Calloc(1, sizeof(char) * content_size);
    
    if(cache->content ==NULL) {
        free(cache);
        fprintf(stderr, "out of mememory\n");
        exit(1);
    }
    
    memcpy(cache->content, response_content, content_size);
    cache->content_size = content_size;
    cache->tag = create_tag(request, strlen(request));
    cache->next_cache = cachehead->next_cache;
    cachehead->next_cache = cache;
    total_cache_size += cache->content_size;
    return;
}

void save_cache(char *request, char *response_content, long int content_size) {

    pthread_mutex_lock(&mutex);
    Cache *cache, *prev_cache;
    cache = cachehead->next_cache;
    
    while (total_cache_size + content_size > MAX_CACHE_SIZE) {
    /* delete tail cache until there is enough space */
        cache = cachehead;
        while(cache->next_cache != NULL) { /* Find last cache */
            prev_cache = cache; /* Temping prev cache */
            cache = cache->next_cache;   
        }
        total_cache_size -= cache->content_size;
        prev_cache->next_cache = NULL;
        free(cache->content);
        free(cache);
    }   
    add_cache(request, response_content, content_size);
    pthread_mutex_unlock(&mutex);
    return;
}

int check_cache(char *request, int Client_FD) {

    pthread_mutex_lock(&mutex);
    Cache *cache, *prev_cache;
    unsigned long tag = create_tag(request, strlen(request));
    prev_cache = cachehead;
    cache = cachehead->next_cache;
    
    while(cache != NULL) {
        if(cache->tag == tag) {
            prev_cache->next_cache = cache->next_cache;
            cache->next_cache = cachehead->next_cache;
            cachehead->next_cache = cache;	
            rio_writen(Client_FD, cache->content, cache->content_size);

            //find cache already, then close it.
            close(Client_FD);
            pthread_mutex_unlock(&mutex);
            return 1;
        }
        else {
            prev_cache = cache; 
            cache = cache->next_cache;
        }
    }

    pthread_mutex_unlock(&mutex);
    return 0;
}   

void Read_response(char *request, int Server_FD, int Client_FD) { 

    // Read response from server, forward to cliend, save it to cache
    size_t n;
    char header_buffer[MAXLINE];
    char content_buffer[MAX_OBJECT_SIZE];
    memset(content_buffer, 0, MAXLINE);
    char *response_content;
    rio_t server_rio;
    rio_readinitb(&server_rio, Server_FD);
    long int total_length = 0;
    
    while ((n = rio_readnb(&server_rio, header_buffer, MAXLINE)) != 0) {
        if(total_length + n < MAX_OBJECT_SIZE)
            memcpy(content_buffer + total_length, header_buffer, n);
        rio_writen(Client_FD, header_buffer, n);
        total_length += n;
    }
    
    if (total_length < MAX_OBJECT_SIZE) {
        response_content = Calloc(1, sizeof(char) * total_length);
        memcpy(response_content, content_buffer, sizeof(char) * total_length);

        save_cache(request, response_content, total_length);
        free(response_content);
    }
}

int Forward_request(int Client_FD,char* request, char* request_rest, char *host) {
    // Forward client's request to the server 
    int Server_FD;
    int port = 80;
    char hostConnet[MAXLINE];
    
    strncpy(hostConnet, host + 6, strlen(host + 6));
    hostConnet[strlen(host + 6) - 2] = '\0';

    if( hostConnet[0] == '\0' )
        return 1;

    if ((Server_FD = open_clientfd(hostConnet, port)) < 0) {
        fprintf(stderr, "Warning connection refused! Server %s port %d\n", hostConnet, port);
        return -1;
    }

    /* Write request line to sever */
    char writecontent[MAXLINE];
    memset(writecontent, 0, MAXLINE);
    strcat(writecontent, request_rest);
    strcat(writecontent, "\r\n");
    writecontent[strlen(writecontent)] = '\0';
    rio_writen(Server_FD, writecontent, strlen(writecontent));
    
    /* Read the server's response */
    Read_response(request, Server_FD, Client_FD);
    Close(Server_FD);
    Close(Client_FD);
    return 1;
}

void Read_request(void *arg) { 

    //Read request from client 
    Pthread_detach(pthread_self());
    int Client_FD = *(int *)arg;
    

    rio_t Client_rio;
    char request[MAXLINE];
    char request1[MAXLINE];
    char host[MAXLINE];
    char *str;
    char buf[MAXLINE];
    char newRequest[MAXLINE];
    memset(request1, 0, MAXLINE);
    memset(buf, 0, MAXLINE);
    memset(host, 0, MAXLINE);
    memset(newRequest, 0, MAXLINE);
    
    rio_readinitb(&Client_rio, Client_FD);
    rio_readlineb(&Client_rio, request, MAXLINE);
   
    int i = 0, num = 0;
    /* Parsing */
    while(1 && num < MAXLINE-1) {
        if (request[num] == '/')
            i++;
        if (i == 3) {
            break;
        }
        num++;
    }
    strcat(request1, "GET ");
    //Get hostname   
    strcat(request1, request+num);
    
    if ((str = strstr(request1, "HTTP/1.1")) != NULL) {
        strncpy(str, "HTTP/1.0", 8);
    }
    rio_readlineb(&Client_rio, host, MAXLINE);
    int total_length = 0;
    ssize_t judgeEND;
    
    memcpy(newRequest + total_length, request1, strlen(request1));
    total_length=total_length+strlen(request1);
    memcpy(newRequest + total_length, host, strlen(host));
    total_length=total_length+strlen(host);
    memcpy(newRequest + total_length, user_agent, strlen(user_agent));
    total_length=total_length+strlen(user_agent);
    memcpy(newRequest + total_length, accept12, strlen(accept12));
    total_length=total_length+strlen(accept12);
    memcpy(newRequest + total_length, accept_encoding, strlen(accept_encoding));
    total_length=total_length+strlen(accept_encoding);
    memcpy(newRequest + total_length, Connection, strlen(Connection));
    total_length=total_length+strlen(Connection);
    memcpy(newRequest + total_length, Proxy_connection, strlen(Proxy_connection));
    total_length=total_length+strlen(Proxy_connection);
    
    while(1) {
        
        judgeEND = rio_readlineb(&Client_rio, buf, MAXLINE);
        if ((strstr(buf,"User-Agent:") != NULL)         ||
            (strstr(buf,"Accept:") != NULL)             ||
            (strstr(buf,"Accept-Encoding:") != NULL)    ||
            (strstr(buf,"Connection:") != NULL)         ||
            (strstr(buf,"Proxy-Connection:") != NULL)) {
            /* Repeated request line */
        }
        else {
            memcpy(newRequest + total_length, buf, strlen(buf));
            total_length = total_length + strlen(buf);
        }
        
        if ((judgeEND == 2) || (judgeEND == 0)) {
            break;
        }
    }

    if( !check_cache(request, Client_FD) ) {
        //Cache Hit, cache directly send content to client 
        Forward_request(Client_FD, request, newRequest, host);
    }
    Pthread_exit(NULL); 
}

int main(int argc, char **argv) {
    
    int listenfd, port, clientlen;
    int connfd_thread;
    struct sockaddr_in clientaddr;
    pthread_t tid;    
    init_cache(); 

    Signal (SIGPIPE, SIG_IGN); 
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    port = atoi(argv[1]);
    listenfd = Open_listenfd(port);
    pthread_mutex_init(&mutex, NULL);
    while (1) {
        clientlen = sizeof(clientaddr);

        connfd_thread = Accept(listenfd, (SA *)&clientaddr,\
                                (socklen_t *)&clientlen);

        Pthread_create(&tid, NULL, (void *)Read_request,\
                       (void *) &connfd_thread);
    }
    
    pthread_exit(NULL);
    return 0;
}


