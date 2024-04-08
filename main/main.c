#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <task.h>

#include "hardware/adc.h"
#include "pico/stdlib.h"
#include <stdio.h>

#include <math.h>
#include <stdlib.h>

#define MOVING_AVERAGE_SIZE 10
#define SENSITIVITY_SCALE 0.5

QueueHandle_t xQueueAdc;

typedef struct adc {
    int axis;
    int val;
} adc_t;

int moving_average(int new_sample, int samples[], int *sample_index) {
    samples[*sample_index] = new_sample;
    *sample_index = (*sample_index + 1) % MOVING_AVERAGE_SIZE;

    int sum = 0;
    for (int i = 0; i < MOVING_AVERAGE_SIZE; i++) {
        sum += samples[i];
    }

    return (int)(sum / MOVING_AVERAGE_SIZE);
}

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
    int samples[MOVING_AVERAGE_SIZE] = {0};
    int sample_index = 0;

    adc_init();
    adc_gpio_init(26);
    adc_set_round_robin(0b00011);

    while (1) {
        data.axis = 0;
        data.val = adc_read();

        // analogic filter from 0-4095 to -255-255
        int mapped_val = (data.val - 2047) * 255 / 2047;
        int averaged_val = moving_average(mapped_val, samples, &sample_index);

        data.val = (int)(averaged_val * SENSITIVITY_SCALE);
        xQueueSend(xQueueAdc, &data, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void y_task(void *p) {
    adc_t data;
    int samples[MOVING_AVERAGE_SIZE] = {0};
    int sample_index = 0;

    adc_init();
    adc_gpio_init(27);
    adc_set_round_robin(0b00011);

    while (1) {
        data.axis = 1;
        data.val = adc_read();

        // analogic filter from 0-4095 to -255-255
        int mapped_val = -(data.val - 2047) * 255 / 2047;
        int averaged_val = moving_average(mapped_val, samples, &sample_index);

        data.val = (int)(averaged_val * SENSITIVITY_SCALE);
        xQueueSend(xQueueAdc, &data, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void uart_task(void *p) {
    adc_t data;

    while (1) {
        if (xQueueReceive(xQueueAdc, &data, pdMS_TO_TICKS(10))) {
            if (data.val < -30 || data.val > 30) {
                write_package(data);
            }
        }
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
