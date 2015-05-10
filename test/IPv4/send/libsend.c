#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "sendrecvUDP.h"


/* Need to add proper header file. */
#define SO_EE_ORIGIN_LOCAL_NOTIFY	5


int shared_descriptor;

int is_shared_instance_instantiated: 1;

int is_shared_instance_ipv6: 1;



struct sockaddr_in ipv4_destination_address;

struct sockaddr_in6 ipv6_destionation_address;





int instantiate_ipv4_shared_instance_by_address_and_port(char *address, int port)
{
    int error;
    
    is_shared_instance_ipv6 = 0;
    is_shared_instance_instantiated = 1;
    
    error = create_ipv4_socket(address,port,&shared_descriptor, &ipv4_destination_address);
    return error;
}


int instantiate_ipv6_shared_instance_by_address_and_port(char *address, int port)
{
    int error;
    
    is_shared_instance_ipv6 = 1;
    is_shared_instance_instantiated = 1;
    
    error = create_ipv6_socket(address,port,&shared_descriptor, &ipv6_destionation_address);
    return error;
}


void release_shared_instance(void)
{
    shared_descriptor = 0;
    is_shared_instance_instantiated = 0;
    is_shared_instance_ipv6 = 0;
    
    ipv4_destination_address = NULL;
    ipv6_destionation_address = NULL;
}



/* create IPv4 socket */

int create_ipv4_socket(char *address, int port, int *file_descriptor, struct sockaddr_in *destination_address)
{
    int error, option_value;
    
    struct sockaddr_in local_address;
    

    /* get datagram socket */

    *file_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    
    if(*file_descriptor == SOCKET_ERROR)
    {
        printf("Failed getting socket in function create_socket %s \n", strerror(errno));
        return errno;
    }
    
    local_address.sin_family = AF_INET;
    local_address.sin_addr.s_addr = htonl(INADDR_ANY);
    local_address.sin_port = htons(0);
    
    
    /* prepare destination sockaddr_in */
    
    destination_address->sin_family = AF_INET;
    inet_pton(AF_INET, address, &(destination_address->sin_addr));
    destination_address->sin_port = htons(port);
    
    
    /* configuring the socket */
    
    option_value = 1;
    error = setsockopt(*file_descriptor, SOL_SOCKET, SO_REUSEADDR, (char *)&option_value, sizeof(option_value));
    if(error == SOCKET_ERROR)
    {
        printf("setsockopt for reuse address failed \n");
        return errno;
    }
    
    error = setsockopt(*file_descriptor, IPPROTO_IP, IP_RECVERR, (char *)option_value, sizeof(option_value));
    if(error == SOCKET_ERROR)
    {
        printf("setsockopt for IP_RECVERR failed \n");
        return errno;
    }
    
    return 1;
}



/* create IPv6 socket */

int create_ipv6_socket(char *address, int port, int *file_descriptor, struct sockaddr_in6 *destination_address)
{
    int error, option_value;
    
    struct sockaddr_in6 local_address;
    
    /* get datagram socket */
    
    *file_descriptor = socket(AF_INET6, SOCK_DGRAM, 0);
    
    if(*file_descriptor == SOCKET_ERROR)
    {
        printf("Failed getting socket in function create_socket %s \n", strerror(errno));
        return errno;
    }
    
    local_address.sin6_family = AF_INET6;
    local_address.sin6_addr = in6addr_any;
    local_address.sin6_port = htons(0);
    
    
    /* prepare destination sockaddr_in */
    
    destination_address->sin6_family = AF_INET6;
    inet_pton(AF_INET6, address, &(destination_address->sin6_addr));
    destination_address->sin6_port = htons(port);
    
    
    /* Probably this is not the proper way. */
    
    destination_address->sin6_scope_id = if_nametoindex("wlan0");
    
    
    /* configuring the socket */
    
    option_value = 1;
    error = setsockopt(*file_descriptor, SOL_SOCKET, SO_REUSEADDR, (char *)&option_value, sizeof(option_value));
    if(error == SOCKET_ERROR)
    {
        printf("setsockopt for reuse address failed \n");
        return errno;
    }
    
    error = setsockopt(*file_descriptor, IPPROTO_IP, IP_RECVERR, (char *)option_value, sizeof(option_value));
    if(error == SOCKET_ERROR)
    {
        printf("setsockopt for IP_RECVERR failed \n");
        return errno;
    }
    
    return 1;
}



/* Send and receive. */

uint32_t send_packet_with_message(char *message, int message_length)
{
    uint32_t identifier;
    
    if(!is_shared_instance_instantiated)
        return -1;
    
    if(is_shared_instance_ipv6)
        result_value = ipv6_sendmsg_udp(shared_descriptor, message, message_length, ipv6_destionation_address, &identifier);
    else
        result_value = ipv4_sendmsg_udp(shared_descriptor, message, message_length, ipv4_destionation_address, &identifier);
    
    if(result_value < 0)
        return result_value;
    else
        return identifier;
}


uint32_t receive_local_error_notify(void)
{
    if(!is_shared_instance_instantiated)
        return -1;
    if(is_shared_instance_ipv6)
    {
        int return_value;
        ErrMsg *error_message = allocinit_ErrMsg();
        return_value = ipv6_receive_error_message_no_wait(shared_descriptor, error_message);
        /* need to switch along errors type */
        if(return_value == 1)
        {
            for (error_message->c = CMSG_FIRSTHDR(error_message->msg); error_message->c; error_message->c = CMSG_NXTHDR(error_message->msg, error_message->c))
            {
                if(((error_message->c->cmsg_level == IPPROTO_IPV6) && (error_message->c->cmsg_type == IPV6_RECVERR)))
                {
                    struct sockaddr_in6 *from;
                    if((error_message->ee->ee_origin == SO_EE_ORIGIN_LOCAL_NOTIFY) && (error_message->ee->ee_errno == 0))
                    {
                        uint32_t identifier;
                        identifier = ntohl(error_message->ee->ee_info);
                        
                        fprintf(stderr,"ricevuta notifica IP id %d\n", identifier);
                        fflush(stderr);
                    }
                }
            }
        }
    }
    return 0;
}





