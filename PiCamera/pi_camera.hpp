#pragma once
#include <AL/Common.hpp>

#define PI_CAMERA_API_CALL AL_CDECL

#if defined(PI_CAMERA_API)
	#define PI_CAMERA_API_EXPORT AL_DLL_EXPORT
#elif defined(PI_CAMERA_BIN)
	#define PI_CAMERA_API_EXPORT
#else
	#define PI_CAMERA_API_EXPORT AL_DLL_IMPORT
#endif

struct pi_camera;

enum PI_CAMERA_EV : AL::int8
{
	PI_CAMERA_EV_MIN = -10,
	PI_CAMERA_EV_MAX = 10,

	PI_CAMERA_EV_DEFAULT = 0
};

enum PI_CAMERA_ISO : AL::uint16
{
	PI_CAMERA_ISO_0   = 0,
	PI_CAMERA_ISO_100 = 100,
	PI_CAMERA_ISO_200 = 200,
	PI_CAMERA_ISO_400 = 400,
	PI_CAMERA_ISO_800 = 800,

	PI_CAMERA_ISO_MIN = PI_CAMERA_ISO_0,
	PI_CAMERA_ISO_MAX = PI_CAMERA_ISO_800
};

enum PI_CAMERA_CONTRAST : AL::int8
{
	PI_CAMERA_CONTRAST_MIN = -100,
	PI_CAMERA_CONTRAST_MAX = 100,

	PI_CAMERA_CONTRAST_DEFAULT = 0
};

enum PI_CAMERA_SHARPNESS : AL::int8
{
	PI_CAMERA_SHARPNESS_MIN = -100,
	PI_CAMERA_SHARPNESS_MAX = 100,

	PI_CAMERA_SHARPNESS_DEFAULT = 0
};

enum PI_CAMERA_BRIGHTNESS : AL::uint8
{
	PI_CAMERA_BRIGHTNESS_MIN = 0,
	PI_CAMERA_BRIGHTNESS_MAX = 100,

	PI_CAMERA_BRIGHTNESS_DEFAULT = 50
};

enum PI_CAMERA_SATURATION : AL::int8
{
	PI_CAMERA_SATURATION_MIN = -100,
	PI_CAMERA_SATURATION_MAX = 100,

	PI_CAMERA_SATURATION_DEFAULT = 0
};

enum PI_CAMERA_WHITE_BALANCE : AL::uint8
{
	PI_CAMERA_WHITE_BALANCE_OFF,
	PI_CAMERA_WHITE_BALANCE_AUTO,
	PI_CAMERA_WHITE_BALANCE_SUN,
	PI_CAMERA_WHITE_BALANCE_FLASH,
	PI_CAMERA_WHITE_BALANCE_SHADE,
	PI_CAMERA_WHITE_BALANCE_CLOUDS,
	PI_CAMERA_WHITE_BALANCE_HORIZON,
	PI_CAMERA_WHITE_BALANCE_TUNGSTEN,
	PI_CAMERA_WHITE_BALANCE_FLUORESCENT,
	PI_CAMERA_WHITE_BALANCE_INCANDESCENT
};

enum PI_CAMERA_SHUTTER_SPEED : AL::uint64
{
	PI_CAMERA_SHUTTER_SPEED_AUTO = 0
};

enum PI_CAMERA_EXPOSURE_MODES : AL::uint8
{
	PI_CAMERA_EXPOSURE_MODE_OFF,
	PI_CAMERA_EXPOSURE_MODE_AUTO,
	PI_CAMERA_EXPOSURE_MODE_SNOW,
	PI_CAMERA_EXPOSURE_MODE_BEACH,
	PI_CAMERA_EXPOSURE_MODE_NIGHT,
	PI_CAMERA_EXPOSURE_MODE_SPORTS,
	PI_CAMERA_EXPOSURE_MODE_BACKLIGHT,
	PI_CAMERA_EXPOSURE_MODE_SPOTLIGHT,
	PI_CAMERA_EXPOSURE_MODE_VERY_LONG,
	PI_CAMERA_EXPOSURE_MODE_FIXED_FPS,
	PI_CAMERA_EXPOSURE_MODE_FIREWORKS,
	PI_CAMERA_EXPOSURE_MODE_ANTI_SHAKE,
	PI_CAMERA_EXPOSURE_MODE_NIGHT_PREVIEW
};

enum PI_CAMERA_METORING_MODES : AL::uint8
{
	PI_CAMERA_METORING_MODE_SPOT,
	PI_CAMERA_METORING_MODE_MATRIX,
	PI_CAMERA_METORING_MODE_AVERAGE,
	PI_CAMERA_METORING_MODE_BACKLIT
};

enum PI_CAMERA_JPG_QUALITY : AL::uint8
{
	PI_CAMERA_JPG_QUALITY_MIN = 0,
	PI_CAMERA_JPG_QUALITY_MAX = 100,

	PI_CAMERA_JPG_QUALITY_DEFAULT = 75
};

enum PI_CAMERA_IMAGE_SIZE : AL::uint16
{
	PI_CAMERA_IMAGE_SIZE_WIDTH_MAX  = 3280,
	PI_CAMERA_IMAGE_SIZE_HEIGHT_MAX = 2464
};

enum PI_CAMERA_IMAGE_EFFECTS : AL::uint8
{
	PI_CAMERA_IMAGE_EFFECT_NONE,
	PI_CAMERA_IMAGE_EFFECT_NEGATIVE,
	PI_CAMERA_IMAGE_EFFECT_SOLARISE,
	PI_CAMERA_IMAGE_EFFECT_WHITEBOARD,
	PI_CAMERA_IMAGE_EFFECT_BLACKBOARD,
	PI_CAMERA_IMAGE_EFFECT_SKETCH,
	PI_CAMERA_IMAGE_EFFECT_DENOISE,
	PI_CAMERA_IMAGE_EFFECT_EMBOSS,
	PI_CAMERA_IMAGE_EFFECT_OIL_PAINT,
	PI_CAMERA_IMAGE_EFFECT_GRAPHITE_SKETCH,
	PI_CAMERA_IMAGE_EFFECT_CROSS_HATCH_SKETCH,
	PI_CAMERA_IMAGE_EFFECT_PASTEL,
	PI_CAMERA_IMAGE_EFFECT_WATERCOLOR,
	PI_CAMERA_IMAGE_EFFECT_FILM,
	PI_CAMERA_IMAGE_EFFECT_BLUR,
	PI_CAMERA_IMAGE_EFFECT_SATURATE
};

enum PI_CAMERA_IMAGE_ROTATION : AL::uint16
{
	PI_CAMERA_IMAGE_ROTATION_MIN = 0,
	PI_CAMERA_IMAGE_ROTATION_MAX = 359,

	PI_CAMERA_IMAGE_ROTATION_DEFAULT = 0
};

enum PI_CAMERA_ERROR_CODES : AL::uint8
{
	PI_CAMERA_ERROR_CODE_SUCCESS,
	PI_CAMERA_ERROR_CODE_CAMERA_BUSY,
	PI_CAMERA_ERROR_CODE_CAMERA_FAILED,
	PI_CAMERA_ERROR_CODE_FILE_STAT_ERROR,
	PI_CAMERA_ERROR_CODE_FILE_READ_ERROR,
	PI_CAMERA_ERROR_CODE_FILE_WRITE_ERROR,
	PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED,

	PI_CAMERA_ERROR_CODE_UNDEFINED
};

struct pi_camera_config
{
	AL::int8     ev;
	AL::uint16   iso;
	AL::int8     contrast;
	AL::int8     sharpness;
	AL::int8     brightness;
	AL::int8     saturation;
	AL::uint8    white_balance;
	AL::TimeSpan shutter_speed;
	AL::uint8    exposure_mode;
	AL::uint8    metoring_mode;
	AL::uint8    jpg_quality;
	AL::uint8    image_effect;
	AL::uint16   image_rotation;
	AL::uint16   image_size_width;
	AL::uint16   image_size_height;
};

extern "C"
{
	PI_CAMERA_API_EXPORT const char* PI_CAMERA_API_CALL pi_camera_get_error_string(AL::uint8 value);

	PI_CAMERA_API_EXPORT pi_camera* PI_CAMERA_API_CALL  pi_camera_open();
	PI_CAMERA_API_EXPORT pi_camera* PI_CAMERA_API_CALL  pi_camera_open_remote(const char* remote_host, AL::uint16 remote_port);
	PI_CAMERA_API_EXPORT pi_camera* PI_CAMERA_API_CALL  pi_camera_open_service(const char* local_host, AL::uint16 local_port, AL::size_t max_connections);
	PI_CAMERA_API_EXPORT void       PI_CAMERA_API_CALL  pi_camera_close(pi_camera* camera);

	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_is_busy(pi_camera* camera, bool* value);
	PI_CAMERA_API_EXPORT bool       PI_CAMERA_API_CALL  pi_camera_is_remote(pi_camera* camera);
	PI_CAMERA_API_EXPORT bool       PI_CAMERA_API_CALL  pi_camera_is_service(pi_camera* camera);
	PI_CAMERA_API_EXPORT bool       PI_CAMERA_API_CALL  pi_camera_is_connected(pi_camera* camera);

	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_ev(pi_camera* camera, AL::int8* value);
	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_ev(pi_camera* camera, AL::int8 value);

	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_iso(pi_camera* camera, AL::uint16* value);
	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_iso(pi_camera* camera, AL::uint16 value);

	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_config(pi_camera* camera, pi_camera_config* value);
	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_config(pi_camera* camera, const pi_camera_config* value);

	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_contrast(pi_camera* camera, AL::int8* value);
	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_contrast(pi_camera* camera, AL::int8 value);

	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_sharpness(pi_camera* camera, AL::int8* value);
	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_sharpness(pi_camera* camera, AL::int8 value);

	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_brightness(pi_camera* camera, AL::uint8* value);
	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_brightness(pi_camera* camera, AL::uint8 value);

	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_saturation(pi_camera* camera, AL::int8* value);
	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_saturation(pi_camera* camera, AL::int8 value);

	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_white_balance(pi_camera* camera, AL::uint8* value);
	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_white_balance(pi_camera* camera, AL::uint8 value);

	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_shutter_speed(pi_camera* camera, AL::TimeSpan* value);
	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_shutter_speed(pi_camera* camera, AL::TimeSpan value);

	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_exposure_mode(pi_camera* camera, AL::uint8* value);
	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_exposure_mode(pi_camera* camera, AL::uint8 value);

	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_metoring_mode(pi_camera* camera, AL::uint8* value);
	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_metoring_mode(pi_camera* camera, AL::uint8 value);

	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_jpg_quality(pi_camera* camera, AL::uint8* value);
	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_jpg_quality(pi_camera* camera, AL::uint8 value);

	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_image_size(pi_camera* camera, AL::uint16* width, AL::uint16* height);
	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_image_size(pi_camera* camera, AL::uint16 width, AL::uint16 height);

	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_image_effect(pi_camera* camera, AL::uint8* value);
	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_image_effect(pi_camera* camera, AL::uint8 value);

	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_image_rotation(pi_camera* camera, AL::uint16* value);
	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_image_rotation(pi_camera* camera, AL::uint16 value);

	PI_CAMERA_API_EXPORT AL::uint8  PI_CAMERA_API_CALL  pi_camera_capture(pi_camera* camera, const char* file_path);
}