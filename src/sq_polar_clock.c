// Square Polar Clock Watch Face
// Copyright (C) 2015 Neil Parsons 
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
#include <pebble.h>

#define HAND_LENGTH 111
#define LAYER_WIDTH 26
#define INVERT_LAYER_COUNT (4 * (TIME_COUNT - 1))
#define POINT_COUNT 7

// Determines the order of layers, from outer to inner
enum time_sections {
	HOUR,
	MINUTE,
	SECOND,
	TIME_COUNT
};

static Window *window;

static Layer* time_layers[TIME_COUNT];
static InverterLayer* invert_layers[INVERT_LAYER_COUNT];

static int32_t time_angles[TIME_COUNT];

static GPath* time_paths[TIME_COUNT];
static GPathInfo time_infos[TIME_COUNT] = {
	{.num_points = POINT_COUNT, .points = (GPoint[POINT_COUNT]) {}},
	{.num_points = POINT_COUNT, .points = (GPoint[POINT_COUNT]) {}},
	{.num_points = POINT_COUNT, .points = (GPoint[POINT_COUNT]) {}}
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
	update_proc(layer, ctx, time_paths[SECOND], time_infos[SECOND], time_angles[SECOND]);
}

static void minute_update_proc(Layer *layer, GContext *ctx) {
	update_proc(layer, ctx, time_paths[MINUTE], time_infos[MINUTE], time_angles[MINUTE]);
}

static void hour_update_proc(Layer *layer, GContext *ctx) {
	update_proc(layer, ctx, time_paths[HOUR], time_infos[HOUR], time_angles[HOUR]);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
	if (units_changed & SECOND_UNIT) {
		time_angles[SECOND] = TRIG_MAX_ANGLE * tick_time->tm_sec / 60;
		layer_mark_dirty(time_layers[SECOND]); 
	}
 
	if (units_changed & MINUTE_UNIT) {
		time_angles[MINUTE] = TRIG_MAX_ANGLE * tick_time->tm_min / 60;
		layer_mark_dirty(time_layers[MINUTE]);
	}

	if (units_changed & HOUR_UNIT) {
		if (clock_is_24h_style()) {
			time_angles[HOUR] = TRIG_MAX_ANGLE * tick_time->tm_hour / 24;
		} else {
			time_angles[HOUR] = TRIG_MAX_ANGLE * (tick_time->tm_hour % 12) / 12;
		}
		layer_mark_dirty(time_layers[HOUR]);
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

	GRect layer_bounds[] = {
		{
			.origin = { 0, 0 },
			.size = {bounds.size.w, bounds.size.h}
		},
		{
			.origin = {LAYER_WIDTH, LAYER_WIDTH},
			.size = {bounds.size.w - LAYER_WIDTH * 2, bounds.size.h - LAYER_WIDTH * 2}
		},
		{
			.origin = {LAYER_WIDTH, LAYER_WIDTH},
			.size = {bounds.size.w - LAYER_WIDTH * 4, bounds.size.h - LAYER_WIDTH * 4}
		}
	};

	void (*update_proc[TIME_COUNT]) (Layer *layer, GContext *ctx);
	update_proc[SECOND] = &second_update_proc;
	update_proc[MINUTE] = &minute_update_proc;
	update_proc[HOUR] = &hour_update_proc;

	Layer* parent = window_layer;
	for (int i = 0, j = 0; i < TIME_COUNT; i++) {
		time_layers[i] = layer_create(layer_bounds[i]);
		time_paths[i] = gpath_create(&time_infos[i]);
		layer_set_update_proc(time_layers[i], *update_proc[i]);
		layer_add_child(parent, time_layers[i]);
		if (i > 0) {
			j = generate_inverter_layers(j, time_layers[i]);
		}
		parent = time_layers[i];
	}

	// Kick off first update
	time_t t = time(NULL);
	tick_handler(localtime(&t), SECOND_UNIT | HOUR_UNIT | MINUTE_UNIT);
}

static void window_unload(Window *window) {
	for (int i = 0; i < TIME_COUNT; i++) {
		layer_destroy(time_layers[i]);
		gpath_destroy(time_paths[i]);
	}

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
