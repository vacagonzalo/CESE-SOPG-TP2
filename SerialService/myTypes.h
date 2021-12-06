#ifndef __MYTYPES_H__
#define __MYTYPES_H__

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Estados posibles de salida
 *
 */
typedef enum
{
    OUT_OFF,
    OUT_ON,
    OUT_BLINK
} out_state_t;

/**
 * @brief 
 * 
 */
typedef struct
{
    char inBoard;
    char inBuffer;
} out_t;


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*__MYTYPES_H__*/