#include <pebble.h>

#define COLORS       true
#define ANTIALIASING true

#define HAND_MARGIN  25
#define FINAL_RADIUS 80

#define ANIMATION_DURATION 500
#define ANIMATION_DELAY    600

typedef struct {
  int hours;
  int minutes;
} Time;

static Window *s_main_window;
static Layer *s_canvas_layer;

static GPoint s_center;
static Time s_last_time, s_anim_time;
static int s_radius = 0, s_anim_hours_60 = 0, s_color_channels[3];
static bool s_animating = false;

static bool s_circle = true;
static bool s_hour = false;
static bool s_blue = false;
static bool s_bat = false;

static GBitmap *bt_bitmap;
static BitmapLayer *bt_layer;

static TextLayer *s_day_label, *s_num_label, *s_hour_label, *s_bat_label;
static char s_num_buffer[4], s_day_buffer[6], s_hour_buffer[32], s_bat_buffer[32];

// Possible messages received from the config page
enum {
  DUMMY = 0x0,
  KEY_CIRCLE = 0x1,
  KEY_HOUR = 0x2,
  KEY_BLUE = 0x3,
  KEY_BAT = 0x4
};

/*
static void handle_battery(BatteryChargeState charge_state) {
  static char battery_text[] = "100% charged";

  if (charge_state.is_charging) {
    snprintf(battery_text, sizeof(battery_text), "en carga");
  } else {
    snprintf(battery_text, sizeof(battery_text), "%d%%", charge_state.charge_percent);
  }
  text_layer_set_text(s_battery_layer, battery_text);
}

*/

/*************************** ConfigUpdate **************************/


void in_received_handler(DictionaryIterator *received, void *context) {
 
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Recibido setting from pebble");
  
   // Get the first pair
  Tuple *t = dict_read_first(received);

  // Process all pairs present
  while(t != NULL) {
    // Process this pair's key
    switch (t->key) {
      case KEY_CIRCLE:
        APP_LOG(APP_LOG_LEVEL_INFO, "KEY_CIRCLE received with value %d", (int)t->value->int32);
        
        if ((int)t->value->int32 == 0) {
          persist_write_bool(KEY_CIRCLE, true);
          s_circle = true;
          APP_LOG(APP_LOG_LEVEL_DEBUG, "0 = circle true, not disabled");
        } else {
          persist_write_bool(KEY_CIRCLE, false);
          s_circle = false;
          APP_LOG(APP_LOG_LEVEL_DEBUG, "1 = circle false, enabled");
        }
      
      break;
      
      case KEY_HOUR:
        APP_LOG(APP_LOG_LEVEL_INFO, "KEY_HOUR received with value %d", (int)t->value->int32);
        
        if ((int)t->value->int32 == 0) {
          persist_write_bool(KEY_HOUR, false);
          s_hour = false;
          APP_LOG(APP_LOG_LEVEL_DEBUG, "0 = hour false");
        } else {
          persist_write_bool(KEY_HOUR, true);
          s_hour = true;
          APP_LOG(APP_LOG_LEVEL_DEBUG, "1 = hour true");
        }
      
      break;
      
      case KEY_BLUE:
        APP_LOG(APP_LOG_LEVEL_INFO, "KEY_BLUE received with value %d", (int)t->value->int32);
        
        if ((int)t->value->int32 == 0) {
          persist_write_bool(KEY_BLUE, false);
          s_blue = false;
        } else {
          persist_write_bool(KEY_BLUE, true);
          s_blue = true;
        }
      
      break;
      
      case KEY_BAT:
        APP_LOG(APP_LOG_LEVEL_INFO, "KEY_BAT received with value %d", (int)t->value->int32);
        
        if ((int)t->value->int32 == 0) {
          persist_write_bool(KEY_BAT, false);
          s_bat = false;
        } else {
          persist_write_bool(KEY_BAT, true);
          s_bat = true;
        }
      
      break;
      
      
    }

    // Get next pair, if any
    t = dict_read_next(received);
  }
    
    //actualizar aquí el render
      // Redraw
  if(s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }

}

static void in_dropped_handler(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Dropped!");
}


/*************************** BT **************************/

static void handle_bluetooth(bool connected) {
  
  if (connected) {
    layer_set_hidden(bitmap_layer_get_layer(bt_layer), false);
    //text_layer_set_text(s_connection_layer, "OK");
  } else {
    layer_set_hidden(bitmap_layer_get_layer(bt_layer), true);
    //text_layer_set_text(s_connection_layer, "NO");
    //vibes_enqueue_custom_pattern(pat);
  }
}

/*************************** DateUpdateImplementation **************************/
#include <ctype.h>
  
char *upcase(char *str)
{
    char *s = str;

    while (*s)
    {
        *s++ = toupper((int)*s);
    }

    return str;
}

static void date_update_proc() {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  strftime(s_day_buffer, sizeof(s_day_buffer), "%a", t);
  upcase(s_day_buffer);
  text_layer_set_text(s_day_label, s_day_buffer);

  strftime(s_num_buffer, sizeof(s_num_buffer), "%d", t);
  text_layer_set_text(s_num_label, s_num_buffer);
  
  if (s_hour) {
    strftime(s_hour_buffer, sizeof(s_hour_buffer), "%H:%M", t);
    text_layer_set_text(s_hour_label, s_hour_buffer);
    layer_set_hidden(text_layer_get_layer(s_hour_label), false);
   } else {
    layer_set_hidden(text_layer_get_layer(s_hour_label), true);
  }
}


/*************************** AnimationImplementation **************************/

static void animation_started(Animation *anim, void *context) {
  s_animating = true;
}

static void animation_stopped(Animation *anim, bool stopped, void *context) {
  s_animating = false;
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

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  // Store time
  s_last_time.hours = tick_time->tm_hour;
  s_last_time.hours -= (s_last_time.hours > 12) ? 12 : 0;
  s_last_time.minutes = tick_time->tm_min;

  for(int i = 0; i < 3; i++) {
    s_color_channels[i] = 0;
  }

  // Redraw
  if(s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static int hours_to_minutes(int hours_out_of_12) {
  return (int)(float)(((float)hours_out_of_12 / 12.0F) * 60.0F);
}

static void update_proc(Layer *layer, GContext *ctx) {

  //date update
  date_update_proc();
  
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 8);

  graphics_context_set_antialiased(ctx, ANTIALIASING);

  // Draw outline
  if (!s_circle) {
    graphics_draw_circle(ctx, s_center, s_radius);
  } else {
    // Draw Point
    graphics_draw_circle(ctx, GPoint(72, 12), 1);
  }
  // Don't use current time while animating
  Time mode_time = (s_animating) ? s_anim_time : s_last_time;

  // Adjust for minutes through the hour
  float minute_angle = TRIG_MAX_ANGLE * mode_time.minutes / 60;
  float hour_angle;
  if(s_animating) {
    // Hours out of 60 for smoothness
    hour_angle = TRIG_MAX_ANGLE * mode_time.hours / 60;
  } else {
    hour_angle = TRIG_MAX_ANGLE * mode_time.hours / 12;
  }
  hour_angle += (minute_angle / TRIG_MAX_ANGLE) * (TRIG_MAX_ANGLE / 12);

  // Plot hands
  GPoint minute_hand = (GPoint) {
    .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * (int32_t)(s_radius - HAND_MARGIN) / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * (int32_t)(s_radius - HAND_MARGIN) / TRIG_MAX_RATIO) + s_center.y,
  };
  GPoint hour_hand = (GPoint) {
    .x = (int16_t)(sin_lookup(hour_angle) * (int32_t)(s_radius - (1.5 * HAND_MARGIN)) / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(hour_angle) * (int32_t)(s_radius - (1.5 * HAND_MARGIN)) / TRIG_MAX_RATIO) + s_center.y,
  };

  // Draw hands with positive length only
  
  if(s_radius > HAND_MARGIN) {
      
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_draw_line(ctx, s_center, minute_hand);
  }
  if(s_radius > 2 * HAND_MARGIN) {
    
    if (!s_blue) {
      graphics_context_set_stroke_color(ctx, GColorRed);
    } else {
      graphics_context_set_stroke_color(ctx, GColorVividCerulean);
    }
    
    graphics_draw_line(ctx, s_center, hour_hand);
  } 
  
  
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);

   //date
  
  s_day_label = text_layer_create(GRect(96, 73, 27, 20));
  text_layer_set_text(s_day_label, s_day_buffer);
  text_layer_set_background_color(s_day_label, GColorBlack);
  text_layer_set_text_color(s_day_label, GColorWhite);
  text_layer_set_font(s_day_label, fonts_get_system_font(FONT_KEY_GOTHIC_18));

  layer_add_child(window_layer, text_layer_get_layer(s_day_label));

  s_num_label = text_layer_create(GRect(123, 73, 18, 20));
  text_layer_set_text(s_num_label, s_num_buffer);
  text_layer_set_background_color(s_num_label, GColorBlack);
  text_layer_set_text_color(s_num_label, GColorWhite);
  text_layer_set_font(s_num_label, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));

  layer_add_child(window_layer, text_layer_get_layer(s_num_label));
  
  s_hour_label = text_layer_create(GRect(52, 127, 40, 20));
  text_layer_set_text(s_hour_label, s_hour_buffer);
  text_layer_set_background_color(s_hour_label, GColorBlack);
  text_layer_set_text_color(s_hour_label, GColorWhite);
  text_layer_set_font(s_hour_label, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_hour_label, GTextAlignmentCenter);

  layer_add_child(window_layer, text_layer_get_layer(s_hour_label));
  
  //BT
  
  bt_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BT);

  bt_layer = bitmap_layer_create(GRect(64, 115, 16, 16));
  bitmap_layer_set_bitmap(bt_layer, bt_bitmap);
#ifdef PBL_PLATFORM_APLITE
  bitmap_layer_set_compositing_mode(bt_layer, GCompOpAssign);
#elif PBL_PLATFORM_BASALT
  bitmap_layer_set_compositing_mode(bt_layer, GCompOpSet);
#endif
  layer_add_child(window_layer, bitmap_layer_get_layer(bt_layer));
  
  handle_bluetooth(bluetooth_connection_service_peek());
  bluetooth_connection_service_subscribe(handle_bluetooth);
  
  s_center = grect_center_point(&window_bounds);

  s_canvas_layer = layer_create(window_bounds);
  
  layer_set_update_proc(s_canvas_layer, update_proc);

  layer_add_child(window_layer, s_canvas_layer);
  
     
}

static void window_unload(Window *window) {
  
  text_layer_destroy(s_day_label);
  text_layer_destroy(s_num_label);
  text_layer_destroy(s_hour_label);
  bluetooth_connection_service_unsubscribe();
  bitmap_layer_destroy(bt_layer);
  layer_destroy(s_canvas_layer);
}

/*********************************** App **************************************/

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

static void init() {
  
  if (persist_exists(KEY_BAT)) {
    s_bat = persist_read_bool(KEY_BAT);
  } else {
    persist_write_bool(KEY_BAT, false);
    s_bat = false;
  }

  if (persist_exists(KEY_BLUE)) {
    s_blue = persist_read_bool(KEY_BLUE);
  } else {
    persist_write_bool(KEY_BLUE, false);
    s_blue = false;
  }
  
  if (persist_exists(KEY_CIRCLE)) {
    s_circle = persist_read_bool(KEY_CIRCLE);
  } else {
    persist_write_bool(KEY_CIRCLE, true);
    s_circle = true;
  }
  
  if (persist_exists(KEY_HOUR)) {
    s_hour = persist_read_bool(KEY_HOUR);
  } else {
    persist_write_bool(KEY_HOUR, false);
    s_hour = false;
  }
  
  srand(time(NULL));

  time_t t = time(NULL);
  struct tm *time_now = localtime(&t);
  tick_handler(time_now, MINUTE_UNIT);

  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_main_window, true);
  
  window_set_background_color(s_main_window, GColorBlack);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  // Prepare animations
  AnimationImplementation radius_impl = {
    .update = radius_update
  };
  animate(ANIMATION_DURATION, ANIMATION_DELAY, &radius_impl, false);

  AnimationImplementation hands_impl = {
    .update = hands_update
  };
  animate(2 * ANIMATION_DURATION, ANIMATION_DELAY, &hands_impl, true);
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Init done....");
  app_message_register_inbox_received(in_received_handler);
  app_message_register_inbox_dropped(in_dropped_handler); 
  // Open AppMessage
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
  
}

static void deinit() {
  window_destroy(s_main_window);
}

int main() {
  init();
  app_event_loop();
  deinit();
}
