/**
 * @file menu_manager.h
 * @brief This file defines structures and functions related to the Menu System.
 */

#ifndef __MENU_MANAGER_H__
#define __MENU_MANAGER_H__
#pragma once
#include "freertos/idf_additions.h"
#include "sdkconfig.h"
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define END_MENU_FUNCTION end_menuFunction()
#define SET_QUICK_FUNCTION setQuick_menuFunction()

extern TaskHandle_t tMenuFunction;
extern QueueHandle_t qCommands;

/**
 * Type with  possibles of action in menu system.
 *
 */
typedef enum {
  NAVIGATE_UP,
  /**< Action up (index++). */
  NAVIGATE_DOWN,
  /**< Action DOWN (index--). */
  NAVIGATE_SELECT,
  /**< Action SELECT (depth++). */
  NAVIGATE_BACK,
  /**< Action BACH (depth--). */
  NAVIGATE_NOTHING,
  /**< Nothing. */
} Navigate_t;

/**
 * Menu System is based in nodes all menus are nodes. Submenus are nodes with
 * array of more submenus and function (leafs) are nodes with function.
 *
 */
typedef struct menu_node {
  char *label;
  /**< Title of node. */
  struct menu_node *submenus;
  /**< Point of array submenus or one submenu. */
  size_t num_options;
  /**< number of option on submenu. */
  void (*function)(void *args);
  /**< function that defined this node as leaf. */
} menu_node_t;

/**
 * Struct for save location and index.
 *
 */
typedef struct {
  menu_node_t *current_menu;
  /**< Point if current menu. */
  uint8_t current_index;
  /**< current Index of selected option. */
} menu_path_t;

/**
 * This struct is all essential args that menu_init will need.
 */
typedef struct {
  menu_node_t root;
  /**< Menu_node_t: origin of Menu System. */
  void (*display)(menu_path_t *current_path);
  /**< Function that receive menu_path_t and index for display current
   * selection. */
  bool loop;
  /**< Loop menu. */
} menu_config_t;

/**
 * Start menu system that receive generic input function and generic
 * display function.
 *
 * @param params The struct that there are args.
 */
void menu_init(void *params);

/**
 * @brief Use this function all option menus when finish
 */
void exitFunction(void);

/**
 * @brief Exec especific funtioon
 *
 * @param Function addres of function
 */
void execFunction(void (*function)(void *args));

/**
 * @brief Use this function when your function is a wuick function before
 * end_menuFunction()
 */
void setQuick_menuFunction(void);

#ifdef __cplusplus
}
#endif

#endif //__MENU_MANAGER_H__
