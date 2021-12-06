#ifndef __SERIAL_MANANGER_H__
#define __SERIAL_MANANGER_H__

#ifdef __cplusplus
extern "C"
{
#endif

int serial_open(int pn, int baudrate);
void serial_send(char *pData, int size);
void serial_close(void);
int serial_receive(char *buf, int size);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __SERIAL_MANANGER_H__ */
