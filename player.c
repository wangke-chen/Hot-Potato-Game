#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

int player_socket_fd;
int left_player_socket_fd;
int right_player_socket_fd;

int get_usable_port(){
  int status;
  int socket_fd;
  struct addrinfo host_info;
  struct addrinfo * host_info_list;
  char port_num[10];
  int port;
  memset(&host_info, 0, sizeof(host_info));
  host_info.ai_family = AF_UNSPEC;
  host_info.ai_socktype = SOCK_STREAM;
  host_info.ai_flags = AI_PASSIVE;
  for(port = 1024; port < 65536; port++){
    sprintf(port_num, "%d", port);
    status = getaddrinfo(NULL, port_num, &host_info, &host_info_list);
    if(status != 0){
      perror("Cannont get address info for Ringmaster.\n");
      exit(EXIT_FAILURE);
    }

    socket_fd = socket(host_info_list->ai_family,
		       host_info_list->ai_socktype,
		       host_info_list->ai_protocol);
    if(socket_fd == -1){
      perror("Cannot create socker.\n");
      exit(EXIT_FAILURE);
    }

    status = bind(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
    if(status == -1){
      continue;
    }
    else{
      break;
    }
  }
  
  if(port == 65536){
    port = -1;
  }
  freeaddrinfo(host_info_list);
  player_socket_fd = socket_fd;
  return port;
}

int send_all(int fd, char * buff_ptr, int size) {
  int actually_sent_size;
	while (1) {
		actually_sent_size = send(fd, buff_ptr, size, 0);
        if(actually_sent_size == -1){
            return -1;
        }
		size  -= actually_sent_size;
		buff_ptr += actually_sent_size;
		if (size == 0) {break;}
	}
    return 1;
}

int main(int argc, char * argv[]){
  if(argc != 3){
    perror("Usage: player <machine_name> <port_num>.\n");
    exit(EXIT_FAILURE);
  }

  char * master_name = argv[1];
  char * port_num = argv[2];
  if(atoi(port_num) < 1024 || atoi(port_num) > 65536){
    perror("Invalid port number.\n");
    exit(EXIT_FAILURE);
  }

  int status;
  int socket_fd_to_master;
  struct addrinfo to_master_info;
  struct addrinfo * to_master_info_list;
  
  memset(&to_master_info, 0, sizeof(to_master_info));
  to_master_info.ai_family = AF_UNSPEC;
  to_master_info.ai_socktype = SOCK_STREAM;
  
  status = getaddrinfo(master_name, port_num, &to_master_info, &to_master_info_list);
  if(status != 0){
    perror("Cannont get address info.\n");
    exit(EXIT_FAILURE);
  }

  socket_fd_to_master = socket(to_master_info_list->ai_family,
			    to_master_info_list->ai_socktype,
			    to_master_info_list->ai_protocol);
  if(socket_fd_to_master == -1){
    perror("Cannot create socker connected to Ringmaster.\n");
    exit(EXIT_FAILURE);
  }

  status = connect(socket_fd_to_master, to_master_info_list->ai_addr, to_master_info_list->ai_addrlen);
  if(status == -1){
    perror("Cannot connect to Ringmaster.\n");
    exit(EXIT_FAILURE);
  }

  int num_players;
  status = recv(socket_fd_to_master, &num_players, sizeof(int), MSG_WAITALL);
  if(status == -1){
    perror("Cannot recv the number of players from Ringmaster.\n");
    exit(EXIT_FAILURE);
  }

  int player_id;
  status = recv(socket_fd_to_master, &player_id, sizeof(int), MSG_WAITALL);
  if(status == -1){
    perror("Cannot recv the player id from Ringmaster.\n");
    exit(EXIT_FAILURE);
  }

  int num_hops;
  status = recv(socket_fd_to_master, &num_hops, sizeof(int), MSG_WAITALL);
  if(status == -1){
    perror("Cannot recv the number of hops from Ringmaster.\n");
    exit(EXIT_FAILURE);
  }

  int port_int = get_usable_port();
  if(port_int == -1){
    perror("No usable port.\n");
    exit(EXIT_FAILURE);
  }

  char port[10];
  sprintf(port, "%d", port_int);
  status = send(socket_fd_to_master, &port, sizeof(port), 0);
  if(status == -1){
    perror("Cannot send the neighbor-listening port to Ringmaster.\n");
    exit(EXIT_FAILURE);
  }

  char hostname[500];
  status = gethostname(hostname, 500);
  if(status == -1){
    perror("Cannot get the hostname.\n");
    exit(EXIT_FAILURE);
  }

  status = send(socket_fd_to_master, hostname, sizeof(hostname), 0);
  if(status == -1){
    perror("Cannot send the hostname to Ringmaster.\n");
    exit(EXIT_FAILURE);
  }   
  
  freeaddrinfo(to_master_info_list);
  
  int connect_flag;
  for(int neighbor_connect = 0; neighbor_connect < 2; neighbor_connect++){
    status = recv(socket_fd_to_master, &connect_flag, sizeof(int), MSG_WAITALL);
    if(status == -1){
      perror("Cannot recv the signal of either listening right or connecting left from Ringmaster.\n");
      exit(EXIT_FAILURE);
    }

    if(connect_flag == 0){
      status = listen(player_socket_fd, 2);
      if(status == -1){
        perror("Cannot listen on socket.\n");
        exit(EXIT_FAILURE);
      }

      int listen_ready = 1;
      send(socket_fd_to_master, &listen_ready, sizeof(listen_ready), 0);

      struct sockaddr_storage right_socket_addr;
      socklen_t right_socket_addr_len = sizeof(right_socket_addr);
      right_player_socket_fd = accept(player_socket_fd, (struct sockaddr*)&right_socket_addr, &right_socket_addr_len);
      if(right_player_socket_fd == -1){
	perror("Cannot accept connect from the right player.\n");
	exit(EXIT_FAILURE);
      }
    }
    else{
      struct addrinfo left_info;
      struct addrinfo * left_info_list;
      char left_hostname[500];
      char left_port[10];
      status = recv(socket_fd_to_master, left_hostname, 500, MSG_WAITALL);    
      if(status == -1){
        perror("Cannot recv left-player's hostname from Ringmaster.\n");
        exit(EXIT_FAILURE);
      }

      status = recv(socket_fd_to_master, left_port, 10, MSG_WAITALL);
      if(status == -1){
        perror("Cannot recv left-player's port number from Ringmaster.\n");
        exit(EXIT_FAILURE);
      }
      
      memset(&left_info, 0, sizeof(left_info));
      left_info.ai_family = AF_UNSPEC;
      left_info.ai_socktype = SOCK_STREAM;
  
      status = getaddrinfo(left_hostname, left_port, &left_info, &left_info_list);
      if(status == -1){
	perror("Cannont get address info.\n");
	exit(EXIT_FAILURE);
      }

      left_player_socket_fd = socket(left_info_list->ai_family,
				     left_info_list->ai_socktype,
				     left_info_list->ai_protocol);
      if(left_player_socket_fd == -1){
	perror("Cannot create socker connected to left player.\n");
	exit(EXIT_FAILURE);
      }

      status = connect(left_player_socket_fd, left_info_list->ai_addr, left_info_list->ai_addrlen);
      if(status == -1){
	perror("Cannot connect to left player.\n");
	exit(EXIT_FAILURE);
      }
      int connect_ready = 1;
      send(socket_fd_to_master, &connect_ready, sizeof(int), 0);
      freeaddrinfo(left_info_list);
    }

  }
  printf("Connected as player %d out of %d total players\n", player_id, num_players);

  srand((unsigned int)time(NULL));
  char trace[num_hops*6 + 10];
  int n;
  int hops;
  fd_set readfds;
  while(1){
    strcpy(trace, "");
    FD_ZERO(&readfds);
    FD_SET(socket_fd_to_master, &readfds);
    FD_SET(left_player_socket_fd, &readfds);
    FD_SET(right_player_socket_fd, &readfds);
    n = socket_fd_to_master;
    if(n < left_player_socket_fd){
      n = left_player_socket_fd;
    }
    if(n < right_player_socket_fd){
      n = right_player_socket_fd;
    }

    status = select(n+1, &readfds, NULL, NULL, NULL);
    if(status == -1){
      perror("Cannot select where the potato is from.\n");
      exit(EXIT_FAILURE);
    }

    if(FD_ISSET(socket_fd_to_master, &readfds)){
      status = recv(socket_fd_to_master, trace, sizeof(trace), MSG_WAITALL);
      if(status == -1){
	perror("Cannot recv the hot potato from Ringmaster.\n");
	exit(EXIT_FAILURE);
      }

      status = recv(socket_fd_to_master, &hops, sizeof(int), MSG_WAITALL);
      if(status == -1){
	perror("Cannot recv the initial number of hops from Ringmaster.\n");
	exit(EXIT_FAILURE);
      }
    }
    else if(FD_ISSET(left_player_socket_fd, &readfds)){
      status = recv(left_player_socket_fd, trace, sizeof(trace), MSG_WAITALL);
      if(status == -1){
	perror("Cannot recv the hot potato from the left player.\n");
	exit(EXIT_FAILURE);
      }

      status = recv(left_player_socket_fd, &hops, sizeof(int), MSG_WAITALL);
      if(status == -1){
	perror("Cannot recv the remaining number of hops from the left player.\n");
	exit(EXIT_FAILURE);
      }
    }
    else if(FD_ISSET(right_player_socket_fd, &readfds)){
      status = recv(right_player_socket_fd, trace, sizeof(trace), MSG_WAITALL);
      if(status == -1){
	perror("Cannot recv the hot potato from the right player.\n");
	exit(EXIT_FAILURE);
      }

      status = recv(right_player_socket_fd, &hops, sizeof(int), MSG_WAITALL);
      if(status == -1){
	perror("Cannot recv the remaining number of hops from the right player.\n");
	exit(EXIT_FAILURE);
      }
    }

    if(hops > 1){
      if(strcmp(trace, "")==0){
	sprintf(trace,"%d", player_id);
      }
      else{
	sprintf(trace,"%s,%d", trace, player_id);
      }
      hops--;
      int next_player = rand() % 2;      
      
      if(next_player == 0){
	status = send_all(left_player_socket_fd, trace, sizeof(trace));
	if(status == -1){
	  perror("Cannot send hot potato to the left player.\n");
	  exit(EXIT_FAILURE);
	}
	status = send(left_player_socket_fd, &hops, sizeof(int), 0);
	if(status == -1){
	  perror("Cannot send remaining number of hops to the left player.\n");
	  exit(EXIT_FAILURE);
	}
      }
      else{
	status = send_all(right_player_socket_fd, trace, sizeof(trace));
	if(status == -1){
	  perror("Cannot send hot potato to the right player.\n");
	  exit(EXIT_FAILURE);
	}
	status = send(right_player_socket_fd, &hops, sizeof(int), 0);
	if(status == -1){
	  perror("Cannot send remaining number of hops to the right player.\n");
	  exit(EXIT_FAILURE);
	}
      }	
      if(next_player == 0){
	next_player = -1;
      }
      int next_player_id = (player_id + next_player + num_players) % num_players;
      printf("Sending potato to %d\n", next_player_id);
    }
    else if(hops == 1){
      if(strcmp(trace, "")==0){
	sprintf(trace,"%d", player_id);
      }
      else{
	sprintf(trace,"%s,%d", trace, player_id);
      }

      status = send_all(socket_fd_to_master, trace, sizeof(trace));
      if(status == -1){
	perror("Cannot send potato back to Ringmaster.\n");
	exit(EXIT_FAILURE);
      }
      printf("I'm it\n");
    }
    
    if(strcmp(trace, "close_all") == 0){
      close(socket_fd_to_master);
      close(player_socket_fd);
      close(left_player_socket_fd);
      close(right_player_socket_fd);
      //printf("socket closed.\n");
      break;
    }
  }
  return 1;
}   
