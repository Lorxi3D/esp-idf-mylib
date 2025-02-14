# ESP32 AC Dimmer Control

## Description

Control dimmerable circuits with help of ESP32 using an external circuit like in the diagram below. This library can control AC load with PWM signal. It accept a power percentage value from 0 to 1 or direct PWM dutty cycle [0 - 1000] witch will be automatically inverted to be compatible with the circuit.
You can choose between manually control the dimmer or create a task that will handle it with FreeRTOS

<p align="center">
  <img src="./dimmer.svg" alt="Dimmer Circuit Diagram" >
</p>

## Installation

Just copy this folder into your components and import using the header
```c
#include <dimmer.h>
```
You dont need to clone all the repository for use only this component.

## Documentation

This library is based on [Espressif MCPWM](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/mcpwm.html), it requires and external circuit to handle grid voltage and it generate an inverted PWM signal witch will be synced with grid, as such it needs to known the frequency of grid, you can define it in menuconfig under "Component config -> Dimmer" (default 60Hz), note that the selected frequency is global, all your dimmers will use this.

As stated in espressif documentation you can have up to 3 PWM signal per sync source and a maximum  of 2 sync sources, the library will keep track and handle repeated sync sources to attempt make all working. The PWM signal is inverted as the triac has a minimum holding current that keeps him opened. In AC, the voltage passes 0 twice a period, during the zero-crossing the current is also 0 and then the triac can close. To take advantage on that behavior, we use an inveted logic in the PWM since opening the triac its not a problem, we calculate when to trigger it so the desired dutty is achieved until the next zero-crossing.

There are two ways of using this library you can create an structure containing all necessary data to control the PWM signal or create a task, using FreeRTOS, to handle it and control via task notification with given functions.

### Manual control

```c
typedef struct dimmer
{
    mcpwm_timer_handle_t timer;     // internal management
    mcpwm_cmpr_handle_t  comparator;// internal management
    mcpwm_gen_handle_t   generator; // internal management
    uint8_t              gen_gpio;  // generator gpio
    uint8_t              sync_gpio; // zero-crossing gpio
    float                heartz;    // zero-crossing frequency
    uint16_t             dutty;     // duty cycle 0-1000
} dimmer_t;
```
- **dimmer_t** Is the struct used to manage the dimmer, its values will be autommatically setted when
  *create_dimmer()* is called. Except for *dutty* every field is used for internal managment and should not be changed. To get the dutty cycle you can access it directilly from the struct.

```c
esp_err_t create_dimmer( dimmer_t *dimmer, uint8_t gen_gpio, uint8_t sync_gpio);
```
- **create_dimmer()** This function will initilize all parameters and requirements for a pwm signal following [Espressif MCPWM](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/mcpwm.html) example. It will also initialize the structure and enable PWM output but it will start with 0 dutty by default. There is no need to call *start_dimmer()* and the PWM will not wait for a sync signal but it will sync as soon as one is received.

```c
esp_err_t start_dimmer(dimmer_t *dimmer);
```
- **start_dimmer()** This function will start the dimmer passed in its argument, witch means the pwm signal will be emited in the correspondent GPIO and can be controled by *set_dutty()* or *set_power()*. This only removes a forced value on the PWM and only make sense to be used after *stop_dimmer()*. Its returns ESP_OK.

```c
esp_err_t stop_dimmer(dimmer_t *dimmer);
```
- **stop_dimmer()** This function will stop the especified dimmer immediately. The GPIO will be forced logic level LOW and can only be changed with *start_dimmer()*. It returns ESP_OK.

```c
esp_err_t set_dutty(dimmer_t *dimmer, uint16_t dutty);
```
- **set_dutty()** This function will update the dutty cicle on the specified dimmer, it will take a integer from [0 - 1000] and convert it into apropiate value to match the inverted logic then will update the dimmer and the output of PWM. It returns ESP_OK.

```c
esp_err_t set_power(dimmer_t *dimmer, double power);
```
- **set_power()** This function will update the dutty cicle on the specified dimmer using the power value witch is a value in the range [0 - 1]. Since AC is a sin wave there is no linear relation between time and power, so to get 10% of the wave we need to calculate the area under the curve of voltage x time. Note that there might be floating point fluctuation errors, if you need precise control consider use *set_dutty()*. It returns ESP_OK.

```c
float get_power(dimmer_t *dimmer);
```
- *get_power()** This function will do the oposite of set_power, looking at the provided dimmet_t, it will calculate the power in range [0 - 1] based in its dutty cycle. Note that there might be errors due to floating point fluctuation.
It returns a float corresponding to the calculated power.

```c
esp_err_t delete_dimmer( dimmer_t *dimmer);

```
- *delete_dimmer* In case you ever need to delete a dimmer this function will do it. (Since I couldn´t find the appropiate way to destroy all sctructures used to control PWM I disabled what I could and set to NULL what I couldn´t) It returns ESP_OK

### Task control

```c
typedef struct task_dimmer
{
    uint8_t        gen_gpio;
    uint8_t        sync_gpio;
    uint16_t       dutty;
    TaskHandle_t   task;
} task_dimmer_t;
```
- *task_dimmer_t* Is the struct of the dimmer intended to use with FreeRTOS task, all its values are initialised by *create_task_dimmer()*, and like dimmer_t the dutty cycle value can be accessed directilly.

```c
task_dimmer_t create_task_dimmer( uint8_t gen_gpio, uint8_t sync_gpio );
```
- *create_task_dimmer()* This function will initialize the dimmer, it creates a separeted task using FreeRTOS that will be running in paralel and using extra memory to do this (Note that it will create a new task for every dimmer) once the task is created it will start outputting a PWM with 0 dutty cycle by default ( witch is OFF as the circuit will not allow any current), then it will wait for updates via task notification, use *set_task_dimmer_dutty()* or *set_task_dimmer_power()* for control output.

```c
esp_err_t set_task_dimmer_dutty( task_dimmer_t* dimmer, uint16_t dutty );
```
- *set_task_dimmer_dutty()* This function will update the dutty cicle on the specified dimmer, it will take a integer from [0 - 1000] and convert it into apropiate value to match the inverted logic then will update the dimmer and the output of PWM. It returns ESP_OK.

```c
esp_err_t set_task_dimmer_power( task_dimmer_t* dimmer, double power );

```
- **set_task_dimmer_power()** This function will update the dutty cicle on the specified dimmer using the power value witch is a value in the range [0 - 1]. Since AC is a sin wave there is no linear relation between time and power, so to get 10% of the wave we need to calculate the area under the curve of voltage x time. Note that there might be floating point fluctuation errors, if you need precise control consider use *set_dutty()*. It returns ESP_OK.

```c
float get_task_dimmer_power(task_dimmer_t* dimmer);
```
- **get_task_dimmer_power()** This function will do the oposite of set_power, looking at the provided dimmet_t, it will calculate the power in range [0 - 1] based in its dutty cycle. Note that there might be errors due to floating point fluctuation.
It returns a float corresponding to the calculated power.

```c
esp_err_t delete_task_dimmer( task_dimmer_t* dimmer );
```
- **delete_task_dimmer()** In case you ever need to delete a dimmer this function will do it by terminating the associated task. Every thing in there will be deleted from memory, hence freeing it. It returns ESP_OK.