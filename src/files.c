/*
Modulo para la obtencion de la cantidad de bytes de un archivo
envio del contenido de un archivo de forma optimizada sin usar el buffer del usuario
decoficacion del path en caso que se reciba en valores hexadecimales %FF y
abrir un archivo de forma segura
*/

#include "include/files.h"
#include <fcntl.h>         
#include <linux/openat2.h>
#include <stdio.h>
#include <sys/syscall.h>  
#include <sys/stat.h>
#include <sys/sendfile.h>    
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#define OK_PATH   0


static int fd_dir=-1;
static int is_mode_server=0;
static struct open_how rules={0};
static char *web_cd="./www/";
const char default_file[]="/index.html";

/* Configuracion de las reglas de resolucion de rutas para restrigir el directorio
de trabajo ademas de evitar resolver enlaces simbolicos*/
int init_files(void){
    fd_dir=open(web_cd,__O_PATH | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if(fd_dir<=0){
        perror("No se puede crear el fd del directorio: ");
    }
    rules.flags=O_RDONLY | O_CLOEXEC;
    rules.resolve=RESOLVE_NO_SYMLINKS | RESOLVE_BENEATH | RESOLVE_NO_MAGICLINKS;
    if(is_mode_server){
        close(0);
        close(1);
        close(2);
    }
}


static int is_hex(char value){
    return (value>='0' && value<='9') || (value>='a' && value<='f') || (value>='A' && value<='F');
}

/*Decoficacion del Path en caso que tenga valores hexadeciamles en caso de un error se 
vuelve el codigo de error -400 para luego enviar la respeusta de bad request*/
static int decode_path(char *path, size_t len_path, char *path_dst){
    int idx=0;
    for(size_t i=0; i<len_path;i++,idx++){
        if(path[i]=='%'){
            if(i+2<len_path && is_hex(path[i+1]) && is_hex(path[i+2])){
                char temp[3]={path[i+1],path[i+2],'\0'};
                int value=strtol(temp,NULL,16);
                char letter=(char) value;
                if(letter>=32 && letter<=126){
                    path_dst[idx]=letter;
                    i=i+2;
                    continue;
                }
            }
            return -400;
        }
        path_dst[idx]=path[i];
    }
    int n=snprintf(path,len_path,"%s",path_dst);
    if (n < 0 || (size_t)n >= len_path) {
    return -400;
}
    return OK_PATH;
}

/*Abrir un archivo con las reglas de resolucion, se devuelve un codigo de error en caso
de intentar abrir un archivo fuera del directorio de trabajo -403, en caso de no encontrar
el archivo -404 y en errores fatales -500*/
static int open_fd(char *path, size_t idx_path){
    char path_decode[idx_path+1];
    int error=decode_path(path+1,idx_path+1,path_decode);
    if(error!=0){
        return -400;
    }
    int fd=syscall(SYS_openat2,fd_dir,path_decode,&rules,sizeof(rules));
    if(fd<0){
        switch (errno){
        case ENOENT: // Ruta no existe
            return -404;
        case ENOTDIR: // Un archivo esta siendo usando como directorio
            return -404;
        case EACCES:  // El proceso no tiene permisos suficiente
            return -403;
        case EPERM: // El proceso no tiene permisos suficiente
            return -403;
        case EXDEV:     // Intento de escapar del directorio permitido
            return -403;
        case ELOOP: // usa un enlace simbolico
            return -403;    
        default:
            return -500;   
        }
    }
    return fd;
}


/*Envio del archivo sin pasar por el buffer del usuario tras el envio completo
se cierra el fd o en el caso de un envio parcial se retorna 0 que provoca que se
mofique el envento de seguimiento para incluir la disponibilidad de escritura*/
int send_body_http(int fd_socket, struct file_metadata *fil){
    while(fil->offset<fil->len_file){
        long int n=sendfile(fd_socket,fil->fd_file,&fil->offset,fil->len_file-fil->offset);
        if(n<0){
            if(n==EAGAIN && n==EWOULDBLOCK){
                return 0;
            }else{
                close(fil->fd_file);
                fil->fd_file=-1;
                return -1;
            }
        }
    }
    close(fil->fd_file);
    fil->fd_file=-1;
    return 1;
}   

/*Obtener la cantidad de bytes para enviarlos en el Content-Lenght de la respuesta*/
static int get_len_file(struct file_metadata *fil){
    struct stat metadata={0};
    int status= fstat(fil->fd_file,&metadata);
    if(status<0){
        perror("No se puede consultar la metadata del archivo");
        close(fil->fd_file);
        fil->fd_file=-1;
        return -400;
    }
    fil->len_file=metadata.st_size;
    fil->offset=0;
    fil->is_file_not_send=0;
    if(!S_ISREG(metadata.st_mode)){
        return -404;
    }
    return 200;
}

int open_fd_get_len(struct file_metadata *fil,char *path, size_t idx_path){
    int fd=open_fd(path,idx_path);
    if(fd<0){
        return fd;
    }
    fil->fd_file=fd;
    return get_len_file(fil);
}



