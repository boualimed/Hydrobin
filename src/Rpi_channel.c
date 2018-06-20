/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
 /* POSIX file system api*/
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*THIRD party libs*/

#include "channel-io.h"       //current def_file
#include "eh.h"              // events
#include "gpio.h"           // GPIO_1-8, expanser mcp23008
#include "lck.h"           // threading process definition
#include "log.h"          //logging info data
#include "mcp23008.h" 
#include "safearray.h"  //safe array threading defs
#include "sthread.h"   //single thread defs
#include "w1.h"       //one wire libs

//macros defs
#define GPIO_MINDELAY_USEC 200000 
#ifdef __SINGLE_THREAD
#define W1_TEMP_MINDELAY_SEC 120
#else
#define W1_TEMP_MINDELAY_SEC 30
#endif
#define MCP23008_MINDELAY_SEC 1


typedef struct {
#ifdef __SINGLE_THREAD
  struct timeval close_time;
#endif
// the last time_value structure
  struct timeval last_tv; 
  /* 
  struct timeval { -- "proto.h"--}
  */
  char value;
  void *lck;
} TChannelGpioPortValue;//(time channle GPIO port value)


//  one wire temperature sensor
typedef struct {
  struct timeval last_tv;
  double temp;
  double humidity;
  void *lck;
} TChannelW1TempValue;

//PWM struct
typedef struct {
  int color;
  char color_brightness;
  char brightness;
  void *lck;
} TChannelRGBWValue;

// mcp23008 struct membs
typedef struct {
#ifdef __SINGLE_THREAD
  struct timeval close_time;
#endif
  struct timeval last_tv;
  char addr; //address of i2c expanser (1-5) there are up to 8 mcp23xx_//default 0x20 
  char reset; 
  char dir; //direction I/O
  char port; //1-8 | 0-7
  char active; //state
  void *lck;
  TChannelGpioPortValue gpio;
} TChannelMCP23008;

// device type struct 
typedef struct {
  unsigned char number;
  int type;
  int driver;
  unsigned char gpio1;
  unsigned char gpio2;
  unsigned char bistable;
  char *w1;

  TChannelGpioPortValue gpio1_value;
  TChannelGpioPortValue gpio2_value;
  TChannelW1TempValue w1_value;
  TChannelRGBWValue rgbw_value;
  TChannelMCP23008 mcp23008;
} TDeviceChannel;
// struct of threads arguments
typedef struct {
  TDeviceChannel *channel;
  void *sthread; //single thread
  unsigned char port1;
  unsigned char port2;
  unsigned char bistable; //button
  char hi; //input high
  int time_ms;
  char raise; //raising edge
} TGpioThreadItem;

// channel type input/output

typedef struct {
  char initialized;

  TDeviceChannel *channels;
  int channel_count;

  void *cb_lck; //lock callback
  _func_channelio_valuechanged on_valuechanged;
  void *on_valuechanged_user_data;

  void *wl_lck;
  TGpioPort *watch_list;
  int watch_list_count;

#ifndef __SINGLE_THREAD
  void *gpio_thread_arr;
  void *w1_sthread;
  void *mcp_sthread;
#endif
} TChannelIo;

//(null pointer of TChannel type)

TChannelIo *cio = NULL;

#ifndef __SINGLE_THREAD // RPI has 4 core, sometime we halt the others
void channelio_w1_execute(void *user_data, void *sthread);
void channelio_gpio_monitor_execute(void *user_data, void *sthread);
void channelio_mcp23008_execute(void *user_data, void *sthread);
#endif

// 
char channelio_gpio_in(TDeviceChannel *channel, char port12) {
  if (channel->type == SUPLA_CHANNELTYPE_SENSORNO ||
      channel->type == SUPLA_CHANNELTYPE_SENSORNC ||
      ((channel->type == SUPLA_CHANNELTYPE_RELAYHFD4 ||
        channel->type == SUPLA_CHANNELTYPE_RELAYG5LA1A ||
        channel->type == SUPLA_CHANNELTYPE_RELAYG5LA1A) &&
       port12 == 2 && channel->bistable == 1))
    return 1;

  return 0;
}
//

char channelio_init(void) {
  cio = malloc(sizeof(TChannelIo));
  if (cio == NULL) return 0;

  memset(cio, 0, sizeof(TChannelIo));
  cio->cb_lck = lck_init();
  cio->wl_lck = lck_init();

#ifndef __SINGLE_THREAD
  cio->gpio_thread_arr = safe_array_init();
#endif

  return 1;
}






