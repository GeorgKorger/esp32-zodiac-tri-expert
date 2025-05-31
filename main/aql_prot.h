#ifndef AQL_PROT_H
#define AQL_PROT_H

#include <stdint.h>

#define PACKET_HEADER 0x10, 0x02
#define PACKET_DEST_AQUALINK 0xB0
#define PACKET_FOOTER 0x10, 0x03

typedef struct aquaVal_s {
  float ph_setpoint;
  uint16_t acl_setpoint;
  float ph_current;
  uint16_t acl_current;
  uint8_t extra_bytes[6];
}aquaVal_t;

int parseIdResponse(uint8_t *response,int len);
int prepareOutputCommand(void);
int parseOutputResponse(uint8_t *response, int len);

#endif
