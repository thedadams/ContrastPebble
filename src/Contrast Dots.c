#include <pebble.h>

Window *window;
Layer *hour_layer;
Layer *minute_layer;
InverterLayer *battery;

static int device_height;
static int device_width;
static int hour_height_error;
static int minute_height_error;
static int minute_width_error;
static int hour_width_error;

static const int hourRadius = 6;
static const int minuteRadius = 6;
static GPoint hourPoints[24];
static GPoint minutePoints[60];
static int xHourSpace;
static int yHourSpace;
static int xMinuteSpace;
static int yMinuteSpace;

static int min;
static int hour;

static bool wasConnected;

static void createCircles() {
  int xCounter = xHourSpace + hourRadius + hour_width_error;
  int yCounter = yHourSpace + hourRadius + hour_height_error;
  for(int i = 1; i <= 24; i++) {
    hourPoints[i-1] = GPoint(xCounter, yCounter);

    if(i % 12 == 0) {
      xCounter += 2 * (xHourSpace + hourRadius);
      yCounter = yHourSpace + hourRadius + hour_height_error;
    } else {
      yCounter += 2 * (yHourSpace + hourRadius);
    }
  }

  xCounter = xMinuteSpace + minuteRadius + minute_width_error;
  yCounter = yMinuteSpace + minuteRadius + minute_height_error;
  for(int i = 1; i <= 60; i++) {
    minutePoints[i-1] = GPoint(xCounter, yCounter);

    if(i % 10 == 0) {
      xCounter += 2 * (xMinuteSpace + minuteRadius);
      yCounter = (yMinuteSpace + minuteRadius) + minute_height_error;
    } else {
      yCounter += 2 * (yMinuteSpace + minuteRadius);
    }
  }

}

static void update_hour(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, hourPoints[(hour + 23) % 24], hourRadius);
  for(int i = 0; i < 24; i++) {
    if(i == (hour + 23) % 24) continue;
    graphics_draw_circle(ctx, hourPoints[i], hourRadius);
  }
}

static void update_minute(Layer *layer, GContext *ctx) {
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_draw_circle(ctx, minutePoints[(min + 59) % 60], minuteRadius);
  for(int i = 0; i < 60; i++) {
    if(i == (min + 59) % 60) continue;
    graphics_fill_circle(ctx, minutePoints[i], minuteRadius);
  }
}

static void handle_minute_tick(struct tm* tick_time, TimeUnits units_changed) {
  if(hour != tick_time->tm_hour) {
    hour = tick_time->tm_hour;
    layer_mark_dirty(hour_layer);
  }
  min = tick_time->tm_min;
  layer_mark_dirty(minute_layer);
}

static void bluetooth_change(bool connected) {
  if(!connected && wasConnected) {
    wasConnected = false;
    vibes_long_pulse();
  } else if(connected && !wasConnected) {
    wasConnected = true;
    vibes_double_pulse();
    // Update the time in case it has changed (e.g. flight across time zones).
    psleep(10000);
    time_t    now           = time(NULL);
    struct tm *current_time = localtime(&now);
    handle_minute_tick(current_time, DAY_UNIT);
  }
}

static void battery_update(BatteryChargeState charge) {
  layer_set_bounds(inverter_layer_get_layer(battery),GRect(0,device_height - (charge.charge_percent * device_height) / 100,(2 * device_width) / 7,(charge.charge_percent * device_height) / 91));
}

static void init(void) {
  window = window_create();
  window_set_background_color(window, GColorBlack);
  window_set_fullscreen(window, false);
  window_stack_push(window, true);
  Layer *root_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root_layer);
  device_height = bounds.size.h;
  device_width = bounds.size.w;
  yHourSpace = ((device_height / 12) - 2 * hourRadius) / 2;
  xHourSpace = ((device_width / 7) - (2 * hourRadius)) / 2;
  yMinuteSpace = ((device_height / 10) - 2 * minuteRadius) / 2;
  xMinuteSpace = ((5 * device_width / 42) - (2 * minuteRadius)) / 2;
  minute_height_error = (device_height - (10 * (device_height / 10))) / 2;
  hour_height_error = (device_height - (12 * (device_height / 12))) / 2;
  minute_width_error = ((5 * device_width / 7) - (6 * (2 * (minuteRadius + xMinuteSpace)))) / 2;
  hour_width_error = ((2 * device_width / 7) - (2 * (2 * (hourRadius + xHourSpace)))) / 2;
  createCircles();

  hour_layer = layer_create(GRect(0,0,(2 * device_width) / 7, device_height));
  layer_set_update_proc(hour_layer, update_hour);
  layer_add_child(root_layer,hour_layer);
  layer_mark_dirty(hour_layer);

  minute_layer = layer_create(GRect((2 * device_width) / 7, 0, (5 * device_width) / 7,device_height));
  layer_set_update_proc(minute_layer, update_minute);
  layer_add_child(root_layer,minute_layer);
  layer_mark_dirty(minute_layer);

  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  handle_minute_tick(t,MINUTE_UNIT);

  battery = inverter_layer_create(GRect(0,0,(2 * device_width) / 7,device_height));
  battery_update(battery_state_service_peek());
  layer_add_child(root_layer,inverter_layer_get_layer(battery));

  wasConnected = bluetooth_connection_service_peek();
  tick_timer_service_subscribe(MINUTE_UNIT,handle_minute_tick);
  bluetooth_connection_service_subscribe(&bluetooth_change);
  battery_state_service_subscribe(&battery_update);
}

static void deinit(void) {
  battery_state_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
  tick_timer_service_unsubscribe();
  inverter_layer_destroy(battery);
  layer_destroy(minute_layer);
  layer_destroy(hour_layer);
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
