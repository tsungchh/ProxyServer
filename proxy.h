#ifndef __PROXY_H__
#define __PROXY_H__

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define DEBUG
#define THREAD

typedef struct Cache Cache;
struct Cache{ /* structure of cache node */
    unsigned long tag;
    char *content;
    long int content_size;
    Cache *next_cache;
};

/* Create a tag based on request line */
unsigned long create_tag(char *request, unsigned long request_size);

/* Cache handlers for visited page */
int  check_cache(char *request, int Client_FD);
void save_cache (char *request, char *response_content, long int content_size);
void add_cache  (char *request, char *response_content, long int content_size);
void init_cache ();

/* Request, response handlers */
void Read_request   (void *arg);
void Read_response  (char *request, int Server_FD, int Client_FD);
int  Forward_request(int Client_FD,char* request, char* request_rest, char *host);

#endif /* __PROXY_H__ */