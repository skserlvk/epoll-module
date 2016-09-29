#ifndef TRANS_PROTOCOL_H
#define TRANS_PROTOCOL_H

#define BUFF_MAX 1024;

struct {
	int len;
	char data[BUFF_MAX];
}trans_msg;


/********************  common  ***************************/

#define CHECK_ERR_EXIT(ret, msg)	\
	do{	\
		if (ret == -1) {	\
			perror(msg);	\
			exit (ret);		\
		}					\
	}while(0)

#endif	//TRANS_PROTOCOL_H