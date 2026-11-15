#ifndef SC7A20_H
#define SC7A20_H

#include <linux/ioctl.h>

#define SC7A20_I2C_SLAVE_ADDR_LOW   0x18
#define SC7A20_I2C_SLAVE_ADDR_HIGH  0x19

#define SC7A20_REG_WHO_AM_I         0x0F
#define SC7A20_CHIP_ID              0x11

#define SC7A20_REG_CTRL1            0x20
#define SC7A20_REG_CTRL2            0x21
#define SC7A20_REG_CTRL3            0x22
#define SC7A20_REG_CTRL4            0x23
#define SC7A20_REG_OUT_X_L          0x28

#define SC7A20_SUCCESS              0
#define SC7A20_ERR_I2C              -1
#define SC7A20_ERR_IDENTIFICATION   -6

#define SC7A20_BUFSIZE              256
#define SC7A20_AXIS_X               0
#define SC7A20_AXIS_Y               1
#define SC7A20_AXIS_Z               2
#define SC7A20_AXES_NUM             3
#define SC7A20_DATA_LEN             6
#define SC7A20_DEV_NAME             "SC7A20"

#endif
