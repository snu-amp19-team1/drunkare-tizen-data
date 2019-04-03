#include <accelerometer.h>
#include <sensor.h>
#include <app.h>
#include "dlog.h"
#include <Elementary.h>

#define NUM_OF_SENSOR		2
#define ACCELEROMETER		0
#define GYROSCOPE			1
#define MILLION				1000000
#define SAMPLING_RATE		40
#define SAMPLES_PER_SESOND	(1000 / SAMPLING_RATE)

sensor_h default_accelerometer;
sensor_h default_gyroscope;

sensor_listener_h accel_listener;
sensor_listener_h gyro_listener;


typedef struct sensordata {
	unsigned long long timestamp;
	float x, y, z;
} sensordata_s;

typedef struct appdata {
	Evas_Object *win;
	Evas_Object *conform;
	Evas_Object *label;
	Evas_Object *button;
	char *state;
	sensordata_s *sensor_data[NUM_OF_SENSOR];
} appdata_s;


/* Common callback function */
void example_sensor_callback(sensor_h sensor, sensor_event_s *event, void *user_data) {
	/*
	 If a callback is used to listen for different sensor types,
	 it can check the sensor type
	 */
	sensor_type_e type;
	sensor_get_type(sensor, &type);
	unsigned long long timestamp;
	char buf[32];
	float x, y, z;

	switch (type) {
	case SENSOR_ACCELEROMETER:
		timestamp = event->timestamp;
		// Print timestamp
		sprintf(buf,"[ACCEL] time : %lld",timestamp);
		dlog_print(DLOG_DEBUG, "accel callback", buf);

		// Currently comment out to suppress unused variable warning
		// int accuracy = event->accuracy;

		// Print accelerometer data
		x = event->values[0];
		y = event->values[1];
		z = event->values[2];
		sprintf(buf,"%f %f %f",x,y,z);
		dlog_print(DLOG_DEBUG, "accel callback", buf);
		break;
	case SENSOR_GYROSCOPE:
		//dlog_print(DLOG_DEBUG, "sensor callback", "SENSOR_ACCELEROMETER on");
		timestamp = event->timestamp;

		// Print timestamp
		sprintf(buf,"[GYRO] time : %lld",timestamp);
		dlog_print(DLOG_DEBUG, "gyro callback", buf);

		// Currently comment out to suppress unused variable warning
		// int accuracy = event->accuracy;

		// Print accelerometer data
		x = event->values[0];
		y = event->values[1];
		z = event->values[2];
		sprintf(buf,"%f %f %f",x,y,z);
		dlog_print(DLOG_DEBUG, "gyro callback", buf);
		break;
	default:
		/* Do nothing */
		break;
	}
}

static void initialize_accelerometer(appdata_s *ad){
	// Create listener handler using sensor handler
	sensor_create_listener(default_accelerometer, &accel_listener);

	// Register callback function
	// BUG: Time intervals of the last 1~3 measurements are inaccurate
	sensor_listener_set_event_cb(accel_listener,
                                     SAMPLING_RATE,
                                     example_sensor_callback, NULL);
}

static void initialize_gyroscope(appdata_s *ad){
	// Create listener handler using sensor handler
	sensor_create_listener(default_gyroscope, &gyro_listener);

	// Register callback function
	// BUG: Time intervals of the last 1~3 measurements are inaccurate
	sensor_listener_set_event_cb(gyro_listener,
                                     SAMPLING_RATE,
                                     example_sensor_callback, NULL);
}

static void
win_delete_request_cb(void *data, Evas_Object *obj, void *event_info)
{
	ui_app_exit();
}

static void
win_back_cb(void *data, Evas_Object *obj, void *event_info)
{
	appdata_s *ad = data;
	/* Let window go to hide state. */
	elm_win_lower(ad->win);
}

static void turn_on_accelerometer(appdata_s *ad) {
	// Start Listener
	initialize_accelerometer(ad);
	sensor_listener_start(accel_listener);
	elm_object_text_set(ad->button, "Stop");
	ad->state = "on";

	// create sensor data array
	ad->sensor_data[ACCELEROMETER] = malloc(sizeof(sensordata_s)*SAMPLES_PER_SESOND);
}

static void turn_off_accelerometer(appdata_s *ad) {
	// Stop Listerner
	sensor_listener_stop(accel_listener);
	sensor_destroy_listener(accel_listener);
	elm_object_text_set(ad->button, "Start");
	ad->state = "off";

	// free sensor data array
	free(ad->sensor_data[ACCELEROMETER]);
}

static void turn_on_gyroscope(appdata_s *ad) {
	// Start Listener
	initialize_gyroscope(ad);
	sensor_listener_start(gyro_listener);
	elm_object_text_set(ad->button, "Stop");
	ad->state = "on";

	// create sensor data array
	ad->sensor_data[GYROSCOPE] = malloc(sizeof(sensordata_s)*SAMPLES_PER_SESOND);
}

static void turn_off_gyroscope(appdata_s *ad) {
	// Stop Listerner
	sensor_listener_stop(gyro_listener);
	sensor_destroy_listener(gyro_listener);
	elm_object_text_set(ad->button, "Start");
	ad->state = "off";

	// free sensor data array
	free(ad->sensor_data[GYROSCOPE]);
}

static void btn_clicked_cb(void *data, Evas_Object *obj, void *event_info) {
	appdata_s *ad = data;
	if(strcmp(ad->state, "off") == 0) {
		turn_on_accelerometer(ad);
		turn_on_gyroscope(ad);
	} else if (strcmp(ad->state, "on") == 0) {
		turn_off_accelerometer(ad);
		turn_off_gyroscope(ad);
	}
}

static void
create_base_gui(appdata_s *ad)
{
	/* Window */
	ad->win = elm_win_util_standard_add(PACKAGE, PACKAGE);
	elm_win_autodel_set(ad->win, EINA_TRUE);

	if (elm_win_wm_rotation_supported_get(ad->win)) {
		int rots[4] = { 0, 90, 180, 270 };
		elm_win_wm_rotation_available_rotations_set(ad->win, (const int *)(&rots), 4);
	}

	evas_object_smart_callback_add(ad->win, "delete,request", win_delete_request_cb, NULL);
	eext_object_event_callback_add(ad->win, EEXT_CALLBACK_BACK, win_back_cb, ad);

	/* Conformant */
	ad->conform = elm_conformant_add(ad->win);
	elm_win_indicator_mode_set(ad->win, ELM_WIN_INDICATOR_SHOW);
	elm_win_indicator_opacity_set(ad->win, ELM_WIN_INDICATOR_OPAQUE);
	evas_object_size_hint_weight_set(ad->conform, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_win_resize_object_add(ad->win, ad->conform);
	evas_object_show(ad->conform);

	/* Label */
	ad->label = elm_label_add(ad->conform);
	elm_object_text_set(ad->label, "<align=center><br>ACCELEROMETER</br></align>");
	evas_object_size_hint_weight_set(ad->label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_content_set(ad->conform, ad->label);
	evas_object_move(ad->label, 130, 130);

	/* Set button */
	ad->button = elm_button_add(ad->win);
	evas_object_smart_callback_add(ad->button, "clicked", btn_clicked_cb, ad);
	evas_object_move(ad->button, 130, 100);
	evas_object_resize(ad->button, 100, 60);
	evas_object_show(ad->button);

	// default state
	turn_off_accelerometer(ad);

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
	appdata_s *ad = data;
	create_base_gui(ad);
	//initial_sensor(); // initialize sensor

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

	appdata_s ad = {0,};
	int ret = 0;

	ui_app_lifecycle_callback_s event_callback = {0,};
	app_event_handler_h handlers[5] = {NULL, };

	bool supported[2] = { false, false };
	sensor_is_supported(SENSOR_ACCELEROMETER, &supported[ACCELEROMETER]);
	sensor_is_supported(SENSOR_GYROSCOPE, &supported[GYROSCOPE]);
	if (!supported[0] || !supported[1]) {
		//dlog_print(DLOG_DEBUG, "sensor", "sensor is not supported" );
		return 1;
		/* Accelerometer is not supported on the current device */
	}

    // Initialize sensor handle
	sensor_get_default_sensor(SENSOR_ACCELEROMETER, &default_accelerometer);
	sensor_get_default_sensor(SENSOR_GYROSCOPE, &default_gyroscope);

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
	ui_app_remove_event_handler(handlers[APP_EVENT_LOW_MEMORY]);

	ret = ui_app_main(argc, argv, &event_callback, &ad);
	if (ret != APP_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "app_main() is failed. err = %d", ret);
	}

	return ret;
}
