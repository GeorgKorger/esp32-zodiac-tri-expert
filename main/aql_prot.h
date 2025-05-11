#ifndef AQL_PROT_H
#define AQL_PROT_H

#define PACKET_HEADER 0x10, 0x02
#define PACKET_DEST_AQUALINK 0xB0
#define PACKET_FOOTER 0x10, 0x03

typedef struct aquaVal_s {
  float ph_setpoint;
  int acl_setpoint;
  float ph_current;
  int acl_current;
}aquaVal_t;

int parseIdResponse(uint8_t *response,int len);
int prepareOutputCommand(uint8_t output_percent, uint8_t* cmd);
int parseOutputResponse(uint8_t *response, int len, aquaVal_t* val);

#endif
