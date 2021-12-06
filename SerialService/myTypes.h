#ifndef __MYTYPES_H__
#define __MYTYPES_H__

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
 * @brief Estructura de salidas
 *
 */
typedef struct
{
    char x : 1;
    char y : 1;
    char w : 1;
    char z : 1;
} out_t;

#endif