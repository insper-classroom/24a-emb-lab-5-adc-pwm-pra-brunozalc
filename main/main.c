/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <task.h>

#include "hardware/adc.h"
#include "pico/stdlib.h"
#include <stdio.h>

#include <math.h>
#include <stdlib.h>

QueueHandle_t xQueueAdc;

typedef struct adc {
    int axis;
    int val;
} adc_t;

void write_package(adc_t data) {
    int val = data.val;
    int msb = val >> 8;
    int lsb = val & 0xFF;

    uart_putc_raw(uart0, data.axis);
    uart_putc_raw(uart0, lsb);
    uart_putc_raw(uart0, msb);
    uart_putc_raw(uart0, -1);
}

void x_task(void *p) {
    adc_t data;
    adc_init();
    adc_gpio_init(26);
    adc_select_input(0);

    while (1) {
        data.axis = 0;
        data.val = adc_read();

        // analogic filter from 0-4095 to -255-255 with a central deadzone from -30 to 30
        int mapped_val = ((data.val * (255 - (-255))) / (4095 - 0)) + (-255);
        if (mapped_val > -30 && mapped_val < 30) {
            mapped_val = 0;
        }

        data.val = mapped_val;
        xQueueSend(xQueueAdc, &data, portMAX_DELAY);
    }
}

void y_task(void *p) {
    adc_t data;
    adc_init();
    adc_gpio_init(27);
    adc_select_input(1);

    while (1) {
        data.axis = 1;
        data.val = adc_read();

        // analogic filter from 0-4095 to -255-255 with a central deadzone from -30 to 30
        int mapped_val = ((data.val * (255 - (-255))) / (4095 - 0)) + (-255);
        if (mapped_val > -30 && mapped_val < 30) {
            mapped_val = 0;
        }

        data.val = mapped_val;
        xQueueSend(xQueueAdc, &data, portMAX_DELAY);
    }
}

void uart_task(void *p) {
    adc_t data;

    while (1) {
        xQueueReceive(xQueueAdc, &data, portMAX_DELAY);
        write_package(data);
    }
}

int main() {
    stdio_init_all();

    xQueueAdc = xQueueCreate(32, sizeof(adc_t));

    xTaskCreate(x_task, "x_task", 4096, NULL, 1, NULL);
    xTaskCreate(y_task, "y_task", 4096, NULL, 1, NULL);
    xTaskCreate(uart_task, "uart_task", 4096, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}
