#include "common.h" 
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

/* 
  This function is used to modify the behabior of running client.

  We quit all connections so we can reset the sockets.
*/

void set_behavior_flag(memcached_st *ptr, memcached_flags temp_flag, uint64_t data)
{
  if (data)
    ptr->flags|= temp_flag;
  else
    ptr->flags&= ~temp_flag;
}

memcached_return memcached_behavior_set(memcached_st *ptr, 
                                        memcached_behavior flag, 
                                        uint64_t data)
{
  switch (flag)
  {
  case MEMCACHED_BEHAVIOR_SND_TIMEOUT:
    ptr->snd_timeout= (int32_t)data;
    break;     
  case MEMCACHED_BEHAVIOR_RCV_TIMEOUT:
    ptr->rcv_timeout= (int32_t)data;
    break;     
  case MEMCACHED_BEHAVIOR_SERVER_FAILURE_LIMIT:
    ptr->server_failure_limit= (uint32_t)data;
    break;     
  case MEMCACHED_BEHAVIOR_BINARY_PROTOCOL:
    set_behavior_flag(ptr, MEM_BINARY_PROTOCOL, data);
    break;     
  case MEMCACHED_BEHAVIOR_SUPPORT_CAS:
    set_behavior_flag(ptr, MEM_SUPPORT_CAS, data);
    break;
  case MEMCACHED_BEHAVIOR_NO_BLOCK:
    set_behavior_flag(ptr, MEM_NO_BLOCK, data);
    memcached_quit(ptr);
  case MEMCACHED_BEHAVIOR_BUFFER_REQUESTS:
    set_behavior_flag(ptr, MEM_BUFFER_REQUESTS, data);
    memcached_quit(ptr);
    break;
  case MEMCACHED_BEHAVIOR_TCP_NODELAY:
    set_behavior_flag(ptr, MEM_TCP_NODELAY, data);
    memcached_quit(ptr);
    break;
  case MEMCACHED_BEHAVIOR_DISTRIBUTION:
    {
      ptr->distribution= (memcached_server_distribution)(data);
      run_distribution(ptr);
      break;
    }
  case MEMCACHED_BEHAVIOR_KETAMA:
    {
      if (data)
      {
        ptr->hash= MEMCACHED_HASH_MD5;
        ptr->distribution= MEMCACHED_DISTRIBUTION_CONSISTENT_KETAMA;
      }
      else
      {
        ptr->hash= 0;
        ptr->distribution= 0;
      }
      run_distribution(ptr);
      break;
    }
  case MEMCACHED_BEHAVIOR_KETAMA_WEIGHTED:
    {
      ptr->hash= MEMCACHED_HASH_MD5;
      ptr->distribution= MEMCACHED_DISTRIBUTION_CONSISTENT_KETAMA;
      set_behavior_flag(ptr, MEM_KETAMA_WEIGHTED, data);
      run_distribution(ptr);
      break;
    }
  case MEMCACHED_BEHAVIOR_HASH:
    ptr->hash= (memcached_hash)(data);
    break;
  case MEMCACHED_BEHAVIOR_KETAMA_HASH:
    ptr->hash_continuum= (memcached_hash)(data);
    run_distribution(ptr);
    break;
  case MEMCACHED_BEHAVIOR_CACHE_LOOKUPS:
    set_behavior_flag(ptr, MEM_USE_CACHE_LOOKUPS, data);
    memcached_quit(ptr);
    break;
  case MEMCACHED_BEHAVIOR_VERIFY_KEY:
    set_behavior_flag(ptr, MEM_VERIFY_KEY, data);
    break;
  case MEMCACHED_BEHAVIOR_SORT_HOSTS:
    {
      set_behavior_flag(ptr, MEM_USE_SORT_HOSTS, data);
      run_distribution(ptr);

      break;
    }
  case MEMCACHED_BEHAVIOR_POLL_TIMEOUT:
    ptr->poll_timeout= (int32_t)data;
    break;
  case MEMCACHED_BEHAVIOR_CONNECT_TIMEOUT:
    ptr->connect_timeout= (int32_t)data;
    break;
  case MEMCACHED_BEHAVIOR_RETRY_TIMEOUT:
    ptr->retry_timeout= (int32_t)data;
    break;
  case MEMCACHED_BEHAVIOR_SOCKET_SEND_SIZE:
    ptr->send_size= (int32_t)data;
    memcached_quit(ptr);
    break;
  case MEMCACHED_BEHAVIOR_SOCKET_RECV_SIZE:
    ptr->recv_size= (int32_t)data;
    memcached_quit(ptr);
    break;
  case MEMCACHED_BEHAVIOR_USER_DATA:
    return MEMCACHED_FAILURE;
  }

  return MEMCACHED_SUCCESS;
}

uint64_t memcached_behavior_get(memcached_st *ptr, 
                                memcached_behavior flag)
{
  memcached_flags temp_flag= 0;

  switch (flag)
  {
  case MEMCACHED_BEHAVIOR_BINARY_PROTOCOL:
    temp_flag= MEM_BINARY_PROTOCOL;
    break;     
  case MEMCACHED_BEHAVIOR_SUPPORT_CAS:
    temp_flag= MEM_SUPPORT_CAS;
    break;
  case MEMCACHED_BEHAVIOR_CACHE_LOOKUPS:
    temp_flag= MEM_USE_CACHE_LOOKUPS;
    break;
  case MEMCACHED_BEHAVIOR_NO_BLOCK:
    temp_flag= MEM_NO_BLOCK;
    break;
  case MEMCACHED_BEHAVIOR_BUFFER_REQUESTS:
    temp_flag= MEM_BUFFER_REQUESTS;
    break;
  case MEMCACHED_BEHAVIOR_TCP_NODELAY:
    temp_flag= MEM_TCP_NODELAY;
    break;
  case MEMCACHED_BEHAVIOR_VERIFY_KEY:
    temp_flag= MEM_VERIFY_KEY;
    break;
  case MEMCACHED_BEHAVIOR_KETAMA_WEIGHTED:
    temp_flag= MEM_KETAMA_WEIGHTED;
    break;
  case MEMCACHED_BEHAVIOR_DISTRIBUTION:
    return ptr->distribution;
  case MEMCACHED_BEHAVIOR_KETAMA:
    return (ptr->distribution == MEMCACHED_DISTRIBUTION_CONSISTENT_KETAMA) ? 1 : 0;
  case MEMCACHED_BEHAVIOR_HASH:
    return ptr->hash;
  case MEMCACHED_BEHAVIOR_KETAMA_HASH:
    return ptr->hash_continuum;
  case MEMCACHED_BEHAVIOR_SERVER_FAILURE_LIMIT:
    return ptr->server_failure_limit;
  case MEMCACHED_BEHAVIOR_SORT_HOSTS:
    temp_flag= MEM_USE_SORT_HOSTS;
    break;
  case MEMCACHED_BEHAVIOR_POLL_TIMEOUT:
    {
      return (unsigned long long)ptr->poll_timeout;
    }
  case MEMCACHED_BEHAVIOR_CONNECT_TIMEOUT:
    {
      return (unsigned long long)ptr->connect_timeout;
    }
  case MEMCACHED_BEHAVIOR_RETRY_TIMEOUT:
    {
      return (unsigned long long)ptr->retry_timeout;
    }
  case MEMCACHED_BEHAVIOR_SOCKET_SEND_SIZE:
    {
      int sock_size;
      socklen_t sock_length= sizeof(int);

      /* REFACTOR */
      /* We just try the first host, and if it is down we return zero */
      if ((memcached_connect(&ptr->hosts[0])) != MEMCACHED_SUCCESS)
        return 0;

      if (getsockopt(ptr->hosts[0].fd, SOL_SOCKET, 
                     SO_SNDBUF, &sock_size, &sock_length))
        return 0; /* Zero means error */

      return sock_size;
    }
  case MEMCACHED_BEHAVIOR_SOCKET_RECV_SIZE:
    {
      int sock_size;
      socklen_t sock_length= sizeof(int);

      /* REFACTOR */
      /* We just try the first host, and if it is down we return zero */
      if ((memcached_connect(&ptr->hosts[0])) != MEMCACHED_SUCCESS)
        return 0;

      if (getsockopt(ptr->hosts[0].fd, SOL_SOCKET, 
                     SO_RCVBUF, &sock_size, &sock_length))
        return 0; /* Zero means error */

      return sock_size;
    }
  case MEMCACHED_BEHAVIOR_USER_DATA:
    return MEMCACHED_FAILURE;
  }

  WATCHPOINT_ASSERT(temp_flag); /* Programming mistake if it gets this far */
  if (ptr->flags & temp_flag)
    return 1;
  else
    return 0;

  return MEMCACHED_SUCCESS;
}
