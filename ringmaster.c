#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <time.h>

struct player_t{
  int id;
  char listening_port[10];
  char hostname[500];
  int socket_fd_to_master;
  struct sockaddr socket_address;
};

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
  if(argc != 4){
    perror("Usage: ringmaster <port_num> <num_players> <num_hops>.\n");
    exit(EXIT_FAILURE);
  }
  
  char * port_num = argv[1];
  int num_players = atoi(argv[2]);
  int num_hops = atoi(argv[3]);
  if(atoi(port_num) < 1024 || atoi(port_num) > 65536){
    perror("Invalid port number.\n");
    exit(EXIT_FAILURE);
  }
  if(num_players <= 1){
    perror("The number of players must be greater than 1.\n");
    exit(EXIT_FAILURE);
  }
  if(num_hops < 0 || num_hops > 512){
    perror("The number of hops must be greater than or equal to 0 and less than or equal to 512.\n");
    exit(EXIT_FAILURE);
  }

  int status;
  int master_socket_fd;
  struct addrinfo master_info;
  struct addrinfo * master_info_list;
  struct player_t player[num_players];

  printf("Potato Ringmaster\n");
  printf("Players = %d\n", num_players);
  printf("Hops = %d\n", num_hops);

  memset(&master_info, 0, sizeof(master_info));
  master_info.ai_family = AF_UNSPEC;
  master_info.ai_socktype = SOCK_STREAM;
  master_info.ai_flags = AI_PASSIVE;

  status = getaddrinfo(NULL, port_num, &master_info, &master_info_list);
  if(status != 0){
    perror("Cannont get address info for Ringmaster.\n");
    exit(EXIT_FAILURE);
  }

  master_socket_fd = socket(master_info_list->ai_family,
			    master_info_list->ai_socktype,
			    master_info_list->ai_protocol);
  if(master_socket_fd == -1){
    perror("Cannot create socker.\n");
    exit(EXIT_FAILURE);
  }

  int yes = 1;
  status = setsockopt(master_socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
  if(status == -1){
    perror("setsockopt failed.\n");
    exit(EXIT_FAILURE);
  }
  
  status = bind(master_socket_fd, master_info_list->ai_addr, master_info_list->ai_addrlen);
  if(status == -1){
    perror("Cannot bind socket.\n");
    exit(EXIT_FAILURE);
  }

  status = listen(master_socket_fd, num_players);
  if(status == -1){
    perror("Cannot listen on socket.\n");
    exit(EXIT_FAILURE);
  }

  for(int player_id = 0; player_id < num_players; player_id++){
    socklen_t socket_addr_len = sizeof(player[player_id].socket_address);
    int play_connection_fd;
    play_connection_fd = accept(master_socket_fd, &player[player_id].socket_address, &socket_addr_len);
    if(play_connection_fd == -1){
      printf("Cannot accept the connection of player %d\n", player_id);
      perror("Cannont accept the connection.\n");
      exit(EXIT_FAILURE);
    }

    status = send(play_connection_fd, &num_players, sizeof(int), 0);
    if(status == -1){
      printf("Cannot send the number of players to player %d\n", player_id);
      perror("Cannot send data.\n");
      exit(EXIT_FAILURE);
    }

    status = send(play_connection_fd, &player_id, sizeof(int), 0);
    if(status == -1){
      printf("Cannont send the player id to player %d\n", player_id);
      perror("Cannont send data.\n");
      exit(EXIT_FAILURE);
    }

    status = send(play_connection_fd, &num_hops, sizeof(int), 0);
    if(status == -1){
      printf("Cannot send the number of hops to player %d\n", player_id);
      perror("Cannot send data.\n");
      exit(EXIT_FAILURE);
    }

    char port[10];
    status = recv(play_connection_fd, port, 10, MSG_WAITALL);
    if(status == -1){
      printf("Cannot recv the neighbor-listening port of player %d\n", player_id);
      perror("Cannot recv data.\n");
      exit(EXIT_FAILURE);
    }

    char hostname[500];
    status = recv(play_connection_fd, hostname, 500, MSG_WAITALL);
    if(status == -1){
      printf("Cannot recv the hostname of player %d\n", player_id);
      perror("Cannot recv data.\n");
      exit(EXIT_FAILURE);
    }
    
    player[player_id].id = player_id;
    strcpy(player[player_id].listening_port, port);
    strcpy(player[player_id].hostname, hostname);
    player[player_id].socket_fd_to_master = play_connection_fd;
  }

  int connect;
  for(int player_id = 0; player_id < num_players; player_id++){
    connect = 0;
    status = send(player[player_id].socket_fd_to_master, &connect, sizeof(int), 0);
    if(status == -1){
      printf("Cannot send the listen-start instruction to player %d\n", player_id);
      perror("Cannot send data.\n");
      exit(EXIT_FAILURE);
    }

    int listen_ready;
    status = recv(player[player_id].socket_fd_to_master, &listen_ready, sizeof(int), MSG_WAITALL);
    if(status == -1){
      printf("Cannot recv the listen-ready signal from player %d\n", player_id);
      perror("Cannot recv data.\n");
      exit(EXIT_FAILURE);
    }

    connect = 1;
    status = send(player[(player_id+1) % num_players].socket_fd_to_master, &connect, sizeof(int), 0);
    if(status == -1){
      printf("Cannot send the connect instruction to player %d\n", player_id);
      perror("Cannot send data.\n");
      exit(EXIT_FAILURE);
    }
    
    status = send(player[(player_id+1) % num_players].socket_fd_to_master, player[player_id].hostname, sizeof(player[player_id].hostname), 0);
    if(status == -1){
      printf("Cannot send the left-player's hostname to player %d\n", player_id);
      perror("Cannot send data.\n");
      exit(EXIT_FAILURE);
    }

    status = send(player[(player_id+1) % num_players].socket_fd_to_master, player[player_id].listening_port, sizeof(player[player_id].listening_port), 0);
    if(status == -1){
      printf("Cannot send the left-player's port number to player %d\n", player_id);
      perror("Cannot send data.\n");
      exit(EXIT_FAILURE);
    }

    int connect_ready;
    status = recv(player[(player_id+1) % num_players].socket_fd_to_master, &connect_ready, sizeof(int), MSG_WAITALL);
    if(status == -1){
      printf("Cannot recv the connect ready signal from player %d\n", player_id + 1);
      perror("Cannot recv data.\n");
      exit(EXIT_FAILURE);
    }

    printf("Player %d is ready to play\n", player_id);
  }

  char trace[(num_hops)*6+10];
  strcpy(trace, "");
 
  if(num_hops > 0){
    srand((unsigned int)time(NULL));
    int random = rand() % num_players;
    printf("Ready to start the game, sending potato to player %d\n", random);

    status =send_all(player[random].socket_fd_to_master, trace, sizeof(trace));
    if(status == -1){
      printf("Cannot send the hot potato to the first player(%d)\n", random);
      perror("Cannot send data.\n");
      exit(EXIT_FAILURE);
    }

    status = send(player[random].socket_fd_to_master, &num_hops, sizeof(int), 0);
    if(status == -1){
      printf("Cannot send the initial number of hops to the first player(%d)\n", random);
      perror("Cannot send data.\n");
      exit(EXIT_FAILURE);
    }

    fd_set readfds;
    int n = player[0].socket_fd_to_master;
    FD_ZERO(&readfds);
    FD_SET(player[0].socket_fd_to_master, &readfds);
    for(int player_id = 1; player_id < num_players; player_id++){
      FD_SET(player[player_id].socket_fd_to_master, &readfds);
      if(n < player[player_id].socket_fd_to_master){
	n = player[player_id].socket_fd_to_master;
      }
    }
    status = select(n+1, &readfds, NULL, NULL, NULL);
    if(status == -1){
      perror("Cannot select where the returned trace is from.\n");
      exit(EXIT_FAILURE);
    }

    for(int player_id = 0; player_id < num_players; player_id++){
      if(FD_ISSET(player[player_id].socket_fd_to_master, &readfds)){
	status = recv(player[player_id].socket_fd_to_master, trace, sizeof(trace), MSG_WAITALL);
	if(status == -1){
	  perror("Cannot recv the trace.\n");
	  exit(EXIT_FAILURE);
	}
	break;
      }
    }

    printf("Trace of potato:\n");
    printf("%s\n", trace);
    num_hops = 0;
  }

  if(num_hops == 0){
    strcpy(trace, "close_all");
    for(int player_id=0; player_id < num_players; player_id++){ 
      status = send_all(player[player_id].socket_fd_to_master, trace, sizeof(trace));
      if(status == -1){
	printf("Close process failed\n");
	perror("Cannot send data.\n");
	exit(EXIT_FAILURE);
      }
      
      status = send(player[player_id].socket_fd_to_master, &num_hops, sizeof(int), 0);
      if(status == -1){
	printf("Close process failed\n");
	perror("Cannot send data.\n");
	exit(EXIT_FAILURE);
      }
      
      close(player[player_id].socket_fd_to_master);
    }
  }
  freeaddrinfo(master_info_list);
  close(master_socket_fd);
}
		  

 
