#include <pebble.h>

#define HAND_LENGTH 111
#define LAYER_WIDTH 26
#define INVERT_LAYER_COUNT 8
#define POINT_COUNT 7

static Window *window;

static Layer* second_layer;
static Layer* minute_layer;
static Layer* hour_layer;
static InverterLayer* invert_layers[INVERT_LAYER_COUNT];

static int32_t second_angle;
static int32_t minute_angle;
static int32_t hour_angle;

static GPath *hour_path = NULL;
static GPathInfo hour_info = {
  .num_points = POINT_COUNT,
  .points = (GPoint[POINT_COUNT]) {}
};

static GPath *minute_path = NULL;
static GPathInfo minute_info = {
  .num_points = POINT_COUNT,
  .points = (GPoint[POINT_COUNT]) {}
};

static GPath *second_path = NULL;
static GPathInfo second_info = {
  .num_points = POINT_COUNT,
  .points = (GPoint[POINT_COUNT]) {}
};

static void update_proc(Layer *layer, GContext *ctx, GPath *path, GPathInfo info, int32_t angle) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = {bounds.size.w / 2 , bounds.size.h / 2};
  GPoint edge = {center.x + 1, center.y + 1};

  GPoint hand;
  hand.y = (-cos_lookup(angle) * HAND_LENGTH / TRIG_MAX_RATIO) + center.y;
  hand.x = (sin_lookup(angle) * HAND_LENGTH / TRIG_MAX_RATIO) + center.x;
  int i = 0;
  info.points[i++] = GPoint(center.x, center.y);
  if (angle > 0) {
    info.points[i++] = GPoint(center.x, center.y - edge.y);

    if (angle >= TRIG_MAX_ANGLE / 8) {
      info.points[i++] = GPoint(center.x + edge.x, center.y - edge.y);
    }

    if (angle >= TRIG_MAX_ANGLE * 3 / 8) {
      info.points[i++] = GPoint(center.x + edge.x, center.y + edge.y);
    }

    if (angle >= TRIG_MAX_ANGLE * 5 / 8) {
      info.points[i++] = GPoint(center.x - edge.x, center.y + edge.y);
    }

    if (angle >= TRIG_MAX_ANGLE * 7 / 8) {
      info.points[i++] = GPoint(center.x - edge.x, center.y - edge.y);
    }

    info.points[i++] = hand;
  }

  while (i < POINT_COUNT) {
    info.points[i++] = GPoint(center.x, center.y);
  }

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_fill_color(ctx, GColorBlack);
  gpath_draw_filled(ctx, path); 
}

static void second_update_proc(Layer *layer, GContext *ctx) {
  update_proc(layer, ctx, second_path, second_info, second_angle);
}

static void minute_update_proc(Layer *layer, GContext *ctx) {
  update_proc(layer, ctx, minute_path, minute_info, minute_angle);
}

static void hour_update_proc(Layer *layer, GContext *ctx) {
  update_proc(layer, ctx, hour_path, hour_info, hour_angle);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  if (units_changed & SECOND_UNIT) {
    second_angle = TRIG_MAX_ANGLE * tick_time->tm_sec / 60;
    layer_mark_dirty(second_layer); 
  }
 
  if (units_changed & MINUTE_UNIT) {
    minute_angle = TRIG_MAX_ANGLE * tick_time->tm_min / 60;
    layer_mark_dirty(minute_layer);
  }

  if (units_changed & HOUR_UNIT) {
    if (clock_is_24h_style()) {
      hour_angle = TRIG_MAX_ANGLE * tick_time->tm_hour / 24;
    } else {
      hour_angle = TRIG_MAX_ANGLE * (tick_time->tm_hour % 12) / 12;
    }
    layer_mark_dirty(hour_layer); 
  }
}

static int generate_inverter_layers(int index, Layer* layer) {
  GRect bounds = layer_get_bounds(layer);
  InverterLayer* inv_lay;

  inv_lay = inverter_layer_create((GRect) {
    .origin = {0, 0},
    .size = {1, bounds.size.h}});
  layer_add_child(layer, inverter_layer_get_layer(inv_lay));
  invert_layers[index++] = inv_lay;

  inv_lay = inverter_layer_create((GRect) {
    .origin = {bounds.size.w - 1, 0},
    .size = {1, bounds.size.h}});
  layer_add_child(layer, inverter_layer_get_layer(inv_lay));
  invert_layers[index++] = inv_lay;

  inv_lay = inverter_layer_create((GRect) {
    .origin = {0, 0},
    .size = {bounds.size.w, 1}});
  layer_add_child(layer, inverter_layer_get_layer(inv_lay));
  invert_layers[index++] = inv_lay;

  inv_lay = inverter_layer_create((GRect) {
    .origin = {0, bounds.size.h - 1},
    .size = {bounds.size.w, 1}});
  layer_add_child(layer, inverter_layer_get_layer(inv_lay));
  invert_layers[index++] = inv_lay;

  return index;
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  second_layer = layer_create((GRect) {
    .origin = { 0, 0 },
    .size = {bounds.size.w, bounds.size.h}});
  second_path = gpath_create(&second_info);
  layer_set_update_proc(second_layer, second_update_proc);
  layer_add_child(window_layer, second_layer);

  minute_layer = layer_create((GRect) {
    .origin = {LAYER_WIDTH, LAYER_WIDTH},
    .size = {bounds.size.w - LAYER_WIDTH * 2, bounds.size.h - LAYER_WIDTH * 2}});
  minute_path = gpath_create(&minute_info);
  layer_set_update_proc(minute_layer, minute_update_proc);
  layer_add_child(second_layer, minute_layer);
  int index = 0;
  index = generate_inverter_layers(index, minute_layer);

  hour_layer = layer_create((GRect) {
    .origin = {LAYER_WIDTH, LAYER_WIDTH},
    .size = {bounds.size.w - LAYER_WIDTH * 4, bounds.size.h - LAYER_WIDTH * 4}});
  hour_path = gpath_create(&hour_info);
  layer_set_update_proc(hour_layer, hour_update_proc);
  layer_add_child(minute_layer, hour_layer);
  index = generate_inverter_layers(index, hour_layer);

  time_t t = time(NULL);
  tick_handler(localtime(&t), SECOND_UNIT | HOUR_UNIT | MINUTE_UNIT);
}

static void window_unload(Window *window) {
  layer_destroy(second_layer);
  layer_destroy(minute_layer);
  layer_destroy(hour_layer);
  gpath_destroy(second_path);
  gpath_destroy(hour_path);
  gpath_destroy(minute_path);
  for (int i = 0; i < INVERT_LAYER_COUNT; i++) {
    inverter_layer_destroy(invert_layers[i]);
  }
}

static void init(void) {
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  // set up tick handler
  tick_timer_service_subscribe(SECOND_UNIT | HOUR_UNIT | MINUTE_UNIT, (TickHandler) tick_handler);
  window_stack_push(window, true);
}

static void deinit(void) {
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
