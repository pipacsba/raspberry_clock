/*
   2  * wiringPiI2C.c:
   3  *      Simplified I2C access routines
   4  *      Copyright (c) 2013 Gordon Henderson
   5  ***********************************************************************
   6  * This file is part of wiringPi:
   7  *      https://projects.drogon.net/raspberry-pi/wiringpi/
   8  *
   9  *    wiringPi is free software: you can redistribute it and/or modify
  10  *    it under the terms of the GNU Lesser General Public License as
  11  *    published by the Free Software Foundation, either version 3 of the
  12  *    License, or (at your option) any later version.
  13  *
  14  *    wiringPi is distributed in the hope that it will be useful,
  15  *    but WITHOUT ANY WARRANTY; without even the implied warranty of
  16  *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  17  *    GNU Lesser General Public License for more details.
  18  *
  19  *    You should have received a copy of the GNU Lesser General Public
  20  *    License along with wiringPi.
  21  *    If not, see <http://www.gnu.org/licenses/>.
  22  ***********************************************************************
  23  */
  24 
  25 /*
  26  * Notes:
  27  *      The Linux I2C code is actually the same (almost) as the SMBus code.
  28  *      SMBus is System Management Bus - and in essentially I2C with some
  29  *      additional functionality added, and stricter controls on the electrical
  30  *      specifications, etc. however I2C does work well with it and the
  31  *      protocols work over both.
  32  *
  33  *      I'm directly including the SMBus functions here as some Linux distros
  34  *      lack the correct header files, and also some header files are GPLv2
  35  *      rather than the LGPL that wiringPi is released under - presumably because
  36  *      originally no-one expected I2C/SMBus to be used outside the kernel -
  37  *      however enter the Raspberry Pi with people now taking directly to I2C
  38  *      devices without going via the kernel...
  39  *
  40  *      This may ultimately reduce the flexibility of this code, but it won't be
  41  *      hard to maintain it and keep it current, should things change.
  42  *
  43  *      Information here gained from: kernel/Documentation/i2c/dev-interface
  44  *      as well as other online resources.
  45  *********************************************************************************
  46  */
  47 
  48 #include <stdio.h>
  49 #include <stdlib.h>
  50 #include <stdint.h>
  51 #include <errno.h>
  52 #include <string.h>
  53 #include <fcntl.h>
  54 #include <sys/ioctl.h>
  55 #include <asm/ioctl.h>
  56 
  57 #include "wiringPi.h"
  58 #include "wiringPiI2C.h"
  59 
  60 // I2C definitions
  61 
  62 #define I2C_SLAVE       0x0703
  63 #define I2C_SMBUS       0x0720  /* SMBus-level access */
  64 
  65 #define I2C_SMBUS_READ  1
  66 #define I2C_SMBUS_WRITE 0
  67 
  68 // SMBus transaction types
  69 
  70 #define I2C_SMBUS_QUICK             0
  71 #define I2C_SMBUS_BYTE              1
  72 #define I2C_SMBUS_BYTE_DATA         2 
  73 #define I2C_SMBUS_WORD_DATA         3
  74 #define I2C_SMBUS_PROC_CALL         4
  75 #define I2C_SMBUS_BLOCK_DATA        5
  76 #define I2C_SMBUS_I2C_BLOCK_BROKEN  6
  77 #define I2C_SMBUS_BLOCK_PROC_CALL   7           /* SMBus 2.0 */
  78 #define I2C_SMBUS_I2C_BLOCK_DATA    8
  79 
  80 // SMBus messages
  81 
  82 #define I2C_SMBUS_BLOCK_MAX     32      /* As specified in SMBus standard */    
  83 #define I2C_SMBUS_I2C_BLOCK_MAX 32      /* Not specified but we use same structure */
  84 
  85 // Structures used in the ioctl() calls
  86 
  87 union i2c_smbus_data
  88 {
  89   uint8_t  byte ;
  90   uint16_t word ;
  91   uint8_t  block [I2C_SMBUS_BLOCK_MAX + 2] ;    // block [0] is used for length + one more for PEC
  92 } ;
  93 
  94 struct i2c_smbus_ioctl_data
  95 {
  96   char read_write ;
  97   uint8_t command ;
  98   int size ;
  99   union i2c_smbus_data *data ;
 100 } ;
 101 
 102 static inline int i2c_smbus_access (int fd, char rw, uint8_t command, int size, union i2c_smbus_data *data)
 103 {
 104   struct i2c_smbus_ioctl_data args ;
 105 
 106   args.read_write = rw ;
 107   args.command    = command ;
 108   args.size       = size ;
 109   args.data       = data ;
 110   return ioctl (fd, I2C_SMBUS, &args) ;
 111 }
 112 
 113 
 114 /*
 115  * wiringPiI2CRead:
 116  *      Simple device read
 117  *********************************************************************************
 118  */
 119 
 120 int wiringPiI2CRead (int fd)
 121 {
 122   union i2c_smbus_data data ;
 123 
 124   if (i2c_smbus_access (fd, I2C_SMBUS_READ, 0, I2C_SMBUS_BYTE, &data))
 125     return -1 ;
 126   else
 127     return data.byte & 0xFF ;
 128 }
 129 
 130 
 131 /*
 132  * wiringPiI2CReadReg8: wiringPiI2CReadReg16:
 133  *      Read an 8 or 16-bit value from a regsiter on the device
 134  *********************************************************************************
 135  */
 136 
 137 int wiringPiI2CReadReg8 (int fd, int reg)
 138 {
 139   union i2c_smbus_data data;
 140 
 141   if (i2c_smbus_access (fd, I2C_SMBUS_READ, reg, I2C_SMBUS_BYTE_DATA, &data))
 142     return -1 ;
 143   else
 144     return data.byte & 0xFF ;
 145 }
 146 
 147 int wiringPiI2CReadReg16 (int fd, int reg)
 148 {
 149   union i2c_smbus_data data;
 150 
 151   if (i2c_smbus_access (fd, I2C_SMBUS_READ, reg, I2C_SMBUS_WORD_DATA, &data))
 152     return -1 ;
 153   else
 154     return data.word & 0xFFFF ;
 155 }
 156 
 157 
 158 /*
 159  * wiringPiI2CWrite:
 160  *      Simple device write
 161  *********************************************************************************
 162  */
 163 
 164 int wiringPiI2CWrite (int fd, int data)
 165 {
 166   return i2c_smbus_access (fd, I2C_SMBUS_WRITE, data, I2C_SMBUS_BYTE, NULL) ;
 167 }
 168 
 169 
 170 /*
 171  * wiringPiI2CWriteReg8: wiringPiI2CWriteReg16:
 172  *      Write an 8 or 16-bit value to the given register
 173  *********************************************************************************
 174  */
 175 
 176 int wiringPiI2CWriteReg8 (int fd, int reg, int value)
 177 {
 178   union i2c_smbus_data data ;
 179 
 180   data.byte = value ;
 181   return i2c_smbus_access (fd, I2C_SMBUS_WRITE, reg, I2C_SMBUS_BYTE_DATA, &data) ;
 182 }
 183 
 184 int wiringPiI2CWriteReg16 (int fd, int reg, int value)
 185 {
 186   union i2c_smbus_data data ;
 187 
 188   data.word = value ;
 189   return i2c_smbus_access (fd, I2C_SMBUS_WRITE, reg, I2C_SMBUS_WORD_DATA, &data) ;
 190 }
 191 
 192 
 193 /*
 194  * wiringPiI2CSetupInterface:
 195  *      Undocumented access to set the interface explicitly - might be used
 196  *      for the Pi's 2nd I2C interface...
 197  *********************************************************************************
 198  */
 199 
 200 int wiringPiI2CSetupInterface (const char *device, int devId)
 201 {
 202   int fd ;
 203 
 204   if ((fd = open (device, O_RDWR)) < 0)
 205     return wiringPiFailure (WPI_ALMOST, "Unable to open I2C device: %s\n", strerror (errno)) ;
 206 
 207   if (ioctl (fd, I2C_SLAVE, devId) < 0)
 208     return wiringPiFailure (WPI_ALMOST, "Unable to select I2C device: %s\n", strerror (errno)) ;
 209 
 210   return fd ;
 211 }
 212 
 213 
 214 /*
 215  * wiringPiI2CSetup:
 216  *      Open the I2C device, and regsiter the target device
 217  *********************************************************************************
 218  */
 219 
 220 int wiringPiI2CSetup (const int devId)
 221 {
 222   int rev ;
 223   const char *device ;
 224 
 225   rev = piGpioLayout () ;
 226 
 227   if (rev == 1)
 228     device = "/dev/i2c-0" ;
 229   else
 230     device = "/dev/i2c-1" ;
 231 
 232   return wiringPiI2CSetupInterface (device, devId) ;
 233 }