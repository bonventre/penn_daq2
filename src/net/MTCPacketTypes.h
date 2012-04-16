#ifndef _MTC_PACKET_TYPES_H
#define _MTC_PACKET_TYPES_H

#define kSBC_MaxPayloadSizeBytes 1024*400
#define kSBC_MaxMessageSizeBytes    256

/*! \name sbc_send_packet_type
 *  Types of packets that will be sent to the sbc from the xl3
 */
//@{
#define MTC_XILINX_ID           (0x1)
#define MTC_READ_ID             (0x2)
#define MTC_WRITE_ID            (0x3)
//@}

typedef struct {
    uint32_t destination;    /*should be kSBC_Command*/
    uint32_t cmdID;
    uint32_t numberBytesinPayload;
} SBCHeader;

typedef struct {
    uint32_t numBytes;                //filled in automatically
    SBCHeader header;
    char message[kSBC_MaxMessageSizeBytes];
    char payload[kSBC_MaxPayloadSizeBytes];
} SBCPacket;

typedef struct {
    int32_t baseAddress;
    int32_t addressModifier;
    int32_t programRegOffset;
    uint32_t errorCode;
    int32_t fileSize;
} SNOMtcXilinxLoadStruct;

typedef struct {
    uint32_t address;        /*first address*/
    uint32_t addressModifier;
    uint32_t addressSpace;
    uint32_t unitSize;        /*1,2,or 4*/
    uint32_t errorCode;    /*filled on return*/
    uint32_t numItems;        /*number of items to read*/
} SBCVmeReadBlockStruct;

typedef struct {
    uint32_t address;        /*first address*/
    uint32_t addressModifier;
    uint32_t addressSpace;
    uint32_t unitSize;        /*1,2,or 4*/
    uint32_t errorCode;    /*filled on return*/
    uint32_t numItems;        /*number Items of data to follow*/
    /*followed by the requested data, number of items from above*/
} SBCVmeWriteBlockStruct;



#endif
