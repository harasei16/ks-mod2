#include <pebble.h>
#include "ks-mod-2.h"

#define COLORS       true
#define ANTIALIASING true

#define HAND_MARGIN  14
#define FINAL_RADIUS 65
#define INDICATOR_SIZE 6
#define INDICATOR_OFFSET 4
#define SMALL_INDICATOR_SIZE 2

#define ANIMATION_DURATION 500
#define ANIMATION_DELAY    600

typedef struct {
 int hours;
 int minutes;
 int seconds;
} Time;

static Window *s_main_window;
static Layer *s_canvas_layer;
static TextLayer *s_date_layer;

static GPoint s_center;
static Time s_last_time, s_anim_time;
static char s_date_buffer[] = "00";
static int s_radius = 0, s_color_channels[3];
static bool s_animating = false;

static bool show_second_hand = false;
static int show_second_until;

static int battery_level;
static bool battery_charging;

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  // Store time
  s_last_time.hours = tick_time->tm_hour;
  s_last_time.hours -= (s_last_time.hours > 12) ? 12 : 0;
  s_last_time.minutes = tick_time->tm_min;
  s_last_time.seconds = tick_time->tm_sec;
  snprintf(s_date_buffer, sizeof("00"),"%d" , tick_time->tm_mday);
  
  //Dummy time set for debugging
  /*
  s_last_time.hours = 9;
  s_last_time.minutes = 15;
  strcpy(s_date_buffer,"5");
  */
  
  //Change color gradually
  if (s_last_time.seconds % 10 == 0){
    int i=rand() % 3;
    switch (s_color_channels[i]){
      case 0x00:
      s_color_channels[i] = 0x55;
      break;
      case 0Xff:
      s_color_channels[i] = 0xaa;
      break;
      case 0x55:
      s_color_channels[i] = (rand()% 2 ==0) ? 0x00 : 0xaa;
      break;
      case 0xaa:
      s_color_channels[i] = (rand()% 2 ==0) ? 0x55 : 0xff;
    }
  }

  // Redraw
  if(s_canvas_layer) {
    //Stop showing second hand after 55 sec
    if (show_second_hand  && (show_second_until == s_last_time.seconds)){
      show_second_hand = false ; 
      if(s_canvas_layer){
        layer_mark_dirty(s_canvas_layer);
      }
      return;
    }
    //redraw every 1 or 10 sec depending whether second hand is displayed 
    if ( show_second_hand || (s_last_time.seconds % 10 == 0))
    layer_mark_dirty(s_canvas_layer);
  }
}

//Show second hand for 55 sec if accelaration (tap) detected
static void AccelTap_Handler (AccelAxisType axis, int32_t direction){
  APP_LOG(APP_LOG_LEVEL_DEBUG, "AccelTap_Handler called");
  show_second_hand = true;
  time_t t = time(NULL);
  struct tm *tx = localtime(&t);
  
  show_second_until = (tx -> tm_sec) - 5;
  if (show_second_until < 0){
      show_second_until += 60;
  }
  if(s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

// Get battery statud for displaying 
static void handle_battery(BatteryChargeState charge_state) {
  battery_level= charge_state.charge_percent;
  battery_charging=charge_state.is_charging;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "handle_battery called");
  
  //Simulation of AccelTap_Handler
  /*
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Simulated AccelTap_Handler called");
  show_second_hand = true;
  time_t t = time(NULL);
  struct tm *tx = localtime(&t);
  
  show_second_until = (tx -> tm_sec) - 5;
  if (show_second_until < 0){
      show_second_until += 60;
  }
  */
  
  if(s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

/*************************** AnimationImplementation **************************/

static void animation_started(Animation *anim, void *context) {
  s_animating = true;
}

static void animation_stopped(Animation *anim, bool stopped, void *context) {
  s_animating = false;
  text_layer_set_text(s_date_layer, s_date_buffer);
}

static void animate(int duration, int delay, AnimationImplementation *implementation, bool handlers) {
  Animation *anim = animation_create();
  animation_set_duration(anim, duration);
  animation_set_delay(anim, delay);
  animation_set_curve(anim, AnimationCurveEaseInOut);
  animation_set_implementation(anim, implementation);
  if(handlers) {
    animation_set_handlers(anim, (AnimationHandlers) {
      .started = animation_started,
      .stopped = animation_stopped
    }, NULL);
  }
  animation_schedule(anim);
}

/************************************ UI **************************************/

static int hours_to_minutes(int hours_out_of_12) {
  return (int)(float)(((float)hours_out_of_12 / 12.0F) * 60.0F);
}

static void update_proc(Layer *layer, GContext *ctx) {
 //APP_LOG(APP_LOG_LEVEL_DEBUG, "update_proc");
  // Color background?
  if(COLORS) {
    graphics_context_set_fill_color(ctx, GColorFromRGB(s_color_channels[0], s_color_channels[1], s_color_channels[2]));
    graphics_fill_rect(ctx, GRect(0, 0, 144, 168), 0, GCornerNone);
  }

  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 4);

  graphics_context_set_antialiased(ctx, ANTIALIASING);

  // White clockface
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, s_center, s_radius);

  // Draw outline
  //graphics_draw_circle(ctx, s_center, s_radius);

  // Don't use current time while animating
  Time mode_time = (s_animating) ? s_anim_time : s_last_time;

   // Adjust for seconds through the minutes
  float second_angle = TRIG_MAX_ANGLE * mode_time.seconds / 60;
    
  // Adjust for minutes through the hour
  float minute_angle = TRIG_MAX_ANGLE * mode_time.minutes / 60;
  minute_angle += (second_angle / 60);
 
  float hour_angle;
  if(s_animating) {
    // Hours out of 60 for smoothness
    hour_angle = TRIG_MAX_ANGLE * mode_time.hours / 60;
  } else {
    hour_angle = TRIG_MAX_ANGLE * mode_time.hours / 12;
  }
  hour_angle += (minute_angle / 12);
 
  // Plot hands
  GPoint minute_hand = (GPoint) {
    .x = (int16_t)(sin_lookup(minute_angle) * (int32_t)(s_radius - HAND_MARGIN) / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(minute_angle) * (int32_t)(s_radius - HAND_MARGIN) / TRIG_MAX_RATIO) + s_center.y,
  };
  GPoint hour_hand = (GPoint) {
    .x = (int16_t)(sin_lookup(hour_angle) * (int32_t)(s_radius - (2 * HAND_MARGIN)) / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(hour_angle) * (int32_t)(s_radius - (2 * HAND_MARGIN)) / TRIG_MAX_RATIO) + s_center.y,
  };
  GPoint second_hand;
  if( show_second_hand) {
    second_hand = (GPoint) {
      .x = (int16_t)(sin_lookup(second_angle) * (int32_t)(s_radius - (HAND_MARGIN)) / TRIG_MAX_RATIO) + s_center.x,
      .y = (int16_t)(-cos_lookup(second_angle) * (int32_t)(s_radius - (HAND_MARGIN)) / TRIG_MAX_RATIO) + s_center.y,
    };
  }
  
  // Draw hands with positive length only
  if(s_radius > 2 * HAND_MARGIN) {
    graphics_draw_line(ctx, s_center, hour_hand);
  } 
  if(s_radius > HAND_MARGIN) {
    graphics_draw_line(ctx, s_center, minute_hand);
    if( ! (s_animating) && show_second_hand) {
      graphics_context_set_stroke_width(ctx, 2);
      graphics_draw_line(ctx, s_center, second_hand);
      graphics_context_set_stroke_width(ctx, 4);      
    }
  }
  
  // Draw indicators
  int16_t from_x, to_x, from_y, to_y;
  
  if (s_radius > INDICATOR_SIZE){
    for (int j=0; j<=11; j++){
      //color changes depending on battery status
      if (j <= battery_level / 10){
        if (battery_charging){
          graphics_context_set_stroke_color(ctx, GColorRed);
        }else{
          graphics_context_set_stroke_color(ctx, GColorBlack);
        }
      }else{
        graphics_context_set_stroke_color(ctx, GColorDarkGray);
      }
      
      float indicator_angle = TRIG_MAX_ANGLE * j / 12;
      from_x = (int16_t)( sin_lookup(indicator_angle) * (int32_t)(s_radius) / TRIG_MAX_RATIO) + s_center.x;
      from_y = (int16_t)(-cos_lookup(indicator_angle) * (int32_t)(s_radius) / TRIG_MAX_RATIO) + s_center.y;
      
      if (j % 3 == 0 ){
        //Large indicator
        to_x   = (int16_t)( sin_lookup(indicator_angle) * (int32_t)(s_radius - INDICATOR_SIZE) / TRIG_MAX_RATIO) + s_center.x;
        to_y   = (int16_t)(-cos_lookup(indicator_angle) * (int32_t)(s_radius - INDICATOR_SIZE) / TRIG_MAX_RATIO) + s_center.y;
      }else{
        //Small indicator
        to_x   = (int16_t)( sin_lookup(indicator_angle) * (int32_t)(s_radius - SMALL_INDICATOR_SIZE) / TRIG_MAX_RATIO) + s_center.x;
        to_y   = (int16_t)(-cos_lookup(indicator_angle) * (int32_t)(s_radius - SMALL_INDICATOR_SIZE) / TRIG_MAX_RATIO) + s_center.y;
      }
      graphics_draw_line(ctx, GPoint(from_x, from_y), GPoint(to_x,to_y));
    }
  }
  
  // Draw outline
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_circle(ctx, s_center, s_radius);
  
  //Text Layer
  //Change position according to the positions of the hands
  if ((mode_time.minutes>=10 && mode_time.minutes<=20) ||(mode_time.hours>=2 && mode_time.hours<=3)){
    if ((mode_time.minutes>=40 && mode_time.minutes<=50) ||(mode_time.hours>=8 && mode_time.hours<=9)){
      //Date at 6 o'clock
      text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
      layer_set_frame((Layer *)s_date_layer, GRect(s_center.x - 11, s_center.y + s_radius - INDICATOR_SIZE -31, 24, 26));
    }
    else{
      //Date at 9 o'clock
      text_layer_set_text_alignment(s_date_layer, GTextAlignmentLeft);
      layer_set_frame((Layer *)s_date_layer, GRect(s_center.x - s_radius + INDICATOR_SIZE + 8, s_center.y - 17, 24, 26));
    }
  } else{
    //Date at 3 o'clock
    text_layer_set_text_alignment(s_date_layer, GTextAlignmentRight);
    layer_set_frame((Layer *)s_date_layer, GRect(s_center.x + s_radius - INDICATOR_SIZE -31, s_center.y - 17, 24, 26));
  }
}

//Create text layer and graphic layer
static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);

  s_center = grect_center_point(&window_bounds);
  
  //Text Layer for Date
  s_date_layer = text_layer_create(GRect(s_center.x + 35, s_center.y - 16, 24, 26));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));

  //Graphic layer
  s_canvas_layer = layer_create(window_bounds);
  layer_set_update_proc(s_canvas_layer, update_proc);
  
  layer_add_child(s_canvas_layer, text_layer_get_layer(s_date_layer));
  layer_add_child(window_layer, s_canvas_layer);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
}

/*********************************** Animation **************************************/

static int anim_percentage(AnimationProgress dist_normalized, int max) {
  return (int)(float)(((float)dist_normalized / (float)ANIMATION_NORMALIZED_MAX) * (float)max);
}

static void radius_update(Animation *anim, AnimationProgress dist_normalized) {
  s_radius = anim_percentage(dist_normalized, FINAL_RADIUS);

  layer_mark_dirty(s_canvas_layer);
}

static void hands_update(Animation *anim, AnimationProgress dist_normalized) {
  s_anim_time.hours = anim_percentage(dist_normalized, hours_to_minutes(s_last_time.hours));
  s_anim_time.minutes = anim_percentage(dist_normalized, s_last_time.minutes);

  layer_mark_dirty(s_canvas_layer);
}
/*********************************************************************************/

static void init() {
  srand(time(NULL));
  
  //Set random color
  for(int i = 0; i < 3; i++) {
    s_color_channels[i] = rand() % 4 * 0x55 ;
  }
  
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  
  accel_tap_service_subscribe(AccelTap_Handler);

  battery_state_service_subscribe(handle_battery);
  handle_battery(battery_state_service_peek());
  
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_main_window, true);
  
  // Prepare animations
  AnimationImplementation radius_impl = {
    .update = radius_update
  };
  animate(ANIMATION_DURATION, ANIMATION_DELAY, &radius_impl, false);

  AnimationImplementation hands_impl = {
    .update = hands_update
  };
  animate(2 * ANIMATION_DURATION, ANIMATION_DELAY, &hands_impl, true);
}

static void deinit() {
  window_destroy(s_main_window);
  tick_timer_service_unsubscribe();
  accel_tap_service_unsubscribe();
  battery_state_service_unsubscribe();
}

int main() {
  init();
  app_event_loop();
  deinit();
}