#include <esp8266.h>
#include "spi_register.h"
#include "ff.h"
#include "diskio.h"

#define GO_IDLE_STATE			0
#define SEND_OP_COND			1
#define SEND_IF_COND			8
#define SEND_CSD				9
#define STOP_TRANSMISSION		12
#define SEND_STATUS				13
#define SET_BLOCK_LEN			16
#define READ_SINGLE_BLOCK		17
#define READ_MULTIPLE_BLOCKS	18
#define WRITE_SINGLE_BLOCK		24
#define WRITE_MULTIPLE_BLOCKS	25
#define ERASE_BLOCK_START_ADDR	32
#define ERASE_BLOCK_END_ADDR	33
#define ERASE_SELECTED_BLOCKS	38
#define SD_SEND_OP_COND			41
#define APP_CMD					55
#define READ_OCR				58
#define CRC_ON_OFF				59



#define CS_PIN 5
#define CLK_PIN 14
#define MOSI_PIN 13
#define MISO_PIN 12


#define SETCS(v) { \
	if (v) gpio_output_set((1<<CS_PIN), 0, (1<<CS_PIN), 0); \
	else gpio_output_set(0, (1<<CS_PIN), (1<<CS_PIN), 0); } while(0)

static int needByteAddr;

//#define HWSPI
#ifdef HWSPI
static void spiSend(int data) {
	while(READ_PERI_REG(SPI_CMD(1)) & SPI_USR);

	CLEAR_PERI_REG_MASK(SPI_USER(1), SPI_USR_COMMAND | SPI_USR_MISO|SPI_USR_ADDR|SPI_USR_DUMMY);
	SET_PERI_REG_MASK(SPI_USER(1), SPI_USR_MOSI);

	//SPI_FLASH_USER2 bit28-31 is cmd length,cmd bit length is value(0-15)+1,
	// bit15-0 is cmd value.
//	WRITE_PERI_REG(SPI_USER2(1), ((7 & SPI_USR_COMMAND_BITLEN) << SPI_USR_COMMAND_BITLEN_S) | (data));
	WRITE_PERI_REG(SPI_W0(1), data<<24);
	SET_PERI_REG_MASK(SPI_CMD(1), SPI_USR);
}

static int spiRecv(void) {
	while(READ_PERI_REG(SPI_CMD(1)) & SPI_USR);

	CLEAR_PERI_REG_MASK(SPI_USER(1), SPI_USR_COMMAND|SPI_USR_MOSI|SPI_USR_ADDR|SPI_USR_DUMMY);
	SET_PERI_REG_MASK(SPI_USER(1), SPI_USR_MISO);

	//SPI_FLASH_USER2 bit28-31 is cmd length,cmd bit length is value(0-15)+1,
	// bit15-0 is cmd value.
	WRITE_PERI_REG(SPI_USER2(1), ((7 & SPI_USR_COMMAND_BITLEN) << SPI_USR_COMMAND_BITLEN_S)|6);

	SET_PERI_REG_MASK(SPI_CMD(1), SPI_USR);
	while(READ_PERI_REG(SPI_CMD(1)) & SPI_USR);
	return (READ_PERI_REG(SPI_W0(1))>>24);
}
#else

#define CLKDELAY 1
static void spiSend(int data) {
	int x=0;
	os_delay_us(CLKDELAY);
	for (x=0; x<8; x++) {
		if (data&0x80) {
			gpio_output_set((1<<MOSI_PIN), 0, (1<<MOSI_PIN), 0);
		} else {
			gpio_output_set(0, (1<<MOSI_PIN), (1<<MOSI_PIN), 0);
		}
		data<<=1;
		gpio_output_set(0, (1<<CLK_PIN), (1<<CLK_PIN), 0);
		os_delay_us(CLKDELAY);
		gpio_output_set((1<<CLK_PIN), 0, (1<<CLK_PIN), 0);
		os_delay_us(CLKDELAY);
	}
	gpio_output_set(0, (1<<CLK_PIN), (1<<CLK_PIN), 0);
	os_delay_us(CLKDELAY);
}

static int spiRecv(void) {
	int x=0, data=0;
	gpio_output_set((1<<MOSI_PIN), 0, (1<<MOSI_PIN), 0);
	for (x=0; x<8; x++) {
		data<<=1;
		gpio_output_set(0, (1<<CLK_PIN), (1<<CLK_PIN), 0);
		os_delay_us(CLKDELAY);
		if (GPIO_INPUT_GET(MISO_PIN)) data|=1;
		gpio_output_set((1<<CLK_PIN), 0, (1<<CLK_PIN), 0);
		os_delay_us(CLKDELAY);
	}
	gpio_output_set(0, (1<<CLK_PIN), (1<<CLK_PIN), 0);
	os_delay_us(CLKDELAY);
	return data;
}
#endif

int ICACHE_FLASH_ATTR sdcardReadR7Data() {
	int x;
	x=spiRecv()<<24;
	x|=spiRecv()<<16;
	x|=spiRecv()<<8;
	x|=spiRecv();
	return x;
}

int ICACHE_FLASH_ATTR sdcardCmd(int cmd, uint32_t arg) {
	int resp;
	int failTimer=1000;
	if (cmd==STOP_TRANSMISSION) {
		SETCS(1);
		spiSend(0xff);
		spiSend(0xff);
		SETCS(0);
	}
	spiSend(0xff);
	spiSend(cmd|0x40);
	spiSend((arg>>24)&0xff);
	spiSend((arg>>16)&0xff);
	spiSend((arg>>8)&0xff);
	spiSend((arg)&0xff);
	if (cmd==SEND_IF_COND) {
		spiSend(0x87);
	} else if (cmd==READ_OCR) {
		spiSend(0x75);
	} else {
		spiSend(0x95); //CMD0 checksum
	}
	os_printf("Send cmd %d\n", cmd);
	if (cmd==STOP_TRANSMISSION) spiSend(0xff); //dummy
	do {
		resp=spiRecv();
		failTimer--;
//		if (resp==0xff) os_delay_us(100);
	} while (resp==0xff && failTimer!=0);
	os_printf("SD cmd %d resp 0x%x\n", cmd, resp);
	return resp;
}


static int ICACHE_FLASH_ATTR sdcardInit() {
	int x, t;
	int failTimer=20;
	//Enable SPI
#ifdef HWSPI
	WRITE_PERI_REG(PERIPHS_IO_MUX, 0x105);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, 2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, 2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, 2);
	CLEAR_PERI_REG_MASK(SPI_USER(1), SPI_FLASH_MODE );
	SET_PERI_REG_MASK(SPI_USER(1), SPI_WR_BYTE_ORDER | SPI_RD_BYTE_ORDER | SPI_CS_SETUP | SPI_CS_HOLD);
	//clear Dual or Quad lines transmission mode
	CLEAR_PERI_REG_MASK(SPI_CTRL(1), SPI_QIO_MODE |SPI_DIO_MODE | SPI_DOUT_MODE | SPI_QOUT_MODE);

	
	// set clk divider
	WRITE_PERI_REG(SPI_CLOCK(1),
		((3 & SPI_CLKDIV_PRE) << SPI_CLKDIV_PRE_S) |	// sets the divider 0 = 20MHz , 1 = 10MHz , 2 = 6.667MHz , 3 = 5MHz , 4 = 4M$
		((3 & SPI_CLKCNT_N) << SPI_CLKCNT_N_S) |		// sets the SCL cycle length in SPI_CLKDIV_PRE cycles
		((1 & SPI_CLKCNT_H) << SPI_CLKCNT_H_S) |		// sets the SCL high time in SPI_CLKDIV_PRE cycles
		((3 & SPI_CLKCNT_L) << SPI_CLKCNT_L_S));		// sets the SCL low time in SPI_CLKDIV_PRE cycles
														// N == 3, H == 1, L == 3 : 50% Duty Cycle
														// clear bit 31,set SPI clock div

	//set 8bit output buffer length, the buffer is the low 8bit of register"SPI_FLASH_C0"
	WRITE_PERI_REG(SPI_USER1(1),
		((7 & SPI_USR_MOSI_BITLEN) << SPI_USR_MOSI_BITLEN_S)|
		((7 & SPI_USR_MISO_BITLEN) << SPI_USR_MISO_BITLEN_S));
#else
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14);
	gpio_output_set(0, 0, (1<<CLK_PIN)|(1<<MOSI_PIN), (1<<MISO_PIN));
#endif
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);
	gpio_output_set(0, 0, (1<<CS_PIN), 0);

	SETCS(1);
	for (x=0; x<10; x++) spiSend(0xff);
	SETCS(0);
/*
	do {
		x=sdcardCmd(GO_IDLE_STATE, 0);
	} while (x!=1);

	do {
		SETCS(0);
		x=sdcardCmd(SEND_IF_COND, 0x1AA);
		SETCS(1);
	} while (x!=1 && x!=5);
	//If x==5, card is a normal SD card.
	if (x==1) {
		sdcardReadR7Data();
	}

	do {
		SETCS(0);
		x=sdcardCmd(SEND_OP_COND,0);
		SETCS(1);
	} while(x!=0 && x!=5);

	do {
		SETCS(0);
		sdcardCmd(APP_CMD,0);
		x=sdcardCmd(SD_SEND_OP_COND, 0x0);
		SETCS(1);
	} while (x!=1 && x!=5 && x!=0);
//	} while (x!=5 && x!=0);

	if (x==5) {
		while (sdcardCmd(READ_OCR, 0)!=0);
	}
*/
	
	do {
		SETCS(0);
		x=sdcardCmd(GO_IDLE_STATE, 0);
		SETCS(1);
		failTimer--;
	} while (x!=1 && failTimer!=0);
	do {
		SETCS(0);
		x=sdcardCmd(SEND_IF_COND, 0x1aa);
		if (x!=1) SETCS(1);
		failTimer--;
	} while (x!=1 && failTimer!=0);
	if (x==1) {
		x=sdcardReadR7Data();
		os_printf("IFCOND: %x\n", x);
		SETCS(1);
	}
	do {
		SETCS(0);
		x=sdcardCmd(APP_CMD, 0);
		x=x && sdcardCmd(SD_SEND_OP_COND, 0x40000000);
		SETCS(1);
		failTimer--;
	} while (x && failTimer!=0);
	needByteAddr=1;
	do {
		SETCS(0);
		x=sdcardCmd(READ_OCR, 00);
		if (x==0) {
			t=sdcardReadR7Data();
			os_printf("OCR: %x\n", t);
			if (t&0x40000000) needByteAddr=0;
			os_printf("Using %s addressing.\n", needByteAddr?"byte":"block");
		}
		SETCS(1);
		failTimer--;
	} while (x!=0 && failTimer!=0);

	SETCS(1);
	system_update_cpu_freq(160);
	return 1;
}

int sdcardWriteBlk(unsigned long addr) {
	//na
	return RES_ERROR;
}

// ---- Fatfs glue ----
DSTATUS disk_initialize (BYTE pdrv) {
	if (pdrv) return STA_NOINIT;
	return sdcardInit()?0:STA_NOINIT;
}

DSTATUS disk_status (BYTE pdrv) {
	if (pdrv) return STA_NOINIT;
	return 0;
}

DRESULT disk_read (
	BYTE drv,			/* Physical drive nmuber (0) */
	BYTE *buff,			/* Pointer to the data buffer to store read data */
	DWORD sector,		/* Start sector number (LBA) */
	UINT count			/* Sector count (1..128) */
) {
	int i,j;
	BYTE *p=buff;
	os_printf("Rd sec %d ct %d\n", (int)sector, (int)count);
	if (needByteAddr) sector<<=9;
	do {
		SETCS(0);
		j=sdcardCmd(count>1?READ_MULTIPLE_BLOCKS:READ_SINGLE_BLOCK, sector);
		if (j!=0) SETCS(1);
	} while (j!=0);
	for (j=0; j<count; j++) {
		while (spiRecv()!=0xfe);
		for (i=0; i<512; i++) {
			*p++=spiRecv();
		}
	}
	if (count>1) {
		sdcardCmd(STOP_TRANSMISSION, 0);
	}
	SETCS(1);
	return RES_OK;
}
