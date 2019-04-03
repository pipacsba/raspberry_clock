/*
    thise header file is fakeing the i2c-dev.h - to support limited testing of code on computers without I2C
*/

/* $Id: i2c-dev.h,v 1.9 2001/08/15 03:04:58 mds Exp $ */

#ifndef I2C_DEV_H
#define I2C_DEV_H
#define I2C_SLAVE 0x0
#define I2C_INC_FAKE

// #include <linux/types.h>
// #include <linux/i2c.h>

/* Some IOCTL commands are defined in <linux/i2c.h> */
/* Note: 10-bit addresses are NOT supported! */

/* This is the structure as used in the I2C_SMBUS ioctl call */
struct i2c_smbus_ioctl_data {
	char read_write;
	unsigned char command;
	int size;
	union i2c_smbus_data *data;
};

/* This is the structure as used in the I2C_RDWR ioctl call */
struct i2c_rdwr_ioctl_data {
	struct i2c_msg *msgs;	/* pointers to i2c_msgs */
	int nmsgs;		/* number of i2c_msgs */
};


static inline unsigned int i2c_smbus_read_byte(int file)
{
	return 0x0;
}

static inline unsigned int i2c_smbus_write_byte(int file, unsigned char value)
{
	return 0x0;
}

static inline unsigned int i2c_smbus_read_byte_data(int file, unsigned char command)
{
    return 0x0;
}

static inline unsigned int i2c_smbus_write_byte_data(int file, unsigned char command, 
                                              unsigned char value)
{
	return 0x0;
}

static inline unsigned int i2c_smbus_read_word_data(int file, unsigned char command)
{
	return 0x0;
}

static inline unsigned int i2c_smbus_write_word_data(int file, unsigned char command, 
                                              unsigned short value)
{
	return 0x0;
}

static inline unsigned int i2c_smbus_process_call(int file, unsigned char command, unsigned short value)
{
    return 0x0;
}


/* Returns the number of read bytes */
static inline unsigned int i2c_smbus_read_block_data(int file, unsigned char command, 
                                              unsigned char *values)
{
    return 0x0;
}

static inline unsigned int i2c_smbus_write_block_data(int file, unsigned char command, 
                                               unsigned char length, unsigned char *values)
{
	return 0x0;
}

static inline unsigned int i2c_smbus_write_i2c_block_data(int file, unsigned char command,
                                               unsigned char length, unsigned char *values)
{
	return 0x0;
}

static inline unsigned int ioctl(int file, unsigned char command, unsigned int values)
{
	return 0x0;
}

#endif