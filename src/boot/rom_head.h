#ifndef _ROM_HEAD_H
#define _ROM_HEAD_H

#define ROM_USA 0
#define ROM_EUR 1
#define ROM_JAP 2

#define ROM_REGION ROM_USA

#define ROM_SIZE 0x00200000

#define SRAM_START (ROM_SIZE + 1)
#define SRAM_SIZE 0x8000

#endif // _ROM_HEAD_H
