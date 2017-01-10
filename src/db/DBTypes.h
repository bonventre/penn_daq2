#ifndef _DB_TYPES_H
#define _DB_TYPES_H

#include <stdint.h>

#pragma pack(1)

typedef struct
{
  uint16_t mbID;
  uint16_t dbID[4];
} FECConfiguration;

typedef struct {
  /* index definitions [0=ch0-3; 1=ch4-7; 2=ch8-11, etc] */
  uint8_t     rmp[8];    //!< back edge timing ramp    
  uint8_t     rmpup[8];  //!< front edge timing ramp    
  uint8_t     vsi[8];    //!< short integrate voltage     
  uint8_t     vli[8];    //!< long integrate voltage
} TDisc; //!< discriminator timing dacs

typedef struct {
  /* the folowing are motherboard wide constants */
  uint8_t     vMax;           //!< upper TAC reference voltage
  uint8_t     tacRef;         //!< lower TAC reference voltage
  uint8_t     isetm[2];       //!< primary   timing current [0= tac0; 1= tac1]
  uint8_t     iseta[2];       //!< secondary timing current [0= tac0; 1= tac1]
  /* there is one uint8_t of TAC bits for each channel */
  uint8_t     tacShift[32];  //!< TAC shift register load bits 
} TCmos; //!< cmos timing

/* CMOS shift register 100nsec trigger setup */
typedef struct {
  uint8_t      mask[32]; //!< 
  uint8_t      tDelay[32]; //!< tr100 width
} Tr100; //!< nhit 100 trigger

/* CMOS shift register 20nsec trigger setup */
typedef struct {
  uint8_t      mask[32]; //!<
  uint8_t      tWidth[32]; //!< tr20 width
  uint8_t      tDelay[32]; //!< tr20 delay
} Tr20; //!< nhit 20 trigger


typedef struct {
  uint16_t mbID; //!< 
  uint16_t dbID[4]; //!<
  uint8_t vBal[2][32]; //!< 
  uint8_t vThr[32]; //!<
  TDisc tDisc; //!< 
  TCmos tCmos; //!<
  uint8_t vInt; //!< integrator output voltage 
  uint8_t hvRef; //!<  MB control voltage 
  Tr100 tr100; //!< 
  Tr20 tr20; //!< 
  uint16_t sCmos[32]; //!<
  uint32_t  disableMask; //!<
} MB; //!< all database values for one fec

typedef struct
{
	MB mb[16]; //!< all 16 fec database values
	uint32_t ctcDelay; //!< ctc based trigger delay
} Crate; //!< all database values for the crate

typedef struct {
    uint16_t lockoutWidth;
    uint16_t pedestalWidth;
    uint16_t nhit100LoPrescale;
    uint32_t pulserPeriod;
    uint32_t low10MhzClock;
    uint32_t high10MhzClock;
    float fineSlope;
    float minDelayOffset;
    uint16_t coarseDelay;
    uint16_t fineDelay;
    uint32_t gtMask;
    uint32_t gtCrateMask;
    uint32_t pedCrateMask;
    uint32_t controlMask;
}MTCD;

typedef struct{
    char id[20];
    uint16_t threshold;
    uint16_t mvPerAdc;
    uint16_t mvPerHit;
    uint16_t dcOffset;
}Trigger;

typedef struct {
    Trigger triggers[14];
    uint16_t crateStatus[6];
    uint16_t retriggers[6];
}MTCA;

typedef struct {
    MTCD mtcd;
    MTCA mtca;
    uint32_t tubBits;
}MTC;

#pragma pack()

#endif
