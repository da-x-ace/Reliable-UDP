#include "udp_utils.h"

void insert(node *pointer, int sockfd, struct sockaddr_in *ip_address, struct sockaddr_in *network_mask)
{
    /* Iterate through the list till we encounter the last node.*/
    while(pointer->next!=NULL)
    {
            pointer = pointer->next;
    }
    /* Allocate memory for the new node and put sockfd in it.*/
    pointer->next = (node *)malloc(sizeof(node));
    pointer = pointer->next;
    pointer->sockfd = sockfd;
    pointer->ip_address = ip_address;
    pointer->network_mask = network_mask;
    pointer->subnet_address.sin_addr.s_addr = ((ip_address->sin_addr.s_addr) & (network_mask->sin_addr.s_addr));
    pointer->client_info_head = (client_info *)malloc(sizeof(client_info)); 
    memset(pointer->client_info_head, 0, sizeof(client_info));

    pointer->next = NULL;
}

void purge_client_connection(node *head_pointer, pid_t pid)
{

    node* temp;
    int interface_counter;

    interface_counter = 0;

    temp = head_pointer;

    while(temp->next != NULL)
    {

        if (heads_of_all_connections[interface_counter] != NULL)
        {
            while(heads_of_all_connections[interface_counter]->next != NULL && 
                (heads_of_all_connections[interface_counter]->next)->child_pid != pid)
            {
                    heads_of_all_connections[interface_counter] = heads_of_all_connections[interface_counter]->next;
            }
            if(heads_of_all_connections[interface_counter]->next==NULL)
            {
                /* Not in this list */
                goto next;
            }
            /* Now pointer points to a node and the node next to it has to be removed */
            client_info *client_to_remove;
            client_to_remove = heads_of_all_connections[interface_counter]->next;
            /*temp points to the node which has to be removed*/
            heads_of_all_connections[interface_counter]->next = client_to_remove->next;
#if TURNOFF
            printf("Removing node of interface %d with child pid : %ld\n", interface_counter, (unsigned long)client_to_remove->child_pid);
#endif
            free(client_to_remove);
        }
        next:
            temp = temp->next;
            interface_counter++;
    }
}

void insert_client_connections(client_info *pointer, struct sockaddr_in client_ip_address, pid_t child_pid, unsigned short client_port)
{
    /* Iterate through the list till we encounter the last client_info.*/
    while(pointer->next!=NULL)
    {
            pointer = pointer->next;
    }
    /* Allocate memory for the new client_info */
    pointer->next = (client_info *)malloc(sizeof(client_info)); 
    pointer = pointer->next;
    pointer->client_ip_address = client_ip_address;
    pointer->child_pid = child_pid;
    pointer->client_port = client_port;

#if TURNOFF
    printf("Inserted child pid : %ld\n", (unsigned long)child_pid);
    printf("Inserted client port : %u\n", pointer->client_port);
    printf("Inserted client ip : %ld\n", (unsigned long)pointer->client_ip_address.sin_addr.s_addr);
#endif

    pointer->next = NULL;     
}

int find_client(int interface_counter, struct sockaddr_in client_ip_address, unsigned short client_port)
{
	//printf("in find client - %d\n", interface_counter);
        if (heads_of_all_connections[interface_counter] != NULL)
        {

            while(heads_of_all_connections[interface_counter]->next != NULL)
            {
                if ((heads_of_all_connections[interface_counter]->next)->client_ip_address.sin_addr.s_addr == (unsigned long)client_ip_address.sin_addr.s_addr 
                    && (heads_of_all_connections[interface_counter]->next)->client_port == client_port)
                {
			//printf("Found \n");
                    return 1;
                }
                heads_of_all_connections[interface_counter] = heads_of_all_connections[interface_counter]->next;
            }
	    //printf("not found\n");
            return 0;
        }    
	return 0;    
}


void print_client_connections(int interface_counter)
{
    char temp_str[BUFFER_SIZE]; 
    memset(temp_str, 0, sizeof(temp_str));

    if (heads_of_all_connections[interface_counter] != NULL)
    {
        while(heads_of_all_connections[interface_counter]->next != NULL)
        {
            printf("Child pid : %ld\n", (unsigned long)(heads_of_all_connections[interface_counter]->next)->child_pid);
            printf("Client port : %u\n", (heads_of_all_connections[interface_counter]->next)->client_port);
            if (inet_ntop(AF_INET, &heads_of_all_connections[interface_counter]->next->client_ip_address.sin_addr , temp_str, sizeof(temp_str)) == NULL)
                perror("Error! On inet_ntop() for ip-address.  ");
            else
                printf("Client ip-address : %s\n", temp_str);
            heads_of_all_connections[interface_counter] = heads_of_all_connections[interface_counter]->next;
            memset(temp_str, 0, sizeof(temp_str));
        }
    }   
}


void print(node *pointer)
{
    char temp_str[BUFFER_SIZE]; 

    memset(temp_str, 0, sizeof(temp_str));

    if(pointer==NULL)
    {
        return;
    }
    //printf("Socket descriptor : %d\n",pointer->sockfd);

    if (inet_ntop(AF_INET, &pointer->ip_address->sin_addr , temp_str, sizeof(temp_str)) == NULL)
        perror("Error! On inet_ntop() for ip-address.  ");
    else
        printf("IP Address : %s\n",temp_str);

    memset(temp_str, 0, sizeof(temp_str));

    if (inet_ntop(AF_INET, &pointer->network_mask->sin_addr , temp_str, sizeof(temp_str)) == NULL)
        perror("Error! On inet_ntop() for network-mask.  ");
    else
        printf("Network Mask : %s\n",temp_str);    

    memset(temp_str, 0, sizeof(temp_str));

    if (inet_ntop(AF_INET, &pointer->subnet_address.sin_addr , temp_str, sizeof(temp_str)) == NULL)
        perror("Error! On inet_ntop() for subnet address.\n");
    else
        printf("Subnet Address : %s\n",temp_str);

    printf("----------------------------------\n");
    print(pointer->next);
}

void cleanup(node *pointer)
{
    if(pointer==NULL)
    {
        return;
    }
    close(pointer->sockfd);
    cleanup(pointer->next);    
}

