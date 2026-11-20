/*
Modulo que contiene la logica del servidor a nivel de sockets
Resolucion de dirrecion y puerto por medio de getaddrinfo
Socket TCP de cliente y del servidor no bloqueante
Uso de event-drive para concurrencia implementada con epoll
Comprobacion de eventos en el orden write, error y event ante
cualquier erro fatal en el inicio del servidor se termina la ejecucion
y a su ante cualquier error fatal en el proceso de procesar una solicitud se
cierra el socket del cliente
*/


#define _POSIX_C_SOURCE 200112L
#include "include/server.h"
#include "include/http.h"
#include <errno.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
static const int Q_len=10;
static char IP_SERVER[]="0.0.0.0";
static char SERVICE_OR_PORT[]="8025";
static int fd_epoll=-1;

static int passive_socket(char * ip_server, char * service_or_port, int q_len);
static void get_addr_clt(struct sockaddr_in *clt_addr,char* ip_port);
static void cfg_sock_clt(int fd_clt);
static void cfg_sock_srv(int fd_srv);
static void set_sock_opt(int fd,int level, int opt, int value, char* msg_e, int len_msg);

static int passive_socket(char * ip_server, char * service_or_port, int q_len){
    // Resolucion de la direccion IP y del servicio por medio de getaddrinfo()
    int fd_sock_server=-100;
    struct addrinfo *pointer=NULL, *resolution=NULL, hints={0};
    hints.ai_family=AF_INET;
    hints.ai_socktype=SOCK_STREAM;
    hints.ai_flags=AI_PASSIVE;
    int error_addr=getaddrinfo(ip_server,service_or_port,&hints,&resolution);
    if(error_addr !=0){
        printf("Error NS:%s \n",gai_strerror(error_addr));
        fflush(stdout);
        printf("Valores Defaults IP 0.0.0.0 Puerto 8080");
        fflush(stdout);
        int error_addr_default=getaddrinfo(IP_SERVER,SERVICE_OR_PORT,&hints,&resolution);
        if(error_addr_default !=0){
            printf("Error NS:%s \n",gai_strerror(error_addr_default));
            fflush(stdout);
            return fd_sock_server;
        }

    }
    for(pointer=resolution; pointer!=NULL;pointer=pointer->ai_next){
        fd_sock_server=socket(pointer->ai_family,pointer->ai_socktype | SOCK_NONBLOCK ,pointer->ai_protocol);
        if(fd_sock_server<0){
            perror("Creacion Socket");
            continue;
        }
        cfg_sock_srv(fd_sock_server);
        int error_bd= bind(fd_sock_server,pointer->ai_addr,pointer->ai_addrlen);
        if (error_bd<0){
            perror("Error Bind : "); 
            close(fd_sock_server);
            fd_sock_server=-100;
            continue;
        }
        int error_listen=listen(fd_sock_server,q_len);
        if(error_listen<0){
            perror("Error Listen : "); 
            close(fd_sock_server);
            fd_sock_server=-100;
            continue;
        }
        break;
    }
    freeaddrinfo(resolution);
    return fd_sock_server;
}

static void get_addr_clt(struct sockaddr_in *clt_addr,char* ip_port){
    if(inet_ntop(AF_INET,&clt_addr->sin_addr,ip_port,INET_ADDRSTRLEN)==NULL){
        perror("Error en clt ntop : "); 
    }
}
    /*Configuracion del socket no bloqueante con fnctl que modifica el descriptor de archivo del socket*/
static void cfg_sock_clt(int fd_clt){
    int flags= fcntl(fd_clt,F_GETFL);
    if(flags==-1){
        perror("No se pudo obtener las flags de socket clt");
        return;
    }
    flags=flags | O_NONBLOCK;
    if(fcntl(fd_clt,F_SETFL,flags)==-1){
        perror("No se pudo configurar el fd como no bloqueante");
    }
}
    /*Configuracion del passive socket para reusar el puerto cuando este se encuentre en time-wait por medio de setsockopt*/
static void cfg_sock_srv(int fd_srv){
    set_sock_opt(fd_srv,SOL_SOCKET,SO_REUSEADDR,1,"SO_REUSEADDR",sizeof("SO_REUSEADDR"));
}

static void set_sock_opt(int fd,int level, int opt, int value, char* msg_e, int len_msg){
    int error_opt= setsockopt(fd,level,opt,&value,sizeof(value));
    if(error_opt!=0){
        char msg_err[len_msg+7];
        snprintf(msg_err, sizeof(msg_err), "Error: ");
        strcat(msg_err,msg_e);
        perror(msg_err);
    }
}
/*Configuracion default de los eventos a seguir con epoll del socket passivo Lectura*/
static int event_epoll_srv(int fd_epoll, int fd_sock_passive){
    struct client *ptr = malloc(sizeof(struct client));
    if (ptr == NULL) {
        perror("malloc");
        return -1;
    }
    ptr->fd_clt=fd_sock_passive;
    struct epoll_event sock_passive={0};
    sock_passive.events=EPOLLIN;
    sock_passive.data.ptr=ptr;
    int error = epoll_ctl(fd_epoll,EPOLL_CTL_ADD,fd_sock_passive,&sock_passive);
    if(error<0){
        perror("No se pudo agregar el socket passivo a epoll:");
        return -1;
    }
    return 0;
}

/*Configuracion default de los eventos a seguir con epoll del socket cliente Lectura Errores*/

static int event_epoll_clt_init(int fd_epoll, int fd_clt){
    struct client *ptr = malloc(sizeof(struct client));
    if (ptr == NULL) {
        perror("malloc");
        return -1;
    }
    *ptr=(struct client){0};
    ptr->fd_clt=fd_clt;
    struct epoll_event clt_event={0};
    clt_event.events= EPOLLIN|EPOLLHUP| EPOLLERR | EPOLLRDHUP;
    clt_event.data.ptr=ptr;
    int error = epoll_ctl(fd_epoll,EPOLL_CTL_ADD,fd_clt,&clt_event);
    if(error<0){
        perror("No se pudo agregar el socket passivo a epoll:");
        return -1;
    }
    http_init(&ptr->http_msg);
    ptr->http_msg.send_file.fd_file=-1;
    return 1;
}

/*Modifacacion de los eventos de siguimiento para incluir la escritura esto cuando se llena el buffer de 
escritura del socket al enviar los datos de un archivo*/

static int event_epoll_clt_write(int fd_epoll, struct epoll_event event){
    event.events=EPOLLOUT|EPOLLHUP| EPOLLERR | EPOLLRDHUP;
    struct client *clt=event.data.ptr;
    int error=epoll_ctl(fd_epoll,EPOLL_CTL_MOD,clt->fd_clt,&event);
    if(error<0){
        perror("No se cambiar la configuracion del socket escritura para epoll:");
        return -1;
    }
}

/*Modifcacion de los eventos de siguimiento para regresar a la configuracion inicial del cliente 
evita estar recibiendo notificaciones de escritura cuando ya no se tiene nada que responder al cliente*/

static int event_epoll_clt_read(int fd_epoll, struct epoll_event event){
    event.events=EPOLLIN|EPOLLHUP| EPOLLERR | EPOLLRDHUP;
    struct client *clt=event.data.ptr;
    int error=epoll_ctl(fd_epoll,EPOLL_CTL_MOD,clt->fd_clt,&event);
    if(error<0){
        perror("No se cambiar la configuracion del socket lectura para epoll:");
        return -1;
    }
}

/* Eliminacion de la estructura principal del cliente (evitar basura en la memoria) y cierre del socket
al cerrar el socket se elimina la entrada de epoll
*/
static void remove_clt(struct epoll_event  event){
    struct client *clt=event.data.ptr;
    if(clt->http_msg.send_file.fd_file>0){
        close(clt->http_msg.send_file.fd_file);
    }
    int fd=clt->fd_clt;
    free(event.data.ptr);
    close(fd);
}

/* Aceptar la conexion de un cliente y la inicializacion de su estrcutura de datos y del seguimiento en epoll*/
static int accept_event(struct epoll_event event,int fd_epoll){
    struct sockaddr_in clt_addr={0};
    struct client *clt=event.data.ptr;
    socklen_t len_sock=16;
    int fd_clt=accept(clt->fd_clt,(struct sockaddr*)&clt_addr,&len_sock);
    if(fd_clt<0){
        fprintf(stderr,"Se fallo al aceptar el cliente: accept() en fd_escucha=%d devolvio=%d errno=%d (%s)\n",clt->fd_clt,fd_clt,errno,strerror(errno));
        return -1;
    }
    cfg_sock_clt(fd_clt);
    if(event_epoll_clt_init(fd_epoll,fd_clt)<0){
        return -1;
    }
    return 0;
}

/*Procedimiento de la elaboracion de la respuesta despues de la lectura de datos y cierre ordenado en caso
de recibir un close o de un fin por parte del cliente*/

static int response_request_socket(struct epoll_event event){
    struct client *clt=event.data.ptr;
    while (1){
        int code_error=response(clt,clt->fd_clt,0);
        if(code_error==200){
            int error=send_body_http(clt->fd_clt,&clt->http_msg.send_file);
                if(error<0){
                    remove_clt(event);
                    return -1;
                }else if(error==0){
                    event_epoll_clt_write(fd_epoll,event);
                }
        }else if(code_error ==-1){
            remove_clt(event);
            return -1;
        }else if(code_error==0){
            break;
        }
    }
    if(clt->http_msg.close_after_response){
        remove_clt(event);
        return -1;
    }
}


/*Continuar con el envio de los datos despues que se haya llenado el buffer y responder a las solicitudes
pendientes*/
static int write_event(struct epoll_event event){
    struct client *clt=event.data.ptr;
    if(clt->http_msg.send_file.offset < clt->http_msg.send_file.len_file){
        int error=send_body_http(clt->fd_clt,&clt->http_msg.send_file);
        if(error<0){
            remove_clt(event);
            return -1;
        }
    }
    int error=response_request_socket(event);
    if(error<0){
        return -1;
    }
    event_epoll_clt_read(fd_epoll,event);
    return error;
}

/*Lectura de los datos del socket y llamada para escribir en la estructura de datos del cliente*/
static int read_event(struct epoll_event event){
    char buffer[4096];
    struct client *clt=event.data.ptr;
    while (1){
       int n= read(clt->fd_clt,buffer,sizeof(buffer));
       if(n<0){
        if(errno==EAGAIN){
            break;
        }else if(errno==EINTR){
            continue;
        }
        remove_clt(event);
        return -1;
       }else if(n==0){
        clt->http_msg.close_after_response=1;
        break;
       }
       if(http_write(&clt->http_msg,buffer,n)<0){
        response(clt,clt->fd_clt,400);
        remove_clt(event);
        return -1;
       }
    }
    int error=response_request_socket(event);
    return error;
}

static void check_event(struct epoll_event event, int fd_passive){
    struct client *clt=event.data.ptr;
    if(event.events & EPOLLOUT){
        write_event(event);
    }else if (event.events & EPOLLHUP | event.events & EPOLLRDHUP){
        clt->http_msg.close_after_response=1;
        write_event(event);
    }else if (event.events & EPOLLERR){
        remove_clt(event);
    }else if(event.events & EPOLLIN){
        if(clt->fd_clt==fd_passive){
            accept_event(event,fd_epoll);
            return;
        }
        if(clt->http_msg.send_file.fd_file==-1){
            read_event(event);
        }
    }    
}

void server(void){
    init_files();
    fd_epoll=epoll_create(30);
    if(fd_epoll<0){
        perror("No se puede crear el fd de concurrencia");
        return;
    }
    int fd_passive=passive_socket(IP_SERVER,SERVICE_OR_PORT,30);
    if(fd_passive<0){
        printf("No se puede crear el socket pasivo");
        return;
    }
    if(event_epoll_srv(fd_epoll,fd_passive)){
        return;
    }
    struct epoll_event maxEvents[30];
    while(1){
        int n_events=epoll_wait(fd_epoll,maxEvents,30,100);
        if(n_events<0){
            perror("Fallo al obtener los eventos de epoll");
            return;
        }
        for(int i=0; i<n_events;i++){
            check_event(maxEvents[i],fd_passive);
        }
    }
}















