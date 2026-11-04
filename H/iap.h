#ifndef __IAP_H
#define __IAP_H

#include <stdint.h>

void IAP_Unlock(void);
void IAP_Lock(void);
void IAP_WriteCmd(uint8_t NewState);
void IAP_SERASE(uint32_t Add, uint32_t AreaBase);
void IAP_EraseAll(void);
void IAP_ProgramWord(uint32_t Add, uint32_t Data);
void IAP_TestMode(void); 
void jump_to_flash(void);
void jump_to_BOOT(void);
#endif

