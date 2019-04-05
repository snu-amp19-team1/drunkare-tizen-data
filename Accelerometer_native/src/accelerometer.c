#include <accelerometer.h>
#include <sensor.h>
#include <app.h>
#include "dlog.h"
#include <Elementary.h>
#include <device/power.h>

#define NUM_OF_SENSOR		2
#define ACCELEROMETER		0
#define GYROSCOPE			1
#define MILLION				1000000
#define SAMPLING_RATE		40
#define SAMPLES_PER_SESOND	(1000 / SAMPLING_RATE)
#define DATA_WRITE_TIME		10 // sec
#define NUM_ACTIVITIES		12

// TODO: Replace with real label name
#define ACTIVITY_0		0
#define ACTIVITY_1		1
#define ACTIVITY_2		2
#define ACTIVITY_3		3
#define ACTIVITY_4		4
#define ACTIVITY_5		5
#define ACTIVITY_6		6
#define ACTIVITY_7		7
#define ACTIVITY_8		8
#define ACTIVITY_9		9
#define ACTIVITY_10		10
#define ACTIVITY_11		11

// Define button size
#define BTN_H		60
#define BTN_W		80

typedef struct pos {
    int x;
    int y;
} pos_t;

// Define button location
pos_t button_pos[] = {
    [ACTIVITY_0]	= {40, 30},
    [ACTIVITY_1]	= {140, 30},
    [ACTIVITY_2]	= {240, 30},
    [ACTIVITY_3]	= {40, 110},
    [ACTIVITY_4]	= {140, 110},
    [ACTIVITY_5]	= {240, 110},
    [ACTIVITY_6]	= {40, 190},
    [ACTIVITY_7]	= {140, 190},
    [ACTIVITY_8]	= {240, 190},
    [ACTIVITY_9]	= {40, 270},
    [ACTIVITY_10]	= {140, 270},
    [ACTIVITY_11]	= {240, 270}
};

#define BTN_X(a)	(button_pos[a].x)
#define BTN_Y(a)	(button_pos[a].y)

// Convert activity number to button object pointer
#define activity2btn(ad, a) (Evas_Object *)(ad->buttons[a])

sensor_h sensors[NUM_OF_SENSOR];
sensor_listener_h listeners[NUM_OF_SENSOR];
const char *filepath;	// "/opt/usr/home/owner/apps_rw/org.example.accelerometer_4_0/data/"

/* struct for sensor data */
typedef struct sensordata {
	int index;						/* index of sensor_data array */
	int sensortype;					/* 0 : ACCELEROMETER , 1 : GYROSCOPE */
	int activity;					/* (not decided) */
	unsigned long long timestamp;	/* timestamp */
	float x, y, z;					/* data */
} sensordata_s;

/* appdata */
typedef struct appdata {
	Evas_Object *win;
	Evas_Object *conform;
	Evas_Object *label;
	Evas_Object *buttons[NUM_ACTIVITIES];
	char *state;
	sensordata_s sensor_data[NUM_OF_SENSOR][SAMPLES_PER_SESOND*DATA_WRITE_TIME]; // to save data per second
    int activity; // FIXME
	int iterator[NUM_OF_SENSOR]; // to save data per second
} appdata_s;

// Button callbacks
// FIXME!
static void btn_cb0(void *data, Evas_Object *obj, void *event_info);
static void btn_cb1(void *data, Evas_Object *obj, void *event_info);
static void btn_cb2(void *data, Evas_Object *obj, void *event_info);
static void btn_cb3(void *data, Evas_Object *obj, void *event_info);
static void btn_cb4(void *data, Evas_Object *obj, void *event_info);
static void btn_cb5(void *data, Evas_Object *obj, void *event_info);
static void btn_cb6(void *data, Evas_Object *obj, void *event_info);
static void btn_cb7(void *data, Evas_Object *obj, void *event_info);
static void btn_cb8(void *data, Evas_Object *obj, void *event_info);
static void btn_cb9(void *data, Evas_Object *obj, void *event_info);
static void btn_cb10(void *data, Evas_Object *obj, void *event_info);
static void btn_cb11(void *data, Evas_Object *obj, void *event_info);

typedef void* (*btn_cb_t)(void*, Evas_Object*, void*);

btn_cb_t btn_cb[] = {
    [ACTIVITY_0]	= &btn_cb0,
    [ACTIVITY_1]	= &btn_cb1,
    [ACTIVITY_2]	= &btn_cb2,
    [ACTIVITY_3]	= &btn_cb3,
    [ACTIVITY_4]	= &btn_cb4,
    [ACTIVITY_5]	= &btn_cb5,
    [ACTIVITY_6]	= &btn_cb6,
    [ACTIVITY_7]	= &btn_cb7,
    [ACTIVITY_8]	= &btn_cb8,
    [ACTIVITY_9]	= &btn_cb9,
    [ACTIVITY_10]	= &btn_cb10,
    [ACTIVITY_11]	= &btn_cb11
};


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

static void turn_on_sensor(appdata_s *ad, Evas_Object* obj, int sensor_index) {
	// Start Listener
	initialize_sensor(ad, sensor_index);
	sensor_listener_start(listeners[sensor_index]);
	elm_object_text_set(obj, "Stop");
	ad->state = "on";
	ad->iterator[sensor_index] = 0;
}

static void turn_off_sensor(appdata_s *ad, Evas_Object* obj, int sensor_index) {
	// Stop Listener
	sensor_listener_stop(listeners[sensor_index]);
	sensor_destroy_listener(listeners[sensor_index]);
	elm_object_text_set(obj, "Start");
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

    dlog_print(DLOG_DEBUG, "activity", "%d", ad->activity);

	if(strcmp(ad->state, "off") == 0) {
		turn_on_sensor(ad, obj, ACCELEROMETER);
		turn_on_sensor(ad, obj, GYROSCOPE);
	} else if (strcmp(ad->state, "on") == 0) {
		turn_off_sensor(ad, obj, ACCELEROMETER);
		turn_off_sensor(ad, obj, GYROSCOPE);
	}
}

static void btn_cb0(void *data, Evas_Object *obj, void *event_info) {
    appdata_s *ad = (appdata_s*) data;
    ad->activity = ACTIVITY_0;
    btn_clicked_cb(data, obj, event_info);
}
static void btn_cb1(void *data, Evas_Object *obj, void *event_info) {
    appdata_s *ad = (appdata_s*) data;
    ad->activity = ACTIVITY_1;
    btn_clicked_cb(data, obj, event_info);
}
static void btn_cb2(void *data, Evas_Object *obj, void *event_info) {
    appdata_s *ad = (appdata_s*) data;
    ad->activity = ACTIVITY_2;
    btn_clicked_cb(data, obj, event_info);
}
static void btn_cb3(void *data, Evas_Object *obj, void *event_info) {
    appdata_s *ad = (appdata_s*) data;
    ad->activity = ACTIVITY_3;
    btn_clicked_cb(data, obj, event_info);
}
static void btn_cb4(void *data, Evas_Object *obj, void *event_info) {
    appdata_s *ad = (appdata_s*) data;
    ad->activity = ACTIVITY_4;
    btn_clicked_cb(data, obj, event_info);
}
static void btn_cb5(void *data, Evas_Object *obj, void *event_info) {
    appdata_s *ad = (appdata_s*) data;
    ad->activity = ACTIVITY_5;
    btn_clicked_cb(data, obj, event_info);
}
static void btn_cb6(void *data, Evas_Object *obj, void *event_info) {
    appdata_s *ad = (appdata_s*) data;
    ad->activity = ACTIVITY_6;
    btn_clicked_cb(data, obj, event_info);
}
static void btn_cb7(void *data, Evas_Object *obj, void *event_info) {
    appdata_s *ad = (appdata_s*) data;
    ad->activity = ACTIVITY_7;
    btn_clicked_cb(data, obj, event_info);
}
static void btn_cb8(void *data, Evas_Object *obj, void *event_info) {
    appdata_s *ad = (appdata_s*) data;
    ad->activity = ACTIVITY_8;
    btn_clicked_cb(data, obj, event_info);
}
static void btn_cb9(void *data, Evas_Object *obj, void *event_info) {
    appdata_s *ad = (appdata_s*) data;
    ad->activity = ACTIVITY_9;
    btn_clicked_cb(data, obj, event_info);
}
static void btn_cb10(void *data, Evas_Object *obj, void *event_info) {
    appdata_s *ad = (appdata_s*) data;
    ad->activity = ACTIVITY_10;
    btn_clicked_cb(data, obj, event_info);
}
static void btn_cb11(void *data, Evas_Object *obj, void *event_info) {
    appdata_s *ad = (appdata_s*) data;
    ad->activity = ACTIVITY_11;
    btn_clicked_cb(data, obj, event_info);
}

static void
init_button(appdata_s *ad,
            void (*cb)(void *data, Evas_Object *obj, void *event_info),
            int a)
{
    if (a >= NUM_ACTIVITIES)
        return;

    ad->buttons[a] = elm_button_add(ad->win);
	evas_object_smart_callback_add(ad->buttons[a], "clicked", cb, ad);
	evas_object_move(ad->buttons[a], BTN_X(a), BTN_Y(a));
	evas_object_resize(ad->buttons[a], BTN_W, BTN_H);
	evas_object_show(ad->buttons[a]);
}

static void
create_base_gui(appdata_s *ad)
{
    int a;

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
	elm_object_text_set(ad->label, "");
	evas_object_size_hint_weight_set(ad->label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_content_set(ad->conform, ad->label);
	evas_object_move(ad->label, 130, 130);

    for (a = 0; a < NUM_ACTIVITIES; ++a) {
        init_button(ad, btn_cb[a], a);
    }

	// default state
	// elm_object_text_set(ad->button, "Start");
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

	// Lock CPU power
	device_power_request_lock(POWER_LOCK_CPU, 0);

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
