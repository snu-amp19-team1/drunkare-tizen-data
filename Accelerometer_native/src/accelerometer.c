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
#define DATA_WRITE_TIME		10 // sec

sensor_h sensors[NUM_OF_SENSOR];
sensor_listener_h listeners[NUM_OF_SENSOR];
const char *filepath;	// "/opt/usr/home/owner/apps_rw/org.example.accelerometer_4_0/data/"

/* struct for sensor data */
typedef struct sensordata {
	int index;						/* index of sensor_data array */
	int sensortype;					/* 0 : ACCELEROMETER , 1 : GYROSCOPE */
	int classification;				/* (not decided) */
	unsigned long long timestamp;	/* timestamp */
	float x, y, z;					/* data */
} sensordata_s;

/* appdata */
typedef struct appdata {
	Evas_Object *win;
	Evas_Object *conform;
	Evas_Object *label;
	Evas_Object *button;
	char *state;
	sensordata_s sensor_data[NUM_OF_SENSOR][SAMPLES_PER_SESOND*DATA_WRITE_TIME]; // to save data per second
	int iterator[NUM_OF_SENSOR]; // to save data per second
} appdata_s;



/* print sensordata struct */
void dlog_print_sensor_data(sensordata_s data) {
	char buf[64];
	sprintf(buf,"type : %d timestamp : %lld index : #%d", data.sensortype, data.timestamp, data.index);
	dlog_print(DLOG_DEBUG, "data_array", buf);
	sprintf(buf,"x : %f y : %f z : %f", data.x, data.y, data.z);
	dlog_print(DLOG_DEBUG, "data_array", buf);
}

/* string concatenate */
char* concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1) + strlen(s2) + 1); // +1 for the null-terminator
    // in real code you would check for errors in malloc here
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

/* write file in data path */
static void write_file(const char* filename, const char* buf)
{
    FILE *fp;
    fp = fopen(concat(filepath, filename), "w");
    fputs(buf,fp);
    fclose(fp);
}

/* read file in data path */
static void read_file(const char* filename)
{
    FILE *fp;
    char buf[255];
    fp = fopen(concat(filepath, filename), "r");
    fscanf(fp, "%s", buf);
    dlog_print(DLOG_DEBUG, "file_read", buf);
    fclose(fp);
}



/* Common callback function */
void sensor_callback(sensor_h sensor, sensor_event_s *event, void *user_data) {
	/*
	 If a callback is used to listen for different sensor types,
	 it can check the sensor type
	 */

	appdata_s *ad = user_data;
	sensordata_s *data;
	sensor_type_e type;
	sensor_get_type(sensor, &type);
	unsigned long long timestamp;
	char buf[64];
	float x, y, z;
	int sensor_index = 0;

	switch (type) {
		case SENSOR_ACCELEROMETER:
			sensor_index = ACCELEROMETER;
			break;
		case SENSOR_GYROSCOPE:
			sensor_index = GYROSCOPE;
			break;
		default:
			break;
	}

	// Print timestamp
	timestamp = event->timestamp;
	sprintf(buf,"[type : %d] time : %lld", sensor_index, timestamp);
	dlog_print(DLOG_DEBUG, "sensor_callback", buf);

	// Print data
	x = event->values[0];
	y = event->values[1];
	z = event->values[2];
	sprintf(buf,"[type : %d] %f %f %f",sensor_index, x, y, z);
	dlog_print(DLOG_DEBUG, "sensor_callback", buf);

	// record to array
	data = &(ad->sensor_data[sensor_index][ad->iterator[sensor_index]++]);
	data->index = ad->iterator[sensor_index];
	data->sensortype = sensor_index;
	data->timestamp = timestamp;
	data->x = x;
	data->y = y;
	data->z = z;

	// test print
	dlog_print_sensor_data(*data);

	// reset iterator
	if (ad->iterator[sensor_index] == SAMPLES_PER_SESOND*DATA_WRITE_TIME){
		ad->iterator[sensor_index] = 0;
		sprintf(buf,"[type : %d] timestamp 1sec : %lld", sensor_index , ad->sensor_data[sensor_index][SAMPLES_PER_SESOND*DATA_WRITE_TIME-1].timestamp - ad->sensor_data[sensor_index][0].timestamp);
		dlog_print(DLOG_DEBUG, "sensor_timestamp", buf);
	}

	// save file
	const char* data_buf = "test";
	write_file("data.txt", data_buf);

	// read file for test
	// => file write works well, but can't see the file in device manager.... why???
	read_file("data.txt");
}

static void initialize_sensor(appdata_s *ad, int sensor_index) {
	// Create listener handler using sensor handler
	sensor_create_listener(sensors[sensor_index], &listeners[sensor_index]);

	// Register callback function
	// BUG: Time intervals of the last 1~3 measurements are inaccurate
	sensor_listener_set_event_cb(listeners[sensor_index],
	                             SAMPLING_RATE,
	                             sensor_callback, ad);
}

static void turn_on_sensor(appdata_s *ad, int sensor_index) {
	// Start Listener
	initialize_sensor(ad, sensor_index);
	sensor_listener_start(listeners[sensor_index]);
	elm_object_text_set(ad->button, "Stop");
	ad->state = "on";
	ad->iterator[sensor_index] = 0;
}

static void turn_off_sensor(appdata_s *ad, int sensor_index) {
	// Stop Listener
	sensor_listener_stop(listeners[sensor_index]);
	sensor_destroy_listener(listeners[sensor_index]);
	elm_object_text_set(ad->button, "Start");
	ad->state = "off";
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

static void btn_clicked_cb(void *data, Evas_Object *obj, void *event_info) {
	appdata_s *ad = data;
	if(strcmp(ad->state, "off") == 0) {
		turn_on_sensor(ad, ACCELEROMETER);
		turn_on_sensor(ad, GYROSCOPE);
	} else if (strcmp(ad->state, "on") == 0) {
		turn_off_sensor(ad, ACCELEROMETER);
		turn_off_sensor(ad, GYROSCOPE);
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
	elm_object_text_set(ad->button, "Start");
	ad->state = "off";

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
	filepath = app_get_data_path();

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
	sensor_get_default_sensor(SENSOR_ACCELEROMETER, &sensors[ACCELEROMETER]);
	sensor_get_default_sensor(SENSOR_GYROSCOPE, &sensors[GYROSCOPE]);

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
