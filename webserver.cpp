#include <iostream>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <fstream>
#include <dirent.h>
#include <stdlib.h>
#include <signal.h>

//发送消息头,传入参数：1，套接字文件描述符 2，状态码 3，状态信息 4，文件类型,5,文件长度
void send_header(int cfd,int code,char *info,char* filetype,int length){
    //状态行和消息头
    char buf[1500] = "";
    int len = 0;
    if (length <= 0)
    {
        len = sprintf(buf,"HTTP/1.1 %d %s\r\nContent-Type:%s\r\n",code,info,filetype);
    }
    else
    {
        len = sprintf(buf,"HTTP/1.1 %d %s\r\nContent-Type:%s\r\nContent-Length:%d\r\n",code,info,filetype,length);
    }
    
    std::cout << "head" << buf;
    send(cfd,buf,len,0);
    //消息头
    // sprintf(buf,"",);
    // std::cout << buf;
    // send(cfd,buf,len,0);
    // if (length > 0)
    // {
    //     sprintf(buf,,length);
    //     send(cfd,buf,len,0);
    // }

    //空行(不能忘)
    send(cfd,"\r\n",2,0);



}
//发送文件,参数：套接字，文件路径，是否关闭标志，监听的事件句柄，epoll树根
void send_file(int cfd,char* filepath,int close_flag,struct epoll_event* ev,int epfd){
    std::cout << "begin send file" << std::endl;
    int fd = open(filepath,O_RDONLY);
    if (fd < 0)
    {
        std::cout << "open file: "<<filepath<<"fail" << std::endl;
        return;
    }
    char buf[5000];
    while (1)
    {
        int count = read(fd,buf,sizeof(buf));
        if (count <= 0)
        {
            break;
        }
        write(cfd,buf,count);
    }
    std::cout  << std::endl;
    if (close_flag == 1)
    {
        epoll_ctl(epfd,EPOLL_CTL_DEL,cfd,ev);
        close(cfd);
    }
    std::cout << "send finished" << std::endl;

}
void request_http(char* msg1,struct epoll_event* ev,int epfd){
    signal(SIGPIPE,SIG_IGN);
    int cfd = ev->data.fd;
    std::string method = "";
    std::string content = "";
    std::string msg = msg1;
    method = msg.substr(0,msg.find(' '));
    content = msg.substr(msg.find(' ')+1,msg.substr(msg.find(' ')+1).find(' '));
    std::cout << "method:"<<" "<<method <<"   "<<"content:"<<" "<<content << std::endl;
 
    char file_path[50] =  ".";
    strcat(file_path,content.c_str());
   std::cout << "content"<<content.c_str() << std::endl;
    std::cout <<"***********file:" <<file_path << std::endl;
    //读取文件
    struct stat s;
    if (stat(file_path,&s) == -1) //文件不存在
    {
       
        std::cout << "the file is not existing" << std::endl;
        //发送错误信息头部
        send_header(cfd,404,(char *)"NOT FOUND",(char *)" text/html",0);
        //发送error.html
        send_file(cfd,(char *)"./error.html",1,ev,epfd);
    }
    else
    {
        //如果是普通文件
        if(S_ISREG(s.st_mode)){
            //发送信息头
            send_header(cfd,200,(char *)"OK",(char *)"text/html",s.st_size);
            //发送文件
            send_file(cfd,file_path,1,ev,epfd);
            std::cout << "regular file" << std::endl;
            
        }
        //如果是目录
        else if (S_ISDIR(s.st_mode))
        {
            //发送信息头部
            send_header(cfd,200,(char *)"OK",(char *)"text/html",0);

            //发送目录文件头
             send_file(cfd,(char*)"dir_head.html",0,ev,epfd);
            //发送目录列表
              //读取目录下列表：参数：1，目录路径 2，用于保存目录下的各个路径 3，筛选 4，排序
            struct dirent** name_list = NULL;
            int di = scandir(file_path,&name_list,NULL,alphasort);
            int i = 0;
            std::cout << "$$$dir$$$ : " << std::endl;
            for(int i = 0;i < di;i++)
            {
                //组包（包含目录）
                char listbuf[1024] = "";
                char lu[100] = "";
                // strcpy(lu,content.c_str());
                // std::cout << "###lu:"<<lu << std::endl;
                int n;
                if (name_list[i]->d_type == DT_REG)
                {
                    n = sprintf(listbuf,"<li><a href=%s>%s</a></li>",name_list[i]->d_name,name_list[i]->d_name);
                }
                else{
                    n = sprintf(listbuf,"<li><a href=%s/>%s</a></li>",name_list[i]->d_name,name_list[i]->d_name);
                }
                send(cfd,listbuf,n,0);
                free(name_list[i]); 
            }
            free(name_list);


             //发送文件尾部
             send_file(cfd,(char*)"dir_tail.html",1,ev,epfd);
            //发送列表

            
        }
    }
    


}




int main(int argc, const char** argv) {
    //切换工作目录
    char* curdir = getenv("PWD");
    std::cout << curdir << std::endl;
    char mydir[256] = "";
    strcpy(mydir,curdir);
    strcat(mydir,"/html");
    chdir(mydir);



    //创建监听套接字
    int lfd = socket(AF_INET,SOCK_STREAM,0);
    if (lfd < 0)
    {
        std::cout << "create lfd erro" << std::endl;
        return 1;
    }
    //绑定地址
    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(8081);
    serveraddr.sin_addr.s_addr = inet_addr("192.168.135.134");
    bind(lfd,(struct sockaddr*)& serveraddr,sizeof(serveraddr));
    //将套接字lfd设置为监听状态
    listen(lfd,128);
    //创建epoll树
    int epfd = epoll_create(1);
    //设置lfd监听读事件,ev用于lfd，用于存储需要读取cfd
    struct epoll_event ev,evs[1024];
    ev.events = EPOLLIN;
    ev.data.fd = lfd;
    //将lfd上树
    epoll_ctl(epfd,EPOLL_CTL_ADD,lfd,&ev);
    //循环监听树
    while (1)
    {
        int epoll_count = epoll_wait(epfd,evs,1024,-1);
        if (epoll_count < 0)
        {
            std::cout << "epoll wait erro " << std::endl;
            break;
        }
        std::cout << "epoll wait:"<<epoll_count << std::endl;
        for (int i = 0; i < epoll_count; i++)
        {
            int fd = evs[i].data.fd;
            if (fd == lfd && evs[i].events&EPOLLIN)
            {
                struct sockaddr_in cliaddr;
                socklen_t len = sizeof(cliaddr);
                int cfd = accept(lfd,(struct sockaddr*)&cliaddr,&len);
                if (cfd < 0)
                {
                    std::cout << " create cfd error" << std::endl;   
                }
                //打印客户端地址
                char ip[16];
                inet_ntop(AF_INET,&cliaddr.sin_addr.s_addr,ip,16);
                std::cout << "client ip:"<<ip<<"port"<<ntohs(cliaddr.sin_port) << std::endl;
                //将新的文件描述符上树
                struct epoll_event evs0;
                evs0.events = EPOLLIN;
                evs0.data.fd = cfd;
                epoll_ctl(epfd,EPOLL_CTL_ADD,cfd,&evs0);
                continue;
            }
            else if (evs[i].events&EPOLLIN)
            {
                char buf[1024];
                int count = read(evs[i].data.fd,buf,sizeof(buf));
                if(count <= 0){
                    std::cout << "client close" << std::endl;
                    close(evs[i].data.fd);
                    epoll_ctl(epfd,EPOLL_CTL_DEL,evs[i].data.fd,&evs[i]);
                    continue;
                }
                //返回指定数量的字符串
                request_http(buf,&evs[i],epfd);
            }
        }
        
    }







    return 0;
}
