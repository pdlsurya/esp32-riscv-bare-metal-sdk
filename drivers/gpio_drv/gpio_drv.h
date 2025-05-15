#pragma once

#include <stdint.h>
#include "hal/gpio_ll.h"

typedef enum
{
    GPIO_INPUT,
    GPIO_OUTPUT,
} gpio_direction_t;

/**
 * @brief Toggle a GPIO pin's state.
 * @param[in] pin The number of the GPIO pin to toggle.
 *
 * This function will toggle the level of the GPIO pin. If the pin was high, it will be set low and
 * vice versa.
 */
__attribute__((always_inline)) static inline void gpio_toggle(uint32_t pin)
{
    int current_level = gpio_ll_get_level(&GPIO, pin);
    
    gpio_ll_set_level(&GPIO, pin, !current_level);
}

/**
 * @brief Write a value to a GPIO pin.
 * @param[in] pin The number of the GPIO pin to write to.
 * @param[in] val The value to write to the pin (0 or 1).
 */
__attribute__((always_inline)) static inline void gpio_write(uint32_t pin, uint32_t val)
{
    gpio_ll_set_level(&GPIO, pin, val);
}

/**
 * @brief Read the value of a GPIO pin.
 * @param[in] pin The number of the GPIO pin to read from.
 * @return The value of the pin (0 or 1).
 */
__attribute__((always_inline)) static inline uint32_t gpio_read(uint32_t pin)
{
    return gpio_ll_get_level(&GPIO, pin);
}

/**
 * @brief Set the direction of a GPIO pin.
 * @param[in] pin The number of the GPIO pin to set the direction of.
 * @param[in] direction The direction to set the pin to (GPIO_INPUT or GPIO_OUTPUT).
 */
__attribute__((always_inline)) static inline void gpio_set_direction(uint32_t pin, gpio_direction_t direction)
{
    switch (direction)
    {
    case GPIO_INPUT:
        // Enable the pin for input
        gpio_ll_input_enable(&GPIO, pin);
        break;

    case GPIO_OUTPUT:
        // Enable the pin for output
        gpio_ll_output_enable(&GPIO, pin);
        break;

    default:
        // Unknown direction, do nothing
        break;
    }
}

/**
 * @brief Set the pull-up of a GPIO pin.
 * @param[in] pin The number of the GPIO pin to set the pull-up of.
 * @param[in] pull_up The pull-up to set the pin to (0 or 1).
 */
__attribute__((always_inline)) static inline void gpio_set_pull_up(uint32_t pin, bool pull_up)
{
    if (pull_up)
    {
        gpio_ll_pullup_en(&GPIO, pin);
    }
    else
    {
        gpio_ll_pullup_dis(&GPIO, pin);
    }
}

/**
 * @brief Set the pull-down of a GPIO pin.
 * @param[in] pin The number of the GPIO pin to set the pull-down of.
 * @param[in] pull_down The pull-down to set the pin to (0 or 1).
 */
__attribute__((always_inline)) static inline void gpio_set_pull_down(uint32_t pin, bool pull_down)
{
    if (pull_down)
    {
        gpio_ll_pulldown_en(&GPIO, pin);
    }
    else
    {
        gpio_ll_pulldown_dis(&GPIO, pin);
    }
}
