#ifndef __MESSAGES_H__
#define __MESSAGES_H__


struct challenge_req_msg {
	char *			ecidlabel;    
	char *			ekcert;      
};

extern struct msg_parser challenge_req_msg_parser;

struct challenge_resp_msg {
	char *			challenge;
	char *			cloudpubKey;
	char *			dhsecret;
	char *			error;
	char *			nounce;
	char *			iv;
};

extern struct msg_parser challenge_resp_msg_parser;

struct activate_req_msg {
    char *			nonce;   
	char *			ecidlabel;    
	char *			eventlogmb2sig;
    char *			eventlogtossig;      
};

extern struct msg_parser activate_req_msg_parser;

struct activate_resp_msg {
	char *			signature;
	char *			signatureASN1;
    char *          error;
};

extern struct msg_parser activate_resp_msg_parser;


#endif
