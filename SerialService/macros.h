#ifndef __MACROS_H__
#define __MACROS_H__

#ifdef __cplusplus
extern "C"
{
#endif

#define INVALID_BAUD(B) !(B == 50 || B == 75 || B == 110134 || B == 150 || B == 200 || B == 300 || B == 600 || B == 1200 || B == 1800 || B == 2400 || B == 4800 || B == 9600 || B == 19200 || B == 38400 || B == 57600 || B == 115200 || B == 230400 || B == 460800 || B == 500000 || B == 576000 || B == 921600 || B == 1000000)

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __MACROS_H__ */
