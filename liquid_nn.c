#include "liquid_nn.h"
#include "lib/printf.h"

// Состояния нейронов
static fxp_t neurons[NUM_NEURONS];
static fxp_t weights[NUM_NEURONS][NUM_NEURONS];
static fxp_t input_weights[INPUT_SIZE][NUM_NEURONS];
static fxp_t tau[NUM_NEURONS];  // постоянные времени

void liquid_init(void) {
    for (int i = 0; i < NUM_NEURONS; i++) {
        neurons[i] = FXP_FROM_FLOAT(0.1f);
        tau[i] = FXP_FROM_FLOAT(1.0f);
        for (int j = 0; j < NUM_NEURONS; j++) {
            weights[i][j] = FXP_FROM_FLOAT(0.01f);
        }
        for (int j = 0; j < INPUT_SIZE; j++) {
            input_weights[j][i] = FXP_FROM_FLOAT(0.01f);
        }
    }
}

static fxp_t fast_tanh(fxp_t x) {
    if (x > FXP_FROM_FLOAT(3.0f)) return FXP_FROM_FLOAT(1.0f);
    if (x < FXP_FROM_FLOAT(-3.0f)) return FXP_FROM_FLOAT(-1.0f);
    return x;
}

void liquid_step(const fxp_t input[INPUT_SIZE]) {
    fxp_t dN[NUM_NEURONS];

    for (int i = 0; i < NUM_NEURONS; i++) {
        fxp_t total_input = 0;

        // Вклад от входа
        for (int j = 0; j < INPUT_SIZE; j++) {
            total_input = total_input + FXP_MUL(input[j], input_weights[j][i]);
        }

        // Вклад от других нейронов
        for (int j = 0; j < NUM_NEURONS; j++) {
            total_input = total_input + FXP_MUL(neurons[j], weights[j][i]);
        }

        // dN/dt = (-neuron + total_input) / tau
        dN[i] = FXP_DIV((total_input - neurons[i]), tau[i]);
    }

    // Обновляем состояние
    for (int i = 0; i < NUM_NEURONS; i++) {
        neurons[i] = neurons[i] + dN[i];
    }
}

void update_weights_liquid(const fxp_t input[INPUT_SIZE], fxp_t target) {
    for (int i = 0; i < NUM_NEURONS; i++) {
        fxp_t error = neurons[i] - target;
        for (int j = 0; j < INPUT_SIZE; j++) {
            input_weights[j][i] = input_weights[j][i] - FXP_MUL(FXP_FROM_FLOAT(0.001f), FXP_MUL(error, input[j]));
        }
    }
}

fxp_t liquid_output(void) {
    return neurons[0];
}

void kernel_liquid_predict(uint32_t pid, fxp_t last_time, fxp_t sys_freq, uint8_t is_active) {
    fxp_t input[INPUT_SIZE] = {FXP_FROM_FLOAT(pid), last_time, sys_freq};
    fxp_t target = FXP_FROM_FLOAT(is_active ? 1.0f : 0.0f);

    liquid_step(input);
    fxp_t output = liquid_output();
    update_weights_liquid(input, target);

    printf("Liquid net output: %ld.%03ld\n",
           output / FIXED_POINT_SCALE,
           (output % FIXED_POINT_SCALE) * 1000 / FIXED_POINT_SCALE);
}
