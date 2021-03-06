#include <string>
#include <iostream>
#include <fstream>
#include <queue>
#include <deque>
// #include <thread>
#include <pthread.h>
#include <memory>

// Tizen libraries
#include <sensor.h>
#include <efl_util.h>
#include <device/power.h>

#include "drunkare_context.h"
#include "queue.h"
#include "data.h"

#define NUM_SENSORS 2
#define NUM_CHANNELS 3
#define NUM_CONTEXTS 4
#define DURATION 60 // seconds
#define CONTEXT_DURATION 600 // seconds
#define MAX_MEASURE_ID (CONTEXT_DURATION / DURATION)
#define ACCELEROMETER 0
#define GYROSCOPE 1

using TMeasure = Measure<NUM_CHANNELS, DURATION>;

static std::vector<std::string> contexts = {"drink", "eat", "cafe", "desk"};
static std::vector<std::pair<int, int>> ofs = {
    {50, 50}, {190, 50}, {50, 190}, {190, 190}};

struct appdata_s {
  Evas_Object *win;
  Evas_Object *conform;
  Evas_Object *label;
  std::vector<Evas_Object *> buttons;
  std::string response; // TODO: delete this

  // Extra app data
  bool _isMeasuring;
  int _numWrite;
  int _context;
  sensor_h sensors[NUM_SENSORS];
  sensor_listener_h listners[NUM_SENSORS];
  int _deviceSamplingRate = 10;
  std::vector<int> _measureId;
  std::vector<int> _doneMeasureId;
  std::deque<std::unique_ptr<TMeasure>> tMeasures[NUM_SENSORS];
  pthread_t fsWorker; // format and write to file system
  Queue<TMeasure> queue;

  std::string filepath;
  std::string pathname;

  appdata_s() : win(nullptr) {}
};

static void win_delete_request_cb(void *data, Evas_Object *obj,
                                  void *event_info) {
  ui_app_exit();
}

static void
win_back_cb(void *data, Evas_Object *obj, void *event_info)
{
	appdata_s *ad = (appdata_s *)data;
        /* Let window go to hide state. */
	elm_win_lower(ad->win);
}

//
// Main function for `fsWorker`.
//   1. Dequeue a `Measure` from `ad->queue`
//   2. Format POST fields
//   3. Send POST request to <hostname:port>
//   4. Repeat
//
static void* fsWorkerJob(void* data) {
  appdata_s *ad = (appdata_s *)data;

  while (true) {
    auto tMeasure = ad->queue.dequeue();

    if (!tMeasure)
      break;

    std::ofstream ofs;
    ofs.open(ad->pathname.c_str() , ofs.app);

    if (!ofs.is_open()) {
      dlog_print(DLOG_DEBUG, "fsWorkerJob", "[-] ofs.is_open()");
    } else {
      ofs << tMeasure->format() << '\n';
    }
  };

  return nullptr;
}

static void startMeasurement(appdata_s *ad);
static void stopMeasurement(appdata_s *ad);

void sensorCb(sensor_h sensor, sensor_event_s *event, void *user_data)
{
  appdata_s *ad = (appdata_s *)user_data;
  int sensor_type;
  sensor_type_e type;
  sensor_get_type(sensor, &type);
  std::vector<float> values;

  switch (type) {
  case SENSOR_ACCELEROMETER:
    sensor_type = ACCELEROMETER;
    break;
  case SENSOR_GYROSCOPE:
    sensor_type = GYROSCOPE;
    break;
  default:
    return;
  }

  // Save temporary value array
  for (int i = 0; i < NUM_CHANNELS; i++) {
    values.push_back(event->values[i]);
  }

  // Check tMeasures deque
  if (ad->tMeasures[sensor_type].empty()) {
    ad->tMeasures[sensor_type].push_back(
        std::make_unique<TMeasure>(ad->_measureId[sensor_type]++,
                                   sensor_type, ad->_context));
    dlog_print(DLOG_DEBUG, LOG_TAG, "tMeasure ( %d ) is created.",
               ad->_measureId[sensor_type] - 1);
  }

  // Tick (store values in Measure.data every periods)
  ad->tMeasures[sensor_type].front()->tick(values);

  // Check Measure->_done and enqueue
  if (ad->tMeasures[sensor_type].front()->_done) {
    dlog_print(DLOG_DEBUG, LOG_TAG, "tMeasure ( %d ) is done.",
               ad->_measureId[sensor_type] - 1);

    ad->_doneMeasureId[sensor_type] = ad->tMeasures[sensor_type].front()->_id;

    ad->queue.enqueue(std::move(ad->tMeasures[sensor_type].front()));
    ad->tMeasures[sensor_type].pop_front();

    // Check termination condition
    if (ad->_doneMeasureId[sensor_type] >= MAX_MEASURE_ID) {
      bool allFinished = true;

      for (auto doneId : ad->_doneMeasureId) {
        allFinished &= (doneId >= MAX_MEASURE_ID);
      }

      // dlog_print(DLOG_DEBUG, "sensorCb", "[+] Hey, are we done?");

      if (allFinished) {
        // dlog_print(DLOG_DEBUG, "sensorCb", "[+] Yes, we are done!");
        stopMeasurement(ad);
        return;
      }

      // dlog_print(DLOG_DEBUG, "sensorCb", "[+] Nope, we are not done yet!");
    }
  }
}

static void startMeasurement(appdata_s *ad)
{
  ad->queue.clear();

  // Create thread here
  if (pthread_create(&ad->fsWorker, nullptr, fsWorkerJob, (void *)ad) < 0) {
    dlog_print(DLOG_ERROR, "btnClickedCb", "[-] pthread_create()");
    return;
  };

  for (int i = 0; i < NUM_SENSORS; i++) {
    sensor_listener_set_event_cb(ad->listners[i], ad->_deviceSamplingRate,
                                 sensorCb, ad);
    sensor_listener_start(ad->listners[i]);
  }
  ad->_isMeasuring = true;
}

static void stopMeasurement(appdata_s *ad)
{
  ad->_isMeasuring = false;

  for (int i = 0; i < NUM_SENSORS; i++) {
    sensor_listener_stop(ad->listners[i]);
  }

  ad->queue.forceDone();
  pthread_join(ad->fsWorker, nullptr);

  for (int i = 0; i < ad->_measureId.size(); i++) {
    ad->_measureId[i] = 0;
    ad->_doneMeasureId[i] = -1;
    // dlog_print(DLOG_DEBUG, "stopMeasurement", "[+] ad->_measureId[%d] = %d", i,
    //            ad->_measureId[i]);
  }

  for (auto button : ad->buttons) {
    elm_object_disabled_set(button, EINA_FALSE);
  }

  // Set screen to default mode
  efl_util_set_window_screen_mode(ad->win, EFL_UTIL_SCREEN_MODE_DEFAULT);
}

static void btnClickedCb(void *data, Evas_Object *obj, void *event_info)
{
  appdata_s* ad = (appdata_s *)data;

  // 1. Set screen always on (This is due to hardware limitation)
  efl_util_set_window_screen_mode(ad->win, EFL_UTIL_SCREEN_MODE_ALWAYS_ON);

  // 2. Set context
  try {
    std::string context = std::string(elm_object_text_get(obj));

    if (context == "drink")
      ad->_context = 0;
    else if (context == "eat")
      ad->_context = 1;
    else if (context == "cafe")
      ad->_context = 2;
    else if (context == "desk")
      ad->_context = 3;
    else
      throw std::exception();
  } catch (const std::exception& e) {
    dlog_print(DLOG_ERROR, "btnClickedCb", "[-] %s", e.what());
  }

  if (!ad->_isMeasuring) {
    for (auto button : ad->buttons) {
      elm_object_disabled_set(button, EINA_TRUE);
    }

    startMeasurement(ad);
  }
}

static void
init_buttons(appdata_s *ad,
            void (*cb)(void *data, Evas_Object *obj, void *event_info))
{
  for (int i = 0; i < NUM_CONTEXTS; i++) {
    auto button = elm_button_add(ad->win);
    evas_object_smart_callback_add(button, "clicked", cb, ad);
    ad->buttons.push_back(button);
    evas_object_move(button, ofs[i].first, ofs[i].second);
    evas_object_resize(button, 120, 120);
    elm_object_text_set(button, contexts[i].c_str());
    evas_object_show(button);
    ad->buttons.push_back(button);
  }
}

static void
create_base_gui(appdata_s *ad)
{
	/* Window */
	/* Create and initialize elm_win.
	   elm_win is mandatory to manipulate window. */
	ad->win = elm_win_util_standard_add(PACKAGE, PACKAGE);
	elm_win_autodel_set(ad->win, EINA_TRUE);

	if (elm_win_wm_rotation_supported_get(ad->win)) {
		int rots[4] = { 0, 90, 180, 270 };
		elm_win_wm_rotation_available_rotations_set(ad->win, (const int *)(&rots), 4);
	}

	evas_object_smart_callback_add(ad->win, "delete,request", win_delete_request_cb, NULL);
	eext_object_event_callback_add(ad->win, EEXT_CALLBACK_BACK, win_back_cb, ad);

	/* Conformant */
	/* Create and initialize elm_conformant.
	   elm_conformant is mandatory for base gui to have proper size
	   when indicator or virtual keypad is visible. */
	ad->conform = elm_conformant_add(ad->win);
	elm_win_indicator_mode_set(ad->win, ELM_WIN_INDICATOR_SHOW);
	elm_win_indicator_opacity_set(ad->win, ELM_WIN_INDICATOR_OPAQUE);
	evas_object_size_hint_weight_set(ad->conform, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_win_resize_object_add(ad->win, ad->conform);
	evas_object_show(ad->conform);

	/* Label */
	/* Create an actual view of the base gui.
	   Modify this part to change the view. */
	ad->label = elm_label_add(ad->conform);
	elm_object_text_set(ad->label, "<align=center>Hello Tizen</align>");
	evas_object_size_hint_weight_set(ad->label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_content_set(ad->conform, ad->label);

        /* Custom initializations are here! */
        ad->response = ""; // TODO: delete this
        ad->_isMeasuring = false;
        init_buttons(ad, btnClickedCb);
        sensor_get_default_sensor(SENSOR_ACCELEROMETER, &ad->sensors[ACCELEROMETER]);
        sensor_get_default_sensor(SENSOR_GYROSCOPE, &ad->sensors[GYROSCOPE]);
        for (int i = 0; i < NUM_SENSORS; i++) {
          sensor_create_listener(ad->sensors[i], &ad->listners[i]);

          // Initialize per-sensor measure IDs
          ad->_measureId.push_back(0);
          ad->_doneMeasureId.push_back(-1);
        }

        ad->filepath = std::string(app_get_data_path());
        ad->pathname = ad->filepath + std::string("data.csv");

        /* Show window after base gui is set up */
	evas_object_show(ad->win);
}

static bool
app_create(void *data)
{
	/* Hook to take necessary actions before main event loop starts
		Initialize UI resources and application's data
		If this function returns true, the main loop of application starts
		If this function returns false, the application is terminated */
	appdata_s *ad = (appdata_s *)data;

	create_base_gui(ad);

	return true;
}

static void
app_control(app_control_h app_control, void *data)
{
	/* Handle the launch request. */
}

static void
app_pause(void *data)
{
	/* Take necessary actions when application becomes invisible. */
}

static void
app_resume(void *data)
{
	/* Take necessary actions when application becomes visible. */
}

static void
app_terminate(void *data)
{
	/* Release all resources. */
}

static void
ui_app_lang_changed(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_LANGUAGE_CHANGED*/
	char *locale = NULL;
	system_settings_get_value_string(SYSTEM_SETTINGS_KEY_LOCALE_LANGUAGE, &locale);
	elm_language_set(locale);
	free(locale);
	return;
}

static void
ui_app_orient_changed(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_DEVICE_ORIENTATION_CHANGED*/
	return;
}

static void
ui_app_region_changed(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_REGION_FORMAT_CHANGED*/
}

static void
ui_app_low_battery(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_LOW_BATTERY*/
}

static void
ui_app_low_memory(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_LOW_MEMORY*/
}

int
main(int argc, char *argv[])
{
  appdata_s ad;
	int ret = 0;

	ui_app_lifecycle_callback_s event_callback = {0,};
	app_event_handler_h handlers[5] = {NULL, };

        bool supported[NUM_SENSORS];
	sensor_is_supported(SENSOR_ACCELEROMETER, &supported[ACCELEROMETER]);
	sensor_is_supported(SENSOR_GYROSCOPE, &supported[GYROSCOPE]);
	if (!supported[ACCELEROMETER] || !supported[GYROSCOPE]) {
		/* Accelerometer is not supported on the current device */
		return 1;
	}

        // Initialize sensor handles
        // 	sensor_get_default_sensor(SENSOR_ACCELEROMETER, &sensors[ACCELEROMETER]);
        // 	sensor_get_default_sensor(SENSOR_GYROSCOPE, &sensors[GYROSCOPE]);
        // 
        //         // Initialize sensor listeners
        //         for (int i = 0; i < NUM_SENSORS; i++) {
        //           sensor_create_listener(sensors[sensor_index],
        //                                  &listeners[sensor_index]);
        //         }

	event_callback.create = app_create;
	event_callback.terminate = app_terminate;
	event_callback.pause = app_pause;
	event_callback.resume = app_resume;
	event_callback.app_control = app_control;

	ui_app_add_event_handler(&handlers[APP_EVENT_LOW_BATTERY], APP_EVENT_LOW_BATTERY, ui_app_low_battery, &ad);
	ui_app_add_event_handler(&handlers[APP_EVENT_LOW_MEMORY], APP_EVENT_LOW_MEMORY, ui_app_low_memory, &ad);
	ui_app_add_event_handler(&handlers[APP_EVENT_DEVICE_ORIENTATION_CHANGED], APP_EVENT_DEVICE_ORIENTATION_CHANGED, ui_app_orient_changed, &ad);
	ui_app_add_event_handler(&handlers[APP_EVENT_LANGUAGE_CHANGED], APP_EVENT_LANGUAGE_CHANGED, ui_app_lang_changed, &ad);
	ui_app_add_event_handler(&handlers[APP_EVENT_REGION_FORMAT_CHANGED], APP_EVENT_REGION_FORMAT_CHANGED, ui_app_region_changed, &ad);

	ret = ui_app_main(argc, argv, &event_callback, &ad);
	if (ret != APP_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "app_main() is failed. err = %d", ret);
	}

	return ret;
}
