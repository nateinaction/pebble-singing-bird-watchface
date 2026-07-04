#include <pebble.h>
#include "../../build/include/message_keys.auto.h"

// ---------------------------------------------------------------------------
// Texas Songbird — a magnified analog dial on black where each of the 12 hour
// positions is a Texas songbird. A single vermilion hand sweeps toward "now".
// On the hour (and on a wrist tap) the current bird's recorded call plays; a
// tap also shows a box with the bird's name, description, and Texas range.
//
// Built on the Chronology 2 watchface: the dial lives on a large off-screen
// layer that orbits the wrist (update_frame_location), so only a slice of the
// ring is ever visible. We keep that machinery and draw birds where Chronology
// drew numerals.
// ---------------------------------------------------------------------------

#define NUM_BIRDS 12
#define BIRD_CACHE_SPAN 2      // load current hour +/- this many neighbours
#define AUDIO_VOLUME 80
#define AUDIO_CHUNK 1024
#define TAP_DEBOUNCE_MS 700
#define INFO_VISIBLE_MS 4200

typedef struct {
  const char *name;
  const char *desc;
  const char *range;
} BirdInfo;

// index 0 = 12 o'clock, clockwise
static const BirdInfo BIRDS[NUM_BIRDS] = {
  {"Northern Mockingbird", "Texas state bird; bold mimic.",   "Statewide, all year"},
  {"Painted Bunting",      "Dazzling multicolored finch.",    "E & central TX, summer"},
  {"Scissor-tailed Flycatcher", "Long forked tail; TX icon.", "Statewide, summer"},
  {"Vermilion Flycatcher", "Brilliant red flycatcher.",       "S & W TX, all year"},
  {"Eastern Screech-Owl",  "Small eared night owl.",          "E & central TX, all year"},
  {"Carolina Wren",        "Loud tea-kettle song.",           "E Texas, all year"},
  {"Bewick's Wren",        "Long white-edged tail.",          "W & central TX, all year"},
  {"American Robin",       "Orange-breasted thrush.",         "Statewide, winter"},
  {"Northern Parula",      "Tiny blue-and-yellow warbler.",   "E Texas, summer"},
  {"Golden-cheeked Warbler","Nests only in Texas.",           "Hill Country, summer"},
  {"Blue Jay",             "Crested, bold, and noisy.",       "E & central TX, all year"},
  {"Northern Cardinal",    "Crimson crest; clear whistle.",   "Statewide, all year"},
};

static const uint32_t BIRD_RES[NUM_BIRDS] = {
  RESOURCE_ID_BIRD_00, RESOURCE_ID_BIRD_01, RESOURCE_ID_BIRD_02, RESOURCE_ID_BIRD_03,
  RESOURCE_ID_BIRD_04, RESOURCE_ID_BIRD_05, RESOURCE_ID_BIRD_06, RESOURCE_ID_BIRD_07,
  RESOURCE_ID_BIRD_08, RESOURCE_ID_BIRD_09, RESOURCE_ID_BIRD_10, RESOURCE_ID_BIRD_11,
};

static const uint32_t CALL_RES[NUM_BIRDS] = {
  RESOURCE_ID_CALL_00, RESOURCE_ID_CALL_01, RESOURCE_ID_CALL_02, RESOURCE_ID_CALL_03,
  RESOURCE_ID_CALL_04, RESOURCE_ID_CALL_05, RESOURCE_ID_CALL_06, RESOURCE_ID_CALL_07,
  RESOURCE_ID_CALL_08, RESOURCE_ID_CALL_09, RESOURCE_ID_CALL_10, RESOURCE_ID_CALL_11,
};

static Window *s_main_window;
static Layer *s_face_layer;
static Layer *s_hand_layer;
static Layer *s_info_layer;

static bool debug = false;
static float s_scale = 1.0f;
static int16_t s_orbit_inset = 150;
static GColor s_hand_color;
static bool s_muted = false;

static GBitmap *s_birds[NUM_BIRDS];   // lazily loaded; NULL when not cached

// audio streaming state
static uint8_t *s_audio_buf = NULL;
static uint32_t s_audio_len = 0;
static uint32_t s_audio_pos = 0;
static AppTimer *s_audio_timer = NULL;

// info box state
static int s_info_idx = -1;
static AppTimer *s_info_timer = NULL;

// tap debounce
static time_t s_last_tap = 0;
static uint16_t s_last_tap_ms = 0;

static int current_hour_index() {
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  int h = debug ? (tm->tm_sec / 5) : tm->tm_hour;
  return ((h % 12) + 12) % 12;
}

// ---------------------------------------------------------------------------
// Audio (Speaker PCM streaming)
// ---------------------------------------------------------------------------

static void audio_cleanup() {
  if (s_audio_timer) { app_timer_cancel(s_audio_timer); s_audio_timer = NULL; }
  if (s_audio_buf) { free(s_audio_buf); s_audio_buf = NULL; }
  s_audio_len = 0;
  s_audio_pos = 0;
}

static void audio_finished(SpeakerFinishReason reason, void *ctx) {
  audio_cleanup();
}

static void audio_feed(void *data) {
  s_audio_timer = NULL;
  if (!s_audio_buf) return;
  while (s_audio_pos < s_audio_len) {
    uint32_t remaining = s_audio_len - s_audio_pos;
    uint32_t chunk = remaining < AUDIO_CHUNK ? remaining : AUDIO_CHUNK;
    uint32_t wrote = speaker_stream_write(s_audio_buf + s_audio_pos, chunk);
    s_audio_pos += wrote;
    if (wrote < chunk) break;   // speaker buffer full; wait and retry
  }
  if (s_audio_pos < s_audio_len) {
    s_audio_timer = app_timer_register(20, audio_feed, NULL);
  } else {
    speaker_stream_close();     // remaining buffered data drains, then callback frees
  }
}

// respect_quiet: skip when Quiet Time is on (used for the automatic on-hour call)
static void play_call(int hour, bool respect_quiet) {
  if (s_muted) return;
  if (respect_quiet && quiet_time_is_active()) return;
  hour = ((hour % 12) + 12) % 12;

  // stop anything already playing
  speaker_stop();
  audio_cleanup();

  ResHandle h = resource_get_handle(CALL_RES[hour]);
  size_t size = resource_size(h);
  if (size == 0) return;
  s_audio_buf = malloc(size);
  if (!s_audio_buf) return;
  s_audio_len = resource_load(h, s_audio_buf, size);
  s_audio_pos = 0;

  if (!speaker_stream_open(SpeakerPcmFormat_8kHz_8bit, AUDIO_VOLUME)) {
    audio_cleanup();
    return;
  }
  speaker_set_finish_callback(audio_finished, NULL);
  audio_feed(NULL);
}

// ---------------------------------------------------------------------------
// Bird bitmap cache — only the visible arc (current hour +/- span) is resident
// ---------------------------------------------------------------------------

static bool bird_in_window(int i, int center) {
  for (int d = -BIRD_CACHE_SPAN; d <= BIRD_CACHE_SPAN; d++) {
    if (((center + d + NUM_BIRDS) % NUM_BIRDS) == i) return true;
  }
  return false;
}

static void manage_bird_cache() {
  int center = current_hour_index();
  for (int i = 0; i < NUM_BIRDS; i++) {
    bool want = bird_in_window(i, center);
    if (want && !s_birds[i]) {
      s_birds[i] = gbitmap_create_with_resource(BIRD_RES[i]);
    } else if (!want && s_birds[i]) {
      gbitmap_destroy(s_birds[i]);
      s_birds[i] = NULL;
    }
  }
}

// ---------------------------------------------------------------------------
// Info box
// ---------------------------------------------------------------------------

static void hide_info(void *data) {
  s_info_timer = NULL;
  s_info_idx = -1;
  layer_set_hidden(s_info_layer, true);
  layer_mark_dirty(s_info_layer);
}

static void show_info(int hour) {
  s_info_idx = ((hour % 12) + 12) % 12;
  layer_set_hidden(s_info_layer, false);
  layer_mark_dirty(s_info_layer);
  if (s_info_timer) app_timer_cancel(s_info_timer);
  s_info_timer = app_timer_register(INFO_VISIBLE_MS, hide_info, NULL);
}

static void info_draw(Layer *layer, GContext *ctx) {
  if (s_info_idx < 0) return;
  const BirdInfo *b = &BIRDS[s_info_idx];
  GRect bounds = layer_get_bounds(layer);
  GRect box = GRect(6, bounds.size.h - 90, bounds.size.w - 12, 84);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, box, 8, GCornersAll);
  graphics_context_set_stroke_color(ctx, s_hand_color);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_round_rect(ctx, box, 8);

  GRect inner = grect_inset(box, GEdgeInsets(6, 8));
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, b->name, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(inner.origin.x, inner.origin.y, inner.size.w, 24),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, b->desc, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(inner.origin.x, inner.origin.y + 26, inner.size.w, 20),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, b->range, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(inner.origin.x, inner.origin.y + 46, inner.size.w, 20),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
}

// ---------------------------------------------------------------------------
// Dial + hand (from Chronology)
// ---------------------------------------------------------------------------

static void update_frame_location() {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  int bhour = tick_time->tm_hour;
  int bmin = tick_time->tm_min;
  float angle = 30 * ((float)(bhour % 12) + ((float)bmin / 60));
  if (debug) angle = 12 * tick_time->tm_sec;

  GRect frame = layer_get_frame(s_face_layer);
  GRect frame2 = layer_get_frame(s_hand_layer);

  GPoint origin = gpoint_from_polar(grect_inset(frame2, GEdgeInsets(-s_orbit_inset)),
                                    GOvalScaleModeFitCircle, DEG_TO_TRIGANGLE(angle + 180));
  frame.origin = origin;
  frame.origin.x -= frame.size.w / 2;
  frame.origin.y -= frame.size.h / 2;
  layer_set_frame(s_face_layer, frame);
}

static void my_hand_draw(Layer *layer, GContext *ctx) {
  GRect face_frame = layer_get_frame(s_face_layer);
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  int bhour = tick_time->tm_hour;
  int bmin = tick_time->tm_min;
  float angle = 30 * ((float)(bhour % 12) + ((float)bmin / 60));
  if (debug) angle = 12 * tick_time->tm_sec;

  GPoint center = GPoint(face_frame.origin.x + face_frame.size.w / 2,
                         face_frame.origin.y + face_frame.size.h / 2);
  GPoint end_point = gpoint_from_polar(face_frame, GOvalScaleModeFitCircle, DEG_TO_TRIGANGLE(angle));

  graphics_context_set_stroke_color(ctx, s_hand_color);
  graphics_context_set_stroke_width(ctx, 6);
  graphics_draw_line(ctx, center, end_point);
}

static void my_face_draw(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  const int16_t hour_inset = (int16_t)(20 * s_scale);

  graphics_context_set_antialiased(ctx, false);
  graphics_context_set_compositing_mode(ctx, GCompOpSet);

  for (int i = 0; i < NUM_BIRDS; i++) {
    int angle = DEG_TO_TRIGANGLE(i * 30);

    // hour tick: a slightly longer white line at the rim
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_line(ctx,
                       gpoint_from_polar(grect_crop(bounds, hour_inset), GOvalScaleModeFitCircle, angle),
                       gpoint_from_polar(bounds, GOvalScaleModeFitCircle, angle));

    // quiet arc of minute dots between the hours
    int a = angle;
    for (int j = 1; j < 12; j++) {
      a += DEG_TO_TRIGANGLE(2.5);
      GPoint dot = gpoint_from_polar(bounds, GOvalScaleModeFitCircle, a);
      graphics_context_set_fill_color(ctx, GColorLightGray);
      graphics_fill_circle(ctx, dot, 2);
    }

    // the bird, drawn just inside the tick (only cached birds render)
    if (s_birds[i]) {
      GRect bb = gbitmap_get_bounds(s_birds[i]);
      GPoint c = gpoint_from_polar(grect_crop(bounds, hour_inset + bb.size.h - 6),
                                   GOvalScaleModeFitCircle, angle);
      GRect dest = GRect(c.x - bb.size.w / 2, c.y - bb.size.h / 2, bb.size.w, bb.size.h);
      graphics_draw_bitmap_in_rect(ctx, s_birds[i], dest);
    }
  }
}

// ---------------------------------------------------------------------------
// Services
// ---------------------------------------------------------------------------

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  manage_bird_cache();
  update_frame_location();
  layer_mark_dirty(s_face_layer);
  layer_mark_dirty(s_hand_layer);

  if ((units_changed & MINUTE_UNIT) && tick_time->tm_min == 0) {
    play_call(((tick_time->tm_hour % 12) + 12) % 12, true /* respect quiet time */);
  }
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  // debounce so ordinary wrist motion doesn't retrigger constantly
  time_t now; uint16_t now_ms;
  time_ms(&now, &now_ms);
  int32_t delta = (int32_t)(now - s_last_tap) * 1000 + ((int)now_ms - (int)s_last_tap_ms);
  if (delta < TAP_DEBOUNCE_MS) return;
  s_last_tap = now; s_last_tap_ms = now_ms;

  int hour = current_hour_index();
  show_info(hour);
  play_call(hour, false /* tap always plays, even in quiet time */);
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  int16_t screen_size = bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h;
  s_scale = 180.0f / (float)screen_size;
  s_orbit_inset = (int16_t)(PBL_IF_ROUND_ELSE(130.0f, 150.0f) * (float)screen_size / 180.0f);

  s_face_layer = layer_create(GRect(0, 0, bounds.size.h * 3, bounds.size.h * 3));
  layer_set_update_proc(s_face_layer, my_face_draw);
  layer_set_clips(s_face_layer, false);

  s_hand_layer = layer_create(bounds);
  layer_set_update_proc(s_hand_layer, my_hand_draw);

  s_info_layer = layer_create(bounds);
  layer_set_update_proc(s_info_layer, info_draw);
  layer_set_hidden(s_info_layer, true);

  manage_bird_cache();
  update_frame_location();

  layer_add_child(window_layer, s_face_layer);
  layer_add_child(window_layer, s_hand_layer);
  layer_add_child(window_layer, s_info_layer);
}

static void main_window_unload(Window *window) {
  layer_destroy(s_face_layer);
  layer_destroy(s_hand_layer);
  layer_destroy(s_info_layer);
  for (int i = 0; i < NUM_BIRDS; i++) {
    if (s_birds[i]) { gbitmap_destroy(s_birds[i]); s_birds[i] = NULL; }
  }
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  Tuple *hand = dict_find(iterator, MESSAGE_KEY_HAND_HEX);
  if (hand) {
    int32_t hex = hand->value->int32;
    s_hand_color = GColorFromHEX(hex);
    persist_write_int(7, hex);
  }
  Tuple *mute = dict_find(iterator, MESSAGE_KEY_MUTE);
  if (mute) {
    s_muted = mute->value->int32 != 0;
    persist_write_bool(10, s_muted);
  }
  layer_mark_dirty(s_face_layer);
  layer_mark_dirty(s_hand_layer);
}

static void init() {
  s_hand_color = GColorFromHEX(persist_exists(7) ? persist_read_int(7) : 0xFF5500);
  s_muted = persist_exists(10) ? persist_read_bool(10) : false;

  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);
  window_set_window_handlers(s_main_window, (WindowHandlers){
      .load = main_window_load, .unload = main_window_unload});
  window_stack_push(s_main_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  accel_tap_service_subscribe(tap_handler);

  app_message_register_inbox_received(inbox_received_callback);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}

static void deinit() {
  speaker_stop();
  audio_cleanup();
  accel_tap_service_unsubscribe();
  tick_timer_service_unsubscribe();
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
