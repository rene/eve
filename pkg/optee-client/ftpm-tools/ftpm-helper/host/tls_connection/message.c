#include "message.h"
#include "json_parse.h"

static struct msg_parser challenge_req_msg_obj_parser[] = {
	MSG_DYNAMIC_STRING("ECIDLable", struct challenge_req_msg, ecidlabel),
	MSG_DYNAMIC_STRING("EKCert", struct challenge_req_msg, ekcert)
};

MSG_TOPLEVEL(challenge_req_msg_parser, sizeof(struct challenge_req_msg), challenge_req_msg_obj_parser);

static struct msg_parser challenge_resp_msg_obj_parser[] = {
	MSG_DYNAMIC_STRING("Challenge", struct challenge_resp_msg, challenge),
	MSG_DYNAMIC_STRING("CloudPubKey", struct challenge_resp_msg, cloudpubKey),
	MSG_DYNAMIC_STRING("DHSecret", struct challenge_resp_msg, dhsecret),
	MSG_DYNAMIC_STRING("Error", struct challenge_resp_msg, error),
	MSG_DYNAMIC_STRING("Nounce", struct challenge_resp_msg, nounce),
	MSG_DYNAMIC_STRING("IV", struct challenge_resp_msg, iv)
};

MSG_TOPLEVEL(challenge_resp_msg_parser, sizeof(struct challenge_resp_msg), challenge_resp_msg_obj_parser);

static struct msg_parser activate_req_msg_obj_parser[] = {
	MSG_DYNAMIC_STRING("ECIDLable", struct activate_req_msg, ecidlabel),
	MSG_DYNAMIC_STRING("NONCE", struct activate_req_msg, nonce),
	MSG_DYNAMIC_STRING("EventLogMB2Sig", struct activate_req_msg, eventlogmb2sig),
	MSG_DYNAMIC_STRING("EventLogTOSSig", struct activate_req_msg, eventlogtossig)
};

MSG_TOPLEVEL(activate_req_msg_parser, sizeof(struct activate_req_msg), activate_req_msg_obj_parser);

static struct msg_parser activate_resp_msg_obj_parser[] = {
	MSG_DYNAMIC_STRING("Signature", struct activate_resp_msg, signature),
	MSG_DYNAMIC_STRING("SignatureASN1", struct activate_resp_msg, signatureASN1),
	MSG_DYNAMIC_STRING("Error", struct activate_resp_msg, error)
};

MSG_TOPLEVEL(activate_resp_msg_parser, sizeof(struct activate_resp_msg), activate_resp_msg_obj_parser);
