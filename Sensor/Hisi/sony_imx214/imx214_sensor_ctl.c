 /*******************************************************************************
 * File Name : im214driver
 * Version : 0.0
 * Author : warren_wzw
 * Created : 2023.05.13
 * Last Modified :
 * Description :  
 * Function List :
 * Modification History :
******************************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include "hi_comm_video.h"
#include "hi_sns_ctrl.h"

#ifdef HI_GPIO_I2C
#include "gpioi2c_ex.h"
#else
#include "hi_i2c.h"
#endif

const unsigned char imx214_i2c_addr = 0x20;     /* I2C Address of IMX214 */
const unsigned int  imx214_addr_byte = 2;
const unsigned int  imx214_data_byte = 1;
static int g_fd[ISP_MAX_PIPE_NUM] = {[0 ...(ISP_MAX_PIPE_NUM - 1)] = -1};

extern ISP_SNS_STATE_S       *g_pastImx214[ISP_MAX_PIPE_NUM];
extern ISP_SNS_COMMBUS_U      g_aunImx214BusInfo[];
//void imx214_wdr_1080p30_2to1_init(VI_PIPE ViPipe);
void imx214_linear_1080p30_init(VI_PIPE ViPipe);

int imx214_i2c_init(VI_PIPE ViPipe)
{
    char acDevFile[16] = {0};
    HI_U8 u8DevNum;

    if (g_fd[ViPipe] >= 0) {
        return HI_SUCCESS;
    }
#ifdef HI_GPIO_I2C
    int ret;

    g_fd[ViPipe] = open("/dev/gpioi2c_ex", O_RDONLY, S_IRUSR);
    if (g_fd[ViPipe] < 0) {
        ISP_ERR_TRACE("Open gpioi2c_ex error!\n");
        return HI_FAILURE;
    }
#else
    int ret;

    u8DevNum = g_aunImx214BusInfo[ViPipe].s8I2cDev;
    snprintf(acDevFile, sizeof(acDevFile),  "/dev/i2c-%u", u8DevNum);
    //printf("-------------------/dev/i2c-%u--\n",u8DevNum);

    g_fd[ViPipe] = open(acDevFile, O_RDWR, S_IRUSR | S_IWUSR);

    if (g_fd[ViPipe] < 0) {
        ISP_ERR_TRACE("Open /dev/hi_i2c_drv-%u error!\n", u8DevNum);
        return HI_FAILURE;
    }

    ret = ioctl(g_fd[ViPipe], I2C_SLAVE_FORCE, (imx214_i2c_addr >> 1));
    if (ret < 0) {
        ISP_ERR_TRACE("I2C_SLAVE_FORCE error!\n");
        close(g_fd[ViPipe]);
        g_fd[ViPipe] = -1;
        return ret;
    }
#endif

    return HI_SUCCESS;
}

int imx214_i2c_exit(VI_PIPE ViPipe)
{
    if (g_fd[ViPipe] >= 0) {
        close(g_fd[ViPipe]);
        g_fd[ViPipe] = -1;
        return HI_SUCCESS;
    }
    return HI_FAILURE;
}

int imx214_read_register(VI_PIPE ViPipe, int addr)
{
    return HI_SUCCESS;
}

int imx214_write_register(VI_PIPE ViPipe, int addr, int data)
{
    if (g_fd[ViPipe] < 0) {
        return HI_SUCCESS;
    }

#ifdef HI_GPIO_I2C
    i2c_data.dev_addr = imx214_i2c_addr;
    i2c_data.reg_addr = addr;
    i2c_data.addr_byte_num = imx214_addr_byte;
    i2c_data.data = data;
    i2c_data.data_byte_num = imx214_data_byte;

    ret = ioctl(g_fd[ViPipe], GPIO_I2C_WRITE, &i2c_data);

    if (ret) {
        ISP_ERR_TRACE("GPIO-I2C write faild!\n");
        return ret;
    }
#else
    int idx = 0;
    int ret;
    char buf[8];

    if (imx214_addr_byte == 2) {
        buf[idx] = (addr >> 8) & 0xff;
        idx++;
        buf[idx] = addr & 0xff;
        idx++;
    } else {
    }

    if (imx214_data_byte == 2) {
    } else {
        buf[idx] = data & 0xff;
        idx++;
    }

    ret = write(g_fd[ViPipe], buf, imx214_addr_byte + imx214_data_byte);
    if (ret < 0) {
        ISP_ERR_TRACE("I2C_WRITE error!\n");
        return HI_FAILURE;
    }

#endif
    return HI_SUCCESS;
}

static void delay_ms(int ms)
{
    usleep(ms * 1000);
}

void imx214_prog(VI_PIPE ViPipe, const int *rom)
{
    int i = 0;
    while (1) {
        int lookup = rom[i++];
        int addr = (lookup >> 16) & 0xFFFF;
        int data = lookup & 0xFFFF;
        if (addr == 0xFFFE) {
            delay_ms(data);
        } else if (addr == 0xFFFF) {
            return;
        } else {
            imx214_write_register(ViPipe, addr, data);
        }
    }
}

void imx214_standby(VI_PIPE ViPipe)
{
    imx214_write_register(ViPipe, 0x3000, 0x01);  /* STANDBY */
    imx214_write_register(ViPipe, 0x3002, 0x01);  /* XTMSTA */

    return;
}

void imx214_restart(VI_PIPE ViPipe)
{
    imx214_write_register(ViPipe, 0x3000, 0x00);  /* standby */
    delay_ms(20);
    imx214_write_register(ViPipe, 0x3002, 0x00);  /* master mode start */
    imx214_write_register(ViPipe, 0x304b, 0x0a);

    return;
}

#define IMX214_SENSOR_1080P_30FPS_LINEAR_MODE  (1)
#define IMX214_SENSOR_1080P_30FPS_2t1_WDR_MODE (2)

 

void imx214_default_reg_init(VI_PIPE ViPipe)
{
    HI_U32 i;

    for (i = 0; i < g_pastImx214[ViPipe]->astRegsInfo[0].u32RegNum; i++) {
        imx214_write_register(ViPipe, g_pastImx214[ViPipe]->astRegsInfo[0].astI2cData[i].u32RegAddr, g_pastImx214[ViPipe]->astRegsInfo[0].astI2cData[i].u32Data);
    }
}

void imx214_init(VI_PIPE ViPipe)
{
    WDR_MODE_E       enWDRMode;
    HI_BOOL          bInit;
    HI_U8            u8ImgMode;

    bInit       = g_pastImx214[ViPipe]->bInit;
    enWDRMode   = g_pastImx214[ViPipe]->enWDRMode;
    u8ImgMode   = g_pastImx214[ViPipe]->u8ImgMode;

    imx214_i2c_init(ViPipe);

    /* When sensor first init, config all registers */
    if (bInit == HI_FALSE) {
        if (WDR_MODE_2To1_LINE == enWDRMode) {
            if (IMX214_SENSOR_1080P_30FPS_2t1_WDR_MODE == u8ImgMode) {  /* IMX214_SENSOR_1080P_30FPS_2t1_WDR_MODE */
                //imx214_wdr_1080p30_2to1_init(ViPipe);
            } else {
            }
        } else {
            imx214_linear_1080p30_init(ViPipe);
        }
    } else {
    /* When sensor switch mode(linear<->WDR or resolution), config different registers(if possible) */
        if (WDR_MODE_2To1_LINE == enWDRMode) {
            if (IMX214_SENSOR_1080P_30FPS_2t1_WDR_MODE == u8ImgMode) {  /* IMX214_SENSOR_1080P_30FPS_2t1_WDR_MODE */
                //imx214_wdr_1080p30_2to1_init(ViPipe);
            } else {
            }
        } else {
            imx214_linear_1080p30_init(ViPipe);
        }
    }

    g_pastImx214[ViPipe]->bInit = HI_TRUE;
    return;
}

void imx214_exit(VI_PIPE ViPipe)
{
    imx214_i2c_exit(ViPipe);

    return;
}

/* 1080P30 and 1080P25 */
void imx214_linear_1080p30_init(VI_PIPE ViPipe)
{
        // BASIC SETTING  TOTAL 20
    imx214_write_register(ViPipe, 0x0101, 0x00);     
    imx214_write_register(ViPipe, 0x0105, 0x01);     
    imx214_write_register(ViPipe, 0x0106, 0x01);
    imx214_write_register(ViPipe, 0x4550, 0x02);
    imx214_write_register(ViPipe, 0x4601, 0x04);     
    imx214_write_register(ViPipe, 0x4642, 0x01);
    imx214_write_register(ViPipe, 0x6227, 0x11);
    imx214_write_register(ViPipe, 0x9276, 0x00);
    imx214_write_register(ViPipe, 0x900E, 0x06);
    imx214_write_register(ViPipe, 0xA802, 0x90);
    imx214_write_register(ViPipe, 0xA803, 0x11);
    imx214_write_register(ViPipe, 0xA804, 0x62);
    imx214_write_register(ViPipe, 0xA805, 0x77);
    imx214_write_register(ViPipe, 0xA806, 0xAE);
    imx214_write_register(ViPipe, 0xA807, 0x34);
    imx214_write_register(ViPipe, 0xA808, 0xAE);
    imx214_write_register(ViPipe, 0xA809, 0x35);
    imx214_write_register(ViPipe, 0xA80A, 0x62);
    imx214_write_register(ViPipe, 0xA80B, 0x83);
    imx214_write_register(ViPipe, 0xAE33, 0x00);
//ANALOG SETTING TOTAL 28
    imx214_write_register(ViPipe, 0x4174, 0x00);
    imx214_write_register(ViPipe, 0x4175, 0x11);
    imx214_write_register(ViPipe, 0x4612, 0x29);
    imx214_write_register(ViPipe, 0x461B, 0x2C);
    imx214_write_register(ViPipe, 0x461F, 0x06);
    imx214_write_register(ViPipe, 0x4635, 0x07);
    imx214_write_register(ViPipe, 0x4637, 0x30);
    imx214_write_register(ViPipe, 0x463F, 0x18);
    imx214_write_register(ViPipe, 0x4641, 0x0D);
    imx214_write_register(ViPipe, 0x465B, 0x2C);
    imx214_write_register(ViPipe, 0x465F, 0x2B);
    imx214_write_register(ViPipe, 0x4663, 0x2B);
    imx214_write_register(ViPipe, 0x4667, 0x24);
    imx214_write_register(ViPipe, 0x466F, 0x24);
    imx214_write_register(ViPipe, 0x470E, 0x09);
    imx214_write_register(ViPipe, 0x4909, 0xAB);
    imx214_write_register(ViPipe, 0x490B, 0x95);
    imx214_write_register(ViPipe, 0x4915, 0x5D);
    imx214_write_register(ViPipe, 0x4A5F, 0xFF);
    imx214_write_register(ViPipe, 0x4A61, 0xFF);
    imx214_write_register(ViPipe, 0x4A73, 0x62);
    imx214_write_register(ViPipe, 0x4A85, 0x00);
    imx214_write_register(ViPipe, 0x4A87, 0xFF);
   // imx214_write_register(ViPipe, 0x583C, 0x04);
    imx214_write_register(ViPipe, 0x620E, 0x04);
    imx214_write_register(ViPipe, 0x6EB2, 0x01);
    imx214_write_register(ViPipe, 0x6EB3, 0x00);
    imx214_write_register(ViPipe, 0x9300, 0x02);
 //image Quality 
 //HDR setting

    imx214_write_register(ViPipe, 0x3001, 0x07);
    imx214_write_register(ViPipe, 0x9344, 0x03);
    imx214_write_register(ViPipe, 0x9706, 0x10);
    imx214_write_register(ViPipe, 0x9707, 0x03);
    imx214_write_register(ViPipe, 0x9708, 0x03);
    imx214_write_register(ViPipe, 0x97BE, 0x01);
    imx214_write_register(ViPipe, 0x97BF, 0x01);
    imx214_write_register(ViPipe, 0x97C0, 0x01);
    imx214_write_register(ViPipe, 0x9E04, 0x01);
    imx214_write_register(ViPipe, 0x9E05, 0x00);
    imx214_write_register(ViPipe, 0x9E0C, 0x01);
    imx214_write_register(ViPipe, 0x9E0D, 0x02);
    imx214_write_register(ViPipe, 0x9E24, 0x00);
    imx214_write_register(ViPipe, 0x9E25, 0x8C);
    imx214_write_register(ViPipe, 0x9E26, 0x00);
    imx214_write_register(ViPipe, 0x9E27, 0x94);
    imx214_write_register(ViPipe, 0x9E28, 0x00);
    imx214_write_register(ViPipe, 0x9E29, 0x96);
     	// CNR Setting
    imx214_write_register(ViPipe, 0x69DB, 0x01);
	     // Moire reduction Parameter Setting
    imx214_write_register(ViPipe, 0x6957, 0x01);
         // Image enhancement Setting
    imx214_write_register(ViPipe, 0x6987, 0x17);
    imx214_write_register(ViPipe, 0x698A, 0x03);
    imx214_write_register(ViPipe, 0x698B, 0x03);
      // White Balance Setting
    imx214_write_register(ViPipe, 0x0B8E, 0x01);
    imx214_write_register(ViPipe, 0x0B8F, 0x00);
    imx214_write_register(ViPipe, 0x0B90, 0x01);
    imx214_write_register(ViPipe, 0x0B91, 0x00);
    imx214_write_register(ViPipe, 0x0B92, 0x01);
    imx214_write_register(ViPipe, 0x0B93, 0x00);
    imx214_write_register(ViPipe, 0x0B94, 0x01);
    imx214_write_register(ViPipe, 0x0B95, 0x10);
      	// ATR Setting
    imx214_write_register(ViPipe, 0x6E50,0x00);
    imx214_write_register(ViPipe, 0x6E51,0x32);
    imx214_write_register(ViPipe, 0x9340,0x00);
    imx214_write_register(ViPipe, 0x9341,0x3C);
    imx214_write_register(ViPipe, 0x9342,0x03);
    imx214_write_register(ViPipe, 0x9343,0xFF);

// Mode setting setting TOTAL 26
    imx214_write_register(ViPipe, 0x0114, 0x03);
    imx214_write_register(ViPipe, 0x0220, 0x00);
    imx214_write_register(ViPipe, 0x0221, 0x11);
    imx214_write_register(ViPipe, 0x0222, 0x01);
    imx214_write_register(ViPipe, 0x0340, 0x04);
    imx214_write_register(ViPipe, 0x0341, 0x68);
    imx214_write_register(ViPipe, 0x0342, 0x13);
    imx214_write_register(ViPipe, 0x0343, 0x90);
    imx214_write_register(ViPipe, 0x0344, 0x04);
    imx214_write_register(ViPipe, 0x0345, 0x78);
    imx214_write_register(ViPipe, 0x0346, 0x03);
    imx214_write_register(ViPipe, 0x0347, 0xFC);
    imx214_write_register(ViPipe, 0x0348, 0x0B);
    imx214_write_register(ViPipe, 0x0349, 0xF7);
    imx214_write_register(ViPipe, 0x034A, 0x08);
    imx214_write_register(ViPipe, 0x034B, 0x33);
    imx214_write_register(ViPipe, 0x0381, 0x01);
    imx214_write_register(ViPipe, 0x0383, 0x01);
    imx214_write_register(ViPipe, 0x0385, 0x01);
    imx214_write_register(ViPipe, 0x0387, 0x01);
    imx214_write_register(ViPipe, 0x0900, 0x00);
    imx214_write_register(ViPipe, 0x0901, 0x00);
    imx214_write_register(ViPipe, 0x0902, 0x00);
    imx214_write_register(ViPipe, 0x3000, 0x35);
    imx214_write_register(ViPipe, 0x3054, 0x01);
    imx214_write_register(ViPipe, 0x305C, 0x11);
//Output Size setting TOTAL17
    imx214_write_register(ViPipe, 0x0112,0x0A);
    imx214_write_register(ViPipe, 0x0112, 0x0A);
    imx214_write_register(ViPipe, 0x034C, 0x07);
    imx214_write_register(ViPipe, 0x034D, 0x80);
    imx214_write_register(ViPipe, 0x034E, 0x04);
    imx214_write_register(ViPipe, 0x034F, 0x38);
    imx214_write_register(ViPipe, 0x0401, 0x00);
    imx214_write_register(ViPipe, 0x0404, 0x00);
    imx214_write_register(ViPipe, 0x0405, 0x01);
    imx214_write_register(ViPipe, 0x0408, 0x00);
    imx214_write_register(ViPipe, 0x0409, 0x00);
    imx214_write_register(ViPipe, 0x040A, 0x00);
    imx214_write_register(ViPipe, 0x040B, 0x00);
    imx214_write_register(ViPipe, 0x040C, 0x07);
    imx214_write_register(ViPipe, 0x040D, 0x80);
    imx214_write_register(ViPipe, 0x040E, 0x04);
    imx214_write_register(ViPipe, 0x040F, 0x38);
//Clock setting  TOTAL 8
    imx214_write_register(ViPipe, 0x0301, 0x05);
    imx214_write_register(ViPipe, 0x0303, 0x02);
    imx214_write_register(ViPipe, 0x0305, 0x03);
    imx214_write_register(ViPipe, 0x0306, 0x00);
    imx214_write_register(ViPipe, 0x0307, 0x58);
    imx214_write_register(ViPipe, 0x0309, 0x0A);
    imx214_write_register(ViPipe, 0x030B, 0x01);
    imx214_write_register(ViPipe, 0x0310, 0x00);
       //DATARATING 
    imx214_write_register(ViPipe, 0x0820, 0x0D);
    imx214_write_register(ViPipe, 0x0821, 0x40);
    imx214_write_register(ViPipe, 0x0822, 0x00);
    imx214_write_register(ViPipe, 0x0823, 0x00);
//WaterMark setting TOTAL  3
    imx214_write_register(ViPipe, 0x3A03, 0x06);
    imx214_write_register(ViPipe, 0x3A04, 0x68);
    imx214_write_register(ViPipe, 0x3A05, 0x01);
    //Enable setting TOTAL
    imx214_write_register(ViPipe, 0x0B06, 0x01);
    imx214_write_register(ViPipe, 0x30A2, 0x00);
     imx214_write_register(ViPipe, 0x4018, 0x00);
    //test setting TOTAL
    imx214_write_register(ViPipe, 0x30B4, 0x00);
        //HDR setting TOTAL
    imx214_write_register(ViPipe, 0x3A02, 0xFF);
        //STATS setting TOTAL
    imx214_write_register(ViPipe, 0x3011, 0x00);
    imx214_write_register(ViPipe, 0x3013, 0x01);
    imx214_write_register(ViPipe, 0x5040, 0x01);
        //Integration Time  setting TOTAL
    imx214_write_register(ViPipe, 0x0202, 0x03);
    imx214_write_register(ViPipe, 0x0203, 0xE8);
    imx214_write_register(ViPipe, 0x0224, 0x01);
    imx214_write_register(ViPipe, 0x0225, 0xF4);
        //gain  setting TOTAL 12
    imx214_write_register(ViPipe, 0x2024, 0x01);
    imx214_write_register(ViPipe, 0x0205, 0x00);
    imx214_write_register(ViPipe, 0x020E, 0x01);
    imx214_write_register(ViPipe, 0x020F, 0x00);
    imx214_write_register(ViPipe, 0x0210, 0x01);
    imx214_write_register(ViPipe, 0x0211, 0x00);
    imx214_write_register(ViPipe, 0x0212, 0x01);
    imx214_write_register(ViPipe, 0x0213, 0x00);
    imx214_write_register(ViPipe, 0x0214, 0x01);
    imx214_write_register(ViPipe, 0x0215, 0x00);
    imx214_write_register(ViPipe, 0x0216, 0x00);
    imx214_write_register(ViPipe, 0x0217, 0x00);
   // Analog Setting total 
    imx214_write_register(ViPipe, 0x4170, 0x00);
    imx214_write_register(ViPipe, 0x4171, 0x10);
    imx214_write_register(ViPipe, 0x4176, 0x00);
    imx214_write_register(ViPipe, 0x4177, 0x3C);
    imx214_write_register(ViPipe, 0xAE20, 0x04);
    imx214_write_register(ViPipe, 0xAE21, 0x5C);

    //longer than 10msec from Power On
    imx214_write_register(ViPipe, 0x0138, 0x01);
    imx214_write_register(ViPipe, 0x0100, 0x01);
    
    // Software Standby setting
    imx214_write_register(ViPipe, 0x0100, 0x00);
    //ATR Setting
    imx214_write_register(ViPipe, 0x9300, 0x02);
    //External Clock Setting
    imx214_write_register(ViPipe, 0x136, 0x18);
    imx214_write_register(ViPipe, 0x137, 0x00);

    imx214_write_register(ViPipe, 0x0100, 0x01); 
    printf("==============================================================\n");
    printf("===Sony imx214 sensor 1080P30fps(MIPI_wzw_C6_self0514_1.1) init success!=====\n");
    printf("==============================================================\n");
    return;
}

/*
void imx214_wdr_1080p30_2to1_init(VI_PIPE ViPipe)
{
    // 10bit
    imx214_write_register(ViPipe, 0x3000, 0x01); // # standby
    delay_ms(200);

    imx214_write_register(ViPipe, 0x3005, 0x00); //  12Bit, 0x00,10Bit;
    imx214_write_register(ViPipe, 0x3007, 0x40); // VREVERSE & HREVERSE & WINMODE
    imx214_write_register(ViPipe, 0x3009, 0x01); // FRSEL&HCG
    imx214_write_register(ViPipe, 0x300A, 0x3C); // BLKLEVEL
    imx214_write_register(ViPipe, 0x300C, 0x11);
    imx214_write_register(ViPipe, 0x3011, 0x0A); //  Change after reset;
    imx214_write_register(ViPipe, 0x3014, 0x02); //  Gain
    imx214_write_register(ViPipe, 0x3018, 0xC4); //  VMAX[7:0]
    imx214_write_register(ViPipe, 0x3019, 0x04); //  VMAX[15:8]
    imx214_write_register(ViPipe, 0x301A, 0x00); //  VMAX[16]
    imx214_write_register(ViPipe, 0x301C, 0xEC); //  HMAX[7:0]      0x14a0->25fps;
    imx214_write_register(ViPipe, 0x301D, 0x07); //  HMAX[15:8]     0x1130->30fps;
    imx214_write_register(ViPipe, 0x3020, 0x02); //  SHS1
    imx214_write_register(ViPipe, 0x3021, 0x00);
    imx214_write_register(ViPipe, 0x3022, 0x00);
    imx214_write_register(ViPipe, 0x3024, 0x53); //  SHS2
    imx214_write_register(ViPipe, 0x3025, 0x04);
    imx214_write_register(ViPipe, 0x3026, 0x00);
    imx214_write_register(ViPipe, 0x3030, 0xE1); //  RHS1
    imx214_write_register(ViPipe, 0x3031, 0x00);
    imx214_write_register(ViPipe, 0x3032, 0x00);
    imx214_write_register(ViPipe, 0x303A, 0x08);
    imx214_write_register(ViPipe, 0x303C, 0x04); //  WINPV
    imx214_write_register(ViPipe, 0x303D, 0x00);
    imx214_write_register(ViPipe, 0x303E, 0x41); //  WINWV
    imx214_write_register(ViPipe, 0x303F, 0x04);
    imx214_write_register(ViPipe, 0x3045, 0x05); //  DOLSCDEN & DOLSYDINFOEN & HINFOEN
    imx214_write_register(ViPipe, 0x3046, 0x00); //  OPORTSE & ODBIT
    imx214_write_register(ViPipe, 0x304B, 0x0A); //  XVSOUTSEL & XHSOUTSEL
    imx214_write_register(ViPipe, 0x305C, 0x18); //  INCKSEL1,1080P,CSI-2,37.125MHz;74.25MHz->0x0C
    imx214_write_register(ViPipe, 0x305D, 0x03); //  INCKSEL2,1080P,CSI-2,37.125MHz;74.25MHz->0x03
    imx214_write_register(ViPipe, 0x305E, 0x20); //  INCKSEL3,1080P,CSI-2,37.125MHz;74.25MHz->0x10
    imx214_write_register(ViPipe, 0x305F, 0x01); //  INCKSEL4,1080P,CSI-2,37.125MHz;74.25MHz->0x01
    imx214_write_register(ViPipe, 0x309E, 0x4A);
    imx214_write_register(ViPipe, 0x309F, 0x4A);

    imx214_write_register(ViPipe, 0x3106, 0x11);
    imx214_write_register(ViPipe, 0x311C, 0x0E);
    imx214_write_register(ViPipe, 0x3128, 0x04);
    imx214_write_register(ViPipe, 0x3129, 0x1D); //  ADBIT1,12Bit;0x1D->10Bit;
    imx214_write_register(ViPipe, 0x313B, 0x41);
    imx214_write_register(ViPipe, 0x315E, 0x1A); //  INCKSEL5,1080P,CSI-2,37.125MHz;74.25MHz->0x1B
    imx214_write_register(ViPipe, 0x3164, 0x1A); //  INCKSEL6,1080P,CSI-2,37.125MHz;74.25MHz->0x1B
    imx214_write_register(ViPipe, 0x317C, 0x12); //  ADBIT2,12Bit;0x12->10Bit;
    imx214_write_register(ViPipe, 0x31EC, 0x37); //  ADBIT3,12Bit;0x37->10Bit;

    // ##MIPI setting
    imx214_write_register(ViPipe, 0x3405, 0x10); //  REPETITION
    imx214_write_register(ViPipe, 0x3407, 0x03);
    imx214_write_register(ViPipe, 0x3414, 0x00);
    imx214_write_register(ViPipe, 0x3415, 0x00);
    imx214_write_register(ViPipe, 0x3418, 0x7A); //  Y_OUT_SIZE
    imx214_write_register(ViPipe, 0x3419, 0x09); //  Y_OUT_SIZE
    imx214_write_register(ViPipe, 0x3441, 0x0A); //  CSI_DT_FMT 10Bit
    imx214_write_register(ViPipe, 0x3442, 0x0A);
    imx214_write_register(ViPipe, 0x3443, 0x03); //  CSI_LANE_MODE MIPI 4CH
    imx214_write_register(ViPipe, 0x3444, 0x20); //  EXTCK_FREQ
    imx214_write_register(ViPipe, 0x3445, 0x25);
    imx214_write_register(ViPipe, 0x3446, 0x57);
    imx214_write_register(ViPipe, 0x3447, 0x00);
    imx214_write_register(ViPipe, 0x3448, 0x37);
    imx214_write_register(ViPipe, 0x3449, 0x00);
    imx214_write_register(ViPipe, 0x344A, 0x1F); //  THSPREPARE
    imx214_write_register(ViPipe, 0x344B, 0x00);
    imx214_write_register(ViPipe, 0x344C, 0x1F);
    imx214_write_register(ViPipe, 0x344D, 0x00);
    imx214_write_register(ViPipe, 0x344E, 0x1F); //  THSTRAIL
    imx214_write_register(ViPipe, 0x344F, 0x00);
    imx214_write_register(ViPipe, 0x3450, 0x77); //  TCLKZERO
    imx214_write_register(ViPipe, 0x3451, 0x00);
    imx214_write_register(ViPipe, 0x3452, 0x1F); //  TCLKPREPARE
    imx214_write_register(ViPipe, 0x3453, 0x00);
    imx214_write_register(ViPipe, 0x3454, 0x17); //  TIPX
    imx214_write_register(ViPipe, 0x3455, 0x00);
    imx214_write_register(ViPipe, 0x3472, 0xA0); //  X_OUT_SIZE
    imx214_write_register(ViPipe, 0x3473, 0x07);
    imx214_write_register(ViPipe, 0x347B, 0x23);
    imx214_write_register(ViPipe, 0x3480, 0x49); //  INCKSEL7,1080P,CSI-2,37.125MHz;74.25MHz->0x92

    imx214_default_reg_init(ViPipe);

    delay_ms(200);
    imx214_write_register(ViPipe, 0x3000, 0x00); // # Standby Cancel
    imx214_write_register(ViPipe, 0x3002, 0x00);
    imx214_write_register(ViPipe, 0x304b, 0x0a);

    printf("=========================================================================\n");
    printf("===Imx214 sensor 1080P30fps 10bit 2to1 WDR(60fps->30fps) init success!===\n");
    printf("=========================================================================\n");

    return;
}
*/