#include "pi_camera.hpp"

#include <AL/OS/Console.hpp>

#include <AL/Collections/LinkedList.hpp>

enum PI_CAMERA_VERBS : AL::uint8
{
	PI_CAMERA_VERB_OPEN,
	PI_CAMERA_VERB_START,
	PI_CAMERA_VERB_CONNECT
};

enum PI_CAMERA_CONSOLE_COMMANDS : AL::uint8
{
	                                                // Input     Output    Arg0          Arg1                           Arg2  Arg3
	PI_CAMERA_CONSOLE_COMMAND_HELP,                 // void      void      help
	PI_CAMERA_CONSOLE_COMMAND_EXIT,                 // void      void      exit
	PI_CAMERA_CONSOLE_COMMAND_IS_BUSY,              // void      bool      is            busy
	PI_CAMERA_CONSOLE_COMMAND_IS_REMOTE,            // void      bool      is            remote
	PI_CAMERA_CONSOLE_COMMAND_IS_SERVICE,           // void      bool      is            service
	PI_CAMERA_CONSOLE_COMMAND_IS_CONNECTED,         // void      bool      is            connected
	PI_CAMERA_CONSOLE_COMMAND_GET_EV,               // void      int8      get           e|ev
	PI_CAMERA_CONSOLE_COMMAND_SET_EV,               // int8      void      set           e|ev                           value
	PI_CAMERA_CONSOLE_COMMAND_GET_ISO,              // void      uint16    get           i|iso
	PI_CAMERA_CONSOLE_COMMAND_SET_ISO,              // uint16    uint8     set           i|iso                          value
	PI_CAMERA_CONSOLE_COMMAND_GET_CONFIG,           // void      *         get           config
	PI_CAMERA_CONSOLE_COMMAND_GET_CONTRAST,         // void      int8      get           c|contrast
	PI_CAMERA_CONSOLE_COMMAND_SET_CONTRAST,         // int8      void      set           c|contrast                     value
	PI_CAMERA_CONSOLE_COMMAND_GET_SHARPNESS,        // void      int8      get           sh|sharpness
	PI_CAMERA_CONSOLE_COMMAND_SET_SHARPNESS,        // int8      void      set           sh|sharpness                   value
	PI_CAMERA_CONSOLE_COMMAND_GET_BRIGHTNESS,       // void      uint8     get           br|brightness
	PI_CAMERA_CONSOLE_COMMAND_SET_BRIGHTNESS,       // uint8     void      set           br|brightness                  value
	PI_CAMERA_CONSOLE_COMMAND_GET_SATURATION,       // void      int8      get           sat|saturation
	PI_CAMERA_CONSOLE_COMMAND_SET_SATURATION,       // int8      void      set           sat|saturation                 value
	PI_CAMERA_CONSOLE_COMMAND_GET_WHITE_BALANCE,    // void      uint8     get           wb|white_balance
	PI_CAMERA_CONSOLE_COMMAND_SET_WHITE_BALANCE,    // uint8     void      set           wb|white_balance               value
	PI_CAMERA_CONSOLE_COMMAND_GET_SHUTTER_SPEED,    // void      uint64    get           ss|shutter|shutter_speed
	PI_CAMERA_CONSOLE_COMMAND_SET_SHUTTER_SPEED,    // uint64    void      set           ss|shutter|shutter_speed       value
	PI_CAMERA_CONSOLE_COMMAND_GET_EXPOSURE_MODE,    // void      uint8     get           em|exposure|exposure_mode
	PI_CAMERA_CONSOLE_COMMAND_SET_EXPOSURE_MODE,    // uint8     void      set           em|exposure|exposure_mode      value
	PI_CAMERA_CONSOLE_COMMAND_GET_METORING_MODE,    // void      uint8     get           mm|metoring|metoring_mode
	PI_CAMERA_CONSOLE_COMMAND_SET_METORING_MODE,    // uint8     void      set           mm|metoring|metoring_mode      value
	PI_CAMERA_CONSOLE_COMMAND_GET_JPG_QUALITY,      // void      uint8     get           jq|quality|jpg_quality
	PI_CAMERA_CONSOLE_COMMAND_SET_JPG_QUALITY,      // uint8     void      set           jq|quality|jpg_quality         value
	PI_CAMERA_CONSOLE_COMMAND_GET_IMAGE_SIZE,       // void      uint16[2] get           is|size|image_size
	PI_CAMERA_CONSOLE_COMMAND_SET_IMAGE_SIZE,       // uint16[2] void      set           is|size|image_size             width height
	PI_CAMERA_CONSOLE_COMMAND_GET_IMAGE_EFFECT,     // void      uint8     get           ie|effect|image_effect
	PI_CAMERA_CONSOLE_COMMAND_SET_IMAGE_EFFECT,     // uint8     void      set           ie|effect|image_effect         value
	PI_CAMERA_CONSOLE_COMMAND_GET_IMAGE_ROTATION,   // void      uint16    get           ir|rot|rotation|image_rotation
	PI_CAMERA_CONSOLE_COMMAND_SET_IMAGE_ROTATION,   // uint16    void      set           ir|rot|rotation|image_rotation value
	PI_CAMERA_CONSOLE_COMMAND_GET_VIDEO_BIT_RATE,   // void      uint32    get           vbr|video_bit_rate
	PI_CAMERA_CONSOLE_COMMAND_SET_VIDEO_BIT_RATE,   // uint32    void      set           vbr|video_bit_rate             value
	PI_CAMERA_CONSOLE_COMMAND_GET_VIDEO_FRAME_RATE, // void      uint8     get           vfr|video_frame_rate
	PI_CAMERA_CONSOLE_COMMAND_SET_VIDEO_FRAME_RATE, // uint8     void      set           vfr|video_frame_rate           value
	PI_CAMERA_CONSOLE_COMMAND_CAPTURE,              // string    void      capture       "/path/to/destination/file"
	PI_CAMERA_CONSOLE_COMMAND_CAPTURE_VIDEO,        // string    void      capture_video duration                      "/path/to/destination/file"

	PI_CAMERA_CONSOLE_COMMAND_COUNT
};

struct pi_camera_args
{
	AL::uint8  verb;
	AL::String host;
	AL::uint16 port;
	AL::size_t max_connections;
};

struct pi_camera_console_command
{
	AL::uint8  type;

	struct
	{
		AL::String string;

		union
		{
			AL::int8   int8;
			AL::uint8  uint8;
			AL::uint16 uint16;
			AL::uint16 uint16_2[2];
			AL::uint32 uint32;
			AL::uint64 uint64;
		};
	} args;
};

struct pi_camera_console_command_result
{
	AL::Collections::LinkedList<AL::String> lines;
};

typedef AL::uint8(*pi_camera_console_command_handler)(const pi_camera_console_command& command, pi_camera_console_command_result& command_result);

struct pi_camera_console_command_context
{
	AL::uint8                         type;
	pi_camera_console_command_handler handler;
	const char*                       description;
};

extern const pi_camera_console_command_context CONSOLE_COMMANDS[PI_CAMERA_CONSOLE_COMMAND_COUNT];

const char* pi_camera_console_command_to_string(AL::uint8 value)
{
	switch (value)
	{
		case PI_CAMERA_CONSOLE_COMMAND_HELP:               return "help";
		case PI_CAMERA_CONSOLE_COMMAND_EXIT:               return "exit";
		case PI_CAMERA_CONSOLE_COMMAND_IS_BUSY:            return "is_busy";
		case PI_CAMERA_CONSOLE_COMMAND_IS_REMOTE:          return "is_remote";
		case PI_CAMERA_CONSOLE_COMMAND_IS_SERVICE:         return "is_service";
		case PI_CAMERA_CONSOLE_COMMAND_IS_CONNECTED:       return "is_connected";
		case PI_CAMERA_CONSOLE_COMMAND_GET_EV:             return "get_ev";
		case PI_CAMERA_CONSOLE_COMMAND_SET_EV:             return "set_ev";
		case PI_CAMERA_CONSOLE_COMMAND_GET_ISO:            return "get_iso";
		case PI_CAMERA_CONSOLE_COMMAND_SET_ISO:            return "set_iso";
		case PI_CAMERA_CONSOLE_COMMAND_GET_CONFIG:         return "get_config";
		case PI_CAMERA_CONSOLE_COMMAND_GET_CONTRAST:       return "get_contrast";
		case PI_CAMERA_CONSOLE_COMMAND_SET_CONTRAST:       return "set_contrast";
		case PI_CAMERA_CONSOLE_COMMAND_GET_SHARPNESS:      return "get_sharpness";
		case PI_CAMERA_CONSOLE_COMMAND_SET_SHARPNESS:      return "set_sharpness";
		case PI_CAMERA_CONSOLE_COMMAND_GET_BRIGHTNESS:     return "get_brightness";
		case PI_CAMERA_CONSOLE_COMMAND_SET_BRIGHTNESS:     return "set_brightness";
		case PI_CAMERA_CONSOLE_COMMAND_GET_SATURATION:     return "get_saturation";
		case PI_CAMERA_CONSOLE_COMMAND_SET_SATURATION:     return "set_saturation";
		case PI_CAMERA_CONSOLE_COMMAND_GET_WHITE_BALANCE:  return "get_white_balance";
		case PI_CAMERA_CONSOLE_COMMAND_SET_WHITE_BALANCE:  return "set_white_balance";
		case PI_CAMERA_CONSOLE_COMMAND_GET_SHUTTER_SPEED:  return "get_shutter_speed";
		case PI_CAMERA_CONSOLE_COMMAND_SET_SHUTTER_SPEED:  return "set_shutter_speed";
		case PI_CAMERA_CONSOLE_COMMAND_GET_EXPOSURE_MODE:  return "get_exposure_mode";
		case PI_CAMERA_CONSOLE_COMMAND_SET_EXPOSURE_MODE:  return "set_exposure_mode";
		case PI_CAMERA_CONSOLE_COMMAND_GET_METORING_MODE:  return "get_metoring_mode";
		case PI_CAMERA_CONSOLE_COMMAND_SET_METORING_MODE:  return "set_metoring_mode";
		case PI_CAMERA_CONSOLE_COMMAND_GET_JPG_QUALITY:    return "get_jpg_quality";
		case PI_CAMERA_CONSOLE_COMMAND_SET_JPG_QUALITY:    return "set_jpg_quality";
		case PI_CAMERA_CONSOLE_COMMAND_GET_IMAGE_SIZE:     return "get_image_size";
		case PI_CAMERA_CONSOLE_COMMAND_SET_IMAGE_SIZE:     return "set_image_size";
		case PI_CAMERA_CONSOLE_COMMAND_GET_IMAGE_EFFECT:   return "get_image_effect";
		case PI_CAMERA_CONSOLE_COMMAND_SET_IMAGE_EFFECT:   return "set_image_effect";
		case PI_CAMERA_CONSOLE_COMMAND_GET_IMAGE_ROTATION: return "get_image_rotation";
		case PI_CAMERA_CONSOLE_COMMAND_SET_IMAGE_ROTATION: return "set_image_rotation";
		case PI_CAMERA_CONSOLE_COMMAND_CAPTURE:            return "capture";
	}

	return "undefined";
}
bool        pi_camera_console_command_from_string(AL::uint8& value, const AL::String& arg0, const AL::String& arg1)
{
	if (arg0.Compare('x') || arg0.Compare("exit", AL::True))
	{
		value = PI_CAMERA_CONSOLE_COMMAND_EXIT;
		return true;
	}
	else if (arg0.Compare('q') || arg0.Compare("quit", AL::True))
	{
		value = PI_CAMERA_CONSOLE_COMMAND_EXIT;
		return true;
	}
	else if (arg0.Compare("help", AL::True))
	{
		value = PI_CAMERA_CONSOLE_COMMAND_HELP;
		return true;
	}
	else if (arg0.Compare("is", AL::True))
	{
		if (arg1.Compare("busy", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_IS_BUSY;
			return true;
		}
		else if (arg1.Compare("remote", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_IS_REMOTE;
			return true;
		}
		else if (arg1.Compare("service", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_IS_SERVICE;
			return true;
		}
		else if (arg1.Compare("connected", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_IS_CONNECTED;
			return true;
		}
	}
	else if (arg0.Compare("get", AL::True))
	{
		if (arg1.Compare('e', AL::True) || arg1.Compare("ev", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_GET_EV;
			return true;
		}
		else if (arg1.Compare('i', AL::True) || arg1.Compare("iso", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_GET_ISO;
			return true;
		}
		else if (arg1.Compare("config", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_GET_CONFIG;
			return true;
		}
		else if (arg1.Compare('c', AL::True) || arg1.Compare("contrast", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_GET_CONTRAST;
			return true;
		}
		else if (arg1.Compare("sh", AL::True) || arg1.Compare("sharpness", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_GET_SHARPNESS;
			return true;
		}
		else if (arg1.Compare("br", AL::True) || arg1.Compare("brightness", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_GET_BRIGHTNESS;
			return true;
		}
		else if (arg1.Compare("sat", AL::True) || arg1.Compare("saturation", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_GET_SATURATION;
			return true;
		}
		else if (arg1.Compare("wb", AL::True) || arg1.Compare("white_balance", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_GET_WHITE_BALANCE;
			return true;
		}
		else if (arg1.Compare("ss", AL::True) || arg1.Compare("shutter", AL::True) || arg1.Compare("shutter_speed", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_GET_SHUTTER_SPEED;
			return true;
		}
		else if (arg1.Compare("em", AL::True) || arg1.Compare("exposure", AL::True) || arg1.Compare("exposure_mode", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_GET_EXPOSURE_MODE;
			return true;
		}
		else if (arg1.Compare("mm", AL::True) || arg1.Compare("metoring", AL::True) || arg1.Compare("metoring_mode", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_GET_METORING_MODE;
			return true;
		}
		else if (arg1.Compare("jq", AL::True) || arg1.Compare("quality", AL::True) || arg1.Compare("jpg_quality", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_GET_JPG_QUALITY;
			return true;
		}
		else if (arg1.Compare("is", AL::True) || arg1.Compare("size", AL::True) || arg1.Compare("image_size", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_GET_IMAGE_SIZE;
			return true;
		}
		else if (arg1.Compare("ie", AL::True) || arg1.Compare("effect", AL::True) || arg1.Compare("image_effect", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_GET_IMAGE_EFFECT;
			return true;
		}
		else if (arg1.Compare("ir", AL::True) || arg1.Compare("rot", AL::True) || arg1.Compare("rotation", AL::True) || arg1.Compare("image_rotation", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_GET_IMAGE_ROTATION;
			return true;
		}
		else if (arg1.Compare("vbr", AL::True) || arg1.Compare("video_bit_rate", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_GET_VIDEO_BIT_RATE;
			return true;
		}
		else if (arg1.Compare("vfr", AL::True) || arg1.Compare("video_frame_rate", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_GET_VIDEO_FRAME_RATE;
			return true;
		}
	}
	else if (arg0.Compare("set", AL::True))
	{
		if (arg1.Compare('e', AL::True) || arg1.Compare("ev", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_SET_EV;
			return true;
		}
		else if (arg1.Compare('i', AL::True) || arg1.Compare("iso", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_SET_ISO;
			return true;
		}
		else if (arg1.Compare("config", AL::True))
		{
			// value = PI_CAMERA_CONSOLE_COMMAND_SET_CONFIG;
			// return true;
		}
		else if (arg1.Compare('c', AL::True) || arg1.Compare("contrast", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_SET_CONTRAST;
			return true;
		}
		else if (arg1.Compare("sh", AL::True) || arg1.Compare("sharpness", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_SET_SHARPNESS;
			return true;
		}
		else if (arg1.Compare("br", AL::True) || arg1.Compare("brightness", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_SET_BRIGHTNESS;
			return true;
		}
		else if (arg1.Compare("sat", AL::True) || arg1.Compare("saturation", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_SET_SATURATION;
			return true;
		}
		else if (arg1.Compare("wb", AL::True) || arg1.Compare("white_balance", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_SET_WHITE_BALANCE;
			return true;
		}
		else if (arg1.Compare("ss", AL::True) || arg1.Compare("shutter", AL::True) || arg1.Compare("shutter_speed", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_SET_SHUTTER_SPEED;
			return true;
		}
		else if (arg1.Compare("em", AL::True) || arg1.Compare("exposure", AL::True) || arg1.Compare("exposure_mode", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_SET_EXPOSURE_MODE;
			return true;
		}
		else if (arg1.Compare("mm", AL::True) || arg1.Compare("metoring", AL::True) || arg1.Compare("metoring_mode", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_SET_METORING_MODE;
			return true;
		}
		else if (arg1.Compare("jq", AL::True) || arg1.Compare("quality", AL::True) || arg1.Compare("jpg_quality", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_SET_JPG_QUALITY;
			return true;
		}
		else if (arg1.Compare("is", AL::True) || arg1.Compare("size", AL::True) || arg1.Compare("image_size", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_SET_IMAGE_SIZE;
			return true;
		}
		else if (arg1.Compare("ie", AL::True) || arg1.Compare("effect", AL::True) || arg1.Compare("image_effect", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_SET_IMAGE_EFFECT;
			return true;
		}
		else if (arg1.Compare("ir", AL::True) || arg1.Compare("rot", AL::True) || arg1.Compare("rotation", AL::True) || arg1.Compare("image_rotation", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_SET_IMAGE_ROTATION;
			return true;
		}
		else if (arg1.Compare("vbr", AL::True) || arg1.Compare("video_bit_rate", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_SET_VIDEO_BIT_RATE;
			return true;
		}
		else if (arg1.Compare("vfr", AL::True) || arg1.Compare("video_frame_rate", AL::True))
		{
			value = PI_CAMERA_CONSOLE_COMMAND_SET_VIDEO_FRAME_RATE;
			return true;
		}
	}
	else if (arg0.Compare("capture", AL::True))
	{
		value = PI_CAMERA_CONSOLE_COMMAND_CAPTURE;
		return true;
	}
	else if (arg0.Compare("capture_video", AL::True))
	{
		value = PI_CAMERA_CONSOLE_COMMAND_CAPTURE_VIDEO;
		return true;
	}

	return false;
}
bool        pi_camera_console_command_args_from_string(pi_camera_console_command& value, const AL::String* args, AL::size_t arg_count)
{
	switch (value.type)
	{
		case PI_CAMERA_CONSOLE_COMMAND_HELP:
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_IS_BUSY:
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_IS_REMOTE:
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_IS_SERVICE:
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_IS_CONNECTED:
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_GET_EV:
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_SET_EV:
			if (arg_count < 2) return false;
			value.args.int8 = AL::FromString<AL::int8>(args[2]);
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_GET_ISO:
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_SET_ISO:
			if (arg_count < 2) return false;
			value.args.uint16 = AL::FromString<AL::uint16>(args[2]);
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_GET_CONFIG:
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_GET_CONTRAST:
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_SET_CONTRAST:
			if (arg_count < 2) return false;
			value.args.int8 = AL::FromString<AL::int8>(args[2]);
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_GET_SHARPNESS:
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_SET_SHARPNESS:
			if (arg_count < 2) return false;
			value.args.int8 = AL::FromString<AL::int8>(args[2]);
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_GET_BRIGHTNESS:
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_SET_BRIGHTNESS:
			if (arg_count < 2) return false;
			value.args.uint8 = AL::FromString<AL::uint8>(args[2]);
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_GET_SATURATION:
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_SET_SATURATION:
			if (arg_count < 2) return false;
			value.args.int8 = AL::FromString<AL::int8>(args[2]);
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_GET_WHITE_BALANCE:
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_SET_WHITE_BALANCE:
			if (arg_count < 2) return false;
			value.args.uint8 = AL::FromString<AL::uint8>(args[2]);
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_GET_SHUTTER_SPEED:
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_SET_SHUTTER_SPEED:
			if (arg_count < 2) return false;
			value.args.uint64 = AL::FromString<AL::uint64>(args[2]);
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_GET_EXPOSURE_MODE:
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_SET_EXPOSURE_MODE:
			if (arg_count < 2) return false;
			value.args.uint8 = AL::FromString<AL::uint8>(args[2]);
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_GET_METORING_MODE:
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_SET_METORING_MODE:
			if (arg_count < 2) return false;
			value.args.uint8 = AL::FromString<AL::uint8>(args[2]);
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_GET_JPG_QUALITY:
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_SET_JPG_QUALITY:
			if (arg_count < 2) return false;
			value.args.uint8 = AL::FromString<AL::uint8>(args[2]);
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_GET_IMAGE_SIZE:
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_SET_IMAGE_SIZE:
			if (arg_count < 3) return false;
			value.args.uint16_2[0] = AL::FromString<AL::uint16>(args[2]);
			value.args.uint16_2[1] = AL::FromString<AL::uint16>(args[3]);
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_GET_IMAGE_EFFECT:
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_SET_IMAGE_EFFECT:
			if (arg_count < 2) return false;
			value.args.uint8 = AL::FromString<AL::uint8>(args[2]);
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_GET_IMAGE_ROTATION:
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_SET_IMAGE_ROTATION:
			if (arg_count < 2) return false;
			value.args.uint16 = AL::FromString<AL::uint16>(args[2]);
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_GET_VIDEO_BIT_RATE:
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_SET_VIDEO_BIT_RATE:
			if (arg_count < 2) return false;
			value.args.uint32 = AL::FromString<AL::uint32>(args[2]);
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_GET_VIDEO_FRAME_RATE:
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_SET_VIDEO_FRAME_RATE:
			if (arg_count < 2) return false;
			value.args.uint8 = AL::FromString<AL::uint8>(args[2]);
			return true;

		case PI_CAMERA_CONSOLE_COMMAND_CAPTURE:
		{
			if (arg_count < 2)
				return false;

			for (AL::size_t i = 1; i < arg_count; ++i)
				value.args.string.Append(args[i]);
		}
		return true;

		case PI_CAMERA_CONSOLE_COMMAND_CAPTURE_VIDEO:
		{
			if (arg_count < 3)
				return false;

			value.args.uint32 = AL::FromString<AL::uint32>(args[1]);

			for (AL::size_t i = 2; i < arg_count; ++i)
				value.args.string.Append(args[i]);
		}
		return true;
	}

	return false;
}

pi_camera*     camera;
pi_camera_args camera_args;

// @return 0 on no input
// @return -1 on decoding error
int  main_args_decode(int argc, char* argv[])
{
	if (argc == 2)
	{
		AL::String arg1(argv[1]);

		if (arg1.Compare("Open", AL::True))
		{
#if defined(PI_CAMERA_DEBUG) || defined(AL_PLATFORM_LINUX)
			camera_args.verb = PI_CAMERA_VERB_OPEN;
			return true;
#else
			AL::OS::Console::WriteLine("Platform not supported");
			return false;
#endif
		}
	}
	else if (argc > 2)
	{
		AL::String arg1(argv[1]);

		if (arg1.Compare("connect", AL::True))
		{
			camera_args.verb = PI_CAMERA_VERB_CONNECT;

			if (argc == 4)
			{
				camera_args.host = argv[2];
				camera_args.port = AL::FromString<AL::uint16>(argv[3]);
				return true;
			}
		}
		else if (arg1.Compare("start", AL::True))
		{
#if defined(PI_CAMERA_DEBUG) || defined(AL_PLATFORM_LINUX)
			camera_args.verb = PI_CAMERA_VERB_START;

			if (argc == 5)
			{
				camera_args.host = argv[2];
				camera_args.port = AL::FromString<AL::uint16>(argv[3]);
				camera_args.max_connections = AL::FromString<AL::size_t>(argv[4]);
				return true;
			}
#else
			AL::OS::Console::WriteLine("Platform not supported");
			return false;
#endif
		}
	}

	return false;
}
bool main_args_show_example(const char* argv0)
{
#if defined(PI_CAMERA_DEBUG) || defined(AL_PLATFORM_LINUX)
	if (!AL::OS::Console::WriteLine("Local: %s open", argv0)) return false;
#endif

	if (!AL::OS::Console::WriteLine("Remote: %s connect host port", argv0)) return false;

#if defined(PI_CAMERA_DEBUG) || defined(AL_PLATFORM_LINUX)
	if (!AL::OS::Console::WriteLine("Service: %s start host port max_connections", argv0)) return false;
#endif

	return true;
}
bool main_args_interactive_prompt(const char* output, AL::String& input)
{
	return AL::OS::Console::Write("%s: ", output) && AL::OS::Console::ReadLine(input);
}
template<typename T>
bool main_args_interactive_prompt(const char* output, T& input)
{
	AL::String line;

	if (!main_args_interactive_prompt(output, line))
		return false;

	input = AL::FromString<T>(line);

	return true;
}
// @return 0 on error
// @return -1 on invalid value
int  main_args_interactive_prompt_verb()
{
	AL::String line;

	if (!main_args_interactive_prompt("Open/Connect/Start", line))
		return 0;

	if (line.Compare("Connect", AL::True))
	{
		camera_args.verb = PI_CAMERA_VERB_CONNECT;
		return 1;
	}
#if defined(PI_CAMERA_DEBUG) || defined(AL_PLATFORM_LINUX)
	else if (line.Compare("Open", AL::True))
	{
		camera_args.verb = PI_CAMERA_VERB_OPEN;
		return 1;
	}
	else if (line.Compare("Start", AL::True))
	{
		camera_args.verb = PI_CAMERA_VERB_START;
		return 1;
	}
#endif

	return -1;
}
bool main_args_interactive_prompt_verb_open()
{
	return true;
}
bool main_args_interactive_prompt_verb_start()
{
	if (!main_args_interactive_prompt("Host", camera_args.host))
		return false;

	if (!main_args_interactive_prompt("Port", camera_args.port))
		return false;

	if (!main_args_interactive_prompt("Max Connections", camera_args.max_connections))
		return false;

	return true;
}
bool main_args_interactive_prompt_verb_connect()
{
	if (!main_args_interactive_prompt("Host", camera_args.host))
		return false;

	if (!main_args_interactive_prompt("Port", camera_args.port))
		return false;

	return true;
}
bool main_args_interactive()
{
	switch (main_args_interactive_prompt_verb())
	{
		case 0:
			return false;

		case -1:
			AL::OS::Console::WriteLine("Invalid option");
			return false;
	}

	switch (camera_args.verb)
	{
		case PI_CAMERA_VERB_OPEN:
			return main_args_interactive_prompt_verb_open();

		case PI_CAMERA_VERB_START:
			return main_args_interactive_prompt_verb_start();

		case PI_CAMERA_VERB_CONNECT:
			return main_args_interactive_prompt_verb_connect();
	}

	return false;
}

bool main_init_open_camera()
{
	switch (camera_args.verb)
	{
		case PI_CAMERA_VERB_OPEN:    return (camera = pi_camera_open()) != nullptr;
		case PI_CAMERA_VERB_START:   return (camera = pi_camera_open_service(camera_args.host.GetCString(), camera_args.port, camera_args.max_connections)) != nullptr;
		case PI_CAMERA_VERB_CONNECT: return (camera = pi_camera_open_remote(camera_args.host.GetCString(), camera_args.port)) != nullptr;
	}

	return false;
}
bool main_init(int argc, char* argv[])
{
	switch (main_args_decode(argc, argv))
	{
		case 0:
			if (!main_args_interactive())
				return false;
			break;

		case -1:
			AL::OS::Console::WriteLine("Error decoding args");
			main_args_show_example(argv[0]);
			return false;
	}

	if (!main_init_open_camera())
	{
		AL::OS::Console::WriteLine("Error opening camera");

		return false;
	}

	return true;
}
void main_deinit()
{
	pi_camera_close(camera);
}

// @return 0 on error
// @return -1 on empty line
// @return -2 on command unknown
// @return -3 on command invalid args
// @return -4 on shutdown requested
int  main_console_read_command(pi_camera_console_command& value)
{
	if (!AL::OS::Console::Write("PiCamera:~$ "))
		return 0;

	AL::String line;

	if (!AL::OS::Console::ReadLine(line))
		return 0;

	if (line.GetLength() == 0)
		return -1;

	auto command_args = line.Split(' ');

	if (!pi_camera_console_command_from_string(value.type, (command_args.GetSize() >= 1) ? command_args[0] : "", (command_args.GetSize() >= 2) ? command_args[1] : ""))
		return -2;

	if (value.type == PI_CAMERA_CONSOLE_COMMAND_EXIT)
		return -4;

	if (!pi_camera_console_command_args_from_string(value, &command_args[0], command_args.GetSize()))
		return -3;

	return 1;
}
bool main_console_execute_command(const pi_camera_console_command& value)
{
	if (value.type >= PI_CAMERA_CONSOLE_COMMAND_COUNT)
		return false;

	pi_camera_console_command_result command_result;

	auto error_code = CONSOLE_COMMANDS[value.type].handler(value, command_result);

	if (error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
	{
		if (!AL::OS::Console::WriteLine("%s returned %u: %s", pi_camera_console_command_to_string(value.type), error_code, pi_camera_get_error_string(error_code)))
			return false;

		if (error_code == PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED)
			return false;
	}

	for (auto& command_result_line : command_result.lines)
		if (!AL::OS::Console::WriteLine(command_result_line))
			return false;

	return true;
}

bool main_run_once()
{
	if (pi_camera_is_remote(camera) && !pi_camera_is_connected(camera))
	{
		AL::OS::Console::WriteLine("Connection lost");
		return false;
	}

	pi_camera_console_command console_command;

	switch (main_console_read_command(console_command))
	{
		case 0:
			break;

		case -1:
			return true;

		case -2:
			return AL::OS::Console::WriteLine("Unknown command");

		case -3:
			return AL::OS::Console::WriteLine("Invalid command args");

		case -4:
			return false;

		default:
			return main_console_execute_command(console_command);
	}

	return false;
}
bool main_display_info()
{
	switch (camera_args.verb)
	{
		case PI_CAMERA_VERB_OPEN:
			return AL::OS::Console::WriteLine("Connected to local PiCamera service");

		case PI_CAMERA_VERB_START:
			return AL::OS::Console::WriteLine("Started PiCamera service");

		case PI_CAMERA_VERB_CONNECT:
			return AL::OS::Console::WriteLine("Connected to remote PiCamera service");
	}

	return true;
}
void main_run()
{
	if (main_display_info())
	{
		while (main_run_once())
		{
		}
	}
}

int main(int argc, char* argv[])
{
	if (main_init(argc, argv))
	{
		main_run();
		main_deinit();
	}

	return 0;
}

AL::uint8 main_console_command_help(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	command_result.lines.PushBack(AL::String::Format("There are %u commands", PI_CAMERA_CONSOLE_COMMAND_COUNT));

	for (auto& console_command : CONSOLE_COMMANDS)
		command_result.lines.PushBack(AL::String::Format("\t%s", console_command.description));

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
AL::uint8 main_console_command_is_busy(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	bool value;
	auto error_code = pi_camera_is_busy(camera, &value);

	if (error_code == PI_CAMERA_ERROR_CODE_SUCCESS)
		command_result.lines.PushBack(AL::ToString(value));

	return error_code;
}
AL::uint8 main_console_command_is_remote(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	command_result.lines.PushBack(AL::ToString(pi_camera_is_remote(camera)));

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
AL::uint8 main_console_command_is_service(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	command_result.lines.PushBack(AL::ToString(pi_camera_is_service(camera)));

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
AL::uint8 main_console_command_is_connected(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	command_result.lines.PushBack(AL::ToString(pi_camera_is_connected(camera)));

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
AL::uint8 main_console_command_get_ev(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	AL::int8 value;
	auto     error_code = pi_camera_get_ev(camera, &value);

	if (error_code == PI_CAMERA_ERROR_CODE_SUCCESS)
		command_result.lines.PushBack(AL::ToString(value));

	return error_code;
}
AL::uint8 main_console_command_set_ev(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	return pi_camera_set_ev(camera, command.args.int8);
}
AL::uint8 main_console_command_get_iso(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	AL::uint16 value;
	auto       error_code = pi_camera_get_iso(camera, &value);

	if (error_code == PI_CAMERA_ERROR_CODE_SUCCESS)
		command_result.lines.PushBack(AL::ToString(value));

	return error_code;
}
AL::uint8 main_console_command_set_iso(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	return pi_camera_set_iso(camera, command.args.uint16);
}
AL::uint8 main_console_command_get_config(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	pi_camera_config value;
	auto             error_code = pi_camera_get_config(camera, &value);

	if (error_code == PI_CAMERA_ERROR_CODE_SUCCESS)
	{
		command_result.lines.PushBack(AL::String::Format("EV: %i", value.ev));
		command_result.lines.PushBack(AL::String::Format("ISO: %u", value.iso));
		command_result.lines.PushBack(AL::String::Format("Contrast: %i", value.contrast));
		command_result.lines.PushBack(AL::String::Format("Sharpness: %i", value.sharpness));
		command_result.lines.PushBack(AL::String::Format("Brightness: %i", value.brightness));
		command_result.lines.PushBack(AL::String::Format("Saturation: %i", value.saturation));
		command_result.lines.PushBack(AL::String::Format("White Balance: %s", (value.white_balance == PI_CAMERA_WHITE_BALANCE_AUTO) ? "auto" : AL::ToString(value.white_balance).GetCString()));
		command_result.lines.PushBack(AL::String::Format("Shutter Speed: %s", (value.shutter_speed_us == 0) ? "auto" : AL::String::Format("%lluus", value.shutter_speed_us).GetCString()));
		command_result.lines.PushBack(AL::String::Format("Exposure Mode: %s", (value.exposure_mode == PI_CAMERA_EXPOSURE_MODE_AUTO) ? "auto" : AL::ToString(value.exposure_mode).GetCString()));
		command_result.lines.PushBack(AL::String::Format("Metoring Mode: %s", (value.metoring_mode == PI_CAMERA_METORING_MODE_MATRIX) ? "matrix" : AL::ToString(value.metoring_mode).GetCString()));
		command_result.lines.PushBack(AL::String::Format("JPG Quality: %u", value.jpg_quality));
		command_result.lines.PushBack(AL::String::Format("Image Size: %ux%u", value.image_size_width, value.image_size_height));
		command_result.lines.PushBack(AL::String::Format("Image Effect: %s", (value.image_effect == PI_CAMERA_IMAGE_EFFECT_NONE) ? "none" : AL::ToString(value.image_effect).GetCString()));
		command_result.lines.PushBack(AL::String::Format("Image Rotation: %u", value.image_rotation));
		command_result.lines.PushBack(AL::String::Format("Video Bit Rate: %s", AL::ToString(value.video_bit_rate).GetCString()));
		command_result.lines.PushBack(AL::String::Format("Video Frame Rate: %u", value.video_frame_rate));
	}

	return error_code;
}
AL::uint8 main_console_command_get_contrast(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	AL::int8 value;
	auto     error_code = pi_camera_get_contrast(camera, &value);

	if (error_code == PI_CAMERA_ERROR_CODE_SUCCESS)
		command_result.lines.PushBack(AL::ToString(value));

	return error_code;
}
AL::uint8 main_console_command_set_contrast(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	return pi_camera_set_contrast(camera, command.args.int8);
}
AL::uint8 main_console_command_get_sharpness(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	AL::int8 value;
	auto     error_code = pi_camera_get_sharpness(camera, &value);

	if (error_code == PI_CAMERA_ERROR_CODE_SUCCESS)
		command_result.lines.PushBack(AL::ToString(value));

	return error_code;
}
AL::uint8 main_console_command_set_sharpness(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	return pi_camera_set_sharpness(camera, command.args.int8);
}
AL::uint8 main_console_command_get_brightness(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	AL::uint8 value;
	auto      error_code = pi_camera_get_brightness(camera, &value);

	if (error_code == PI_CAMERA_ERROR_CODE_SUCCESS)
		command_result.lines.PushBack(AL::ToString(value));

	return error_code;
}
AL::uint8 main_console_command_set_brightness(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	return pi_camera_set_brightness(camera, command.args.uint8);
}
AL::uint8 main_console_command_get_saturation(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	AL::int8 value;
	auto     error_code = pi_camera_get_saturation(camera, &value);

	if (error_code == PI_CAMERA_ERROR_CODE_SUCCESS)
		command_result.lines.PushBack(AL::ToString(value));

	return error_code;
}
AL::uint8 main_console_command_set_saturation(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	return pi_camera_set_saturation(camera, command.args.int8);
}
AL::uint8 main_console_command_get_white_balance(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	AL::uint8 value;
	auto      error_code = pi_camera_get_white_balance(camera, &value);

	if (error_code == PI_CAMERA_ERROR_CODE_SUCCESS)
		command_result.lines.PushBack(AL::ToString(value));

	return error_code;
}
AL::uint8 main_console_command_set_white_balance(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	return pi_camera_set_white_balance(camera, command.args.uint8);
}
AL::uint8 main_console_command_get_shutter_speed(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	AL::uint64 value;
	auto       error_code = pi_camera_get_shutter_speed(camera, &value);

	if (error_code == PI_CAMERA_ERROR_CODE_SUCCESS)
		command_result.lines.PushBack((value == 0) ? "auto" : AL::String::Format("%lluus", value));

	return error_code;
}
AL::uint8 main_console_command_set_shutter_speed(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	return pi_camera_set_shutter_speed(camera, command.args.uint64);
}
AL::uint8 main_console_command_get_exposure_mode(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	AL::uint8 value;
	auto      error_code = pi_camera_get_exposure_mode(camera, &value);

	if (error_code == PI_CAMERA_ERROR_CODE_SUCCESS)
		command_result.lines.PushBack(AL::ToString(value));

	return error_code;
}
AL::uint8 main_console_command_set_exposure_mode(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	return pi_camera_set_exposure_mode(camera, command.args.uint8);
}
AL::uint8 main_console_command_get_metoring_mode(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	AL::uint8 value;
	auto      error_code = pi_camera_get_metoring_mode(camera, &value);

	if (error_code == PI_CAMERA_ERROR_CODE_SUCCESS)
		command_result.lines.PushBack(AL::ToString(value));

	return error_code;
}
AL::uint8 main_console_command_set_metoring_mode(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	return pi_camera_set_metoring_mode(camera, command.args.uint8);
}
AL::uint8 main_console_command_get_jpg_quality(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	AL::uint8 value;
	auto      error_code = pi_camera_get_jpg_quality(camera, &value);

	if (error_code == PI_CAMERA_ERROR_CODE_SUCCESS)
		command_result.lines.PushBack(AL::ToString(value));

	return error_code;
}
AL::uint8 main_console_command_set_jpg_quality(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	return pi_camera_set_jpg_quality(camera, command.args.uint8);
}
AL::uint8 main_console_command_get_image_size(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	AL::uint16 width, height;
	auto       error_code = pi_camera_get_image_size(camera, &width, &height);

	if (error_code == PI_CAMERA_ERROR_CODE_SUCCESS)
		command_result.lines.PushBack(AL::String::Format("%ux%u", width, height));

	return error_code;
}
AL::uint8 main_console_command_set_image_size(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	return pi_camera_set_image_size(camera, command.args.uint16_2[0], command.args.uint16_2[1]);
}
AL::uint8 main_console_command_get_image_effect(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	AL::uint8 value;
	auto      error_code = pi_camera_get_image_effect(camera, &value);

	if (error_code == PI_CAMERA_ERROR_CODE_SUCCESS)
		command_result.lines.PushBack(AL::ToString(value));

	return error_code;
}
AL::uint8 main_console_command_set_image_effect(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	return pi_camera_set_image_effect(camera, command.args.uint8);
}
AL::uint8 main_console_command_get_image_rotation(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	AL::uint16 value;
	auto       error_code = pi_camera_get_image_rotation(camera, &value);

	if (error_code == PI_CAMERA_ERROR_CODE_SUCCESS)
		command_result.lines.PushBack(AL::ToString(value));

	return error_code;
}
AL::uint8 main_console_command_set_image_rotation(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	return pi_camera_set_image_rotation(camera, command.args.uint16);
}
AL::uint8 main_console_command_get_video_bit_rate(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	AL::uint32 value;
	auto       error_code = pi_camera_get_video_bit_rate(camera, &value);

	if (error_code == PI_CAMERA_ERROR_CODE_SUCCESS)
		command_result.lines.PushBack(AL::ToString(value));

	return error_code;
}
AL::uint8 main_console_command_set_video_bit_rate(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	return pi_camera_set_image_rotation(camera, command.args.uint32);
}
AL::uint8 main_console_command_get_video_frame_rate(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	AL::uint8 value;
	auto      error_code = pi_camera_get_video_frame_rate(camera, &value);

	if (error_code == PI_CAMERA_ERROR_CODE_SUCCESS)
		command_result.lines.PushBack(AL::ToString(value));

	return error_code;
}
AL::uint8 main_console_command_set_video_frame_rate(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	return pi_camera_set_image_rotation(camera, command.args.uint8);
}
AL::uint8 main_console_command_capture(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	AL::uint64 file_size;
	auto       error_code = pi_camera_capture(camera, command.args.string.GetCString(), &file_size);

	if (error_code == PI_CAMERA_ERROR_CODE_SUCCESS)
		command_result.lines.PushBack(AL::String::Format("Saved %llu bytes to %s", file_size, command.args.string.GetCString()));

	return error_code;
}
AL::uint8 main_console_command_capture_video(const pi_camera_console_command& command, pi_camera_console_command_result& command_result)
{
	AL::uint64 file_size;
	auto       error_code = pi_camera_capture_video(camera, command.args.string.GetCString(), command.args.uint32, &file_size);

	if (error_code == PI_CAMERA_ERROR_CODE_SUCCESS)
		command_result.lines.PushBack(AL::String::Format("Saved %llu bytes to %s", file_size, command.args.string.GetCString()));

	return error_code;
}

constexpr pi_camera_console_command_context CONSOLE_COMMANDS[PI_CAMERA_CONSOLE_COMMAND_COUNT] =
{
	{ PI_CAMERA_CONSOLE_COMMAND_HELP,                 &main_console_command_help,                 "help" },
	{ PI_CAMERA_CONSOLE_COMMAND_EXIT,                 nullptr,                                    "exit" },
	{ PI_CAMERA_CONSOLE_COMMAND_IS_BUSY,              &main_console_command_is_busy,              "is busy" },
	{ PI_CAMERA_CONSOLE_COMMAND_IS_REMOTE,            &main_console_command_is_remote,            "is remote" },
	{ PI_CAMERA_CONSOLE_COMMAND_IS_SERVICE,           &main_console_command_is_service,           "is service" },
	{ PI_CAMERA_CONSOLE_COMMAND_IS_CONNECTED,         &main_console_command_is_connected,         "is connected" },
	{ PI_CAMERA_CONSOLE_COMMAND_GET_EV,               &main_console_command_get_ev,               "get e|ev" },
	{ PI_CAMERA_CONSOLE_COMMAND_SET_EV,               &main_console_command_set_ev,               "set e|ev value" },
	{ PI_CAMERA_CONSOLE_COMMAND_GET_ISO,              &main_console_command_get_iso,              "get i|iso" },
	{ PI_CAMERA_CONSOLE_COMMAND_SET_ISO,              &main_console_command_set_iso,              "set i|iso value" },
	{ PI_CAMERA_CONSOLE_COMMAND_GET_CONFIG,           &main_console_command_get_config,           "get config" },
	{ PI_CAMERA_CONSOLE_COMMAND_GET_CONTRAST,         &main_console_command_get_contrast,         "get c|contrast" },
	{ PI_CAMERA_CONSOLE_COMMAND_SET_CONTRAST,         &main_console_command_set_contrast,         "set c|contrast value" },
	{ PI_CAMERA_CONSOLE_COMMAND_GET_SHARPNESS,        &main_console_command_get_sharpness,        "get sh|sharpness" },
	{ PI_CAMERA_CONSOLE_COMMAND_SET_SHARPNESS,        &main_console_command_set_sharpness,        "set sh|sharpness value" },
	{ PI_CAMERA_CONSOLE_COMMAND_GET_BRIGHTNESS,       &main_console_command_get_brightness,       "get br|brightness" },
	{ PI_CAMERA_CONSOLE_COMMAND_SET_BRIGHTNESS,       &main_console_command_set_brightness,       "set br|brightness value" },
	{ PI_CAMERA_CONSOLE_COMMAND_GET_SATURATION,       &main_console_command_get_saturation,       "get sat|saturation" },
	{ PI_CAMERA_CONSOLE_COMMAND_SET_SATURATION,       &main_console_command_set_saturation,       "set sat|saturation value" },
	{ PI_CAMERA_CONSOLE_COMMAND_GET_WHITE_BALANCE,    &main_console_command_get_white_balance,    "get wb|white_balance" },
	{ PI_CAMERA_CONSOLE_COMMAND_SET_WHITE_BALANCE,    &main_console_command_set_white_balance,    "set wb|white_balance value" },
	{ PI_CAMERA_CONSOLE_COMMAND_GET_SHUTTER_SPEED,    &main_console_command_get_shutter_speed,    "get ss|shutter|shutter_speed" },
	{ PI_CAMERA_CONSOLE_COMMAND_SET_SHUTTER_SPEED,    &main_console_command_set_shutter_speed,    "set ss|shutter|shutter_speed value" },
	{ PI_CAMERA_CONSOLE_COMMAND_GET_EXPOSURE_MODE,    &main_console_command_get_exposure_mode,    "get em|exposure|exposure_mode" },
	{ PI_CAMERA_CONSOLE_COMMAND_SET_EXPOSURE_MODE,    &main_console_command_set_exposure_mode,    "set em|exposure|exposure_mode value" },
	{ PI_CAMERA_CONSOLE_COMMAND_GET_METORING_MODE,    &main_console_command_get_metoring_mode,    "get mm|metoring|metoring_mode" },
	{ PI_CAMERA_CONSOLE_COMMAND_SET_METORING_MODE,    &main_console_command_set_metoring_mode,    "set mm|metoring|metoring_mode value" },
	{ PI_CAMERA_CONSOLE_COMMAND_GET_JPG_QUALITY,      &main_console_command_get_jpg_quality,      "get jq|quality|jpg_quality" },
	{ PI_CAMERA_CONSOLE_COMMAND_SET_JPG_QUALITY,      &main_console_command_set_jpg_quality,      "set jq|quality|jpg_quality value" },
	{ PI_CAMERA_CONSOLE_COMMAND_GET_IMAGE_SIZE,       &main_console_command_get_image_size,       "get is|size|image_size" },
	{ PI_CAMERA_CONSOLE_COMMAND_SET_IMAGE_SIZE,       &main_console_command_set_image_size,       "set is|size|image_size width height" },
	{ PI_CAMERA_CONSOLE_COMMAND_GET_IMAGE_EFFECT,     &main_console_command_get_image_effect,     "get ie|effect|image_effect" },
	{ PI_CAMERA_CONSOLE_COMMAND_SET_IMAGE_EFFECT,     &main_console_command_set_image_effect,     "set ie|effect|image_effect value" },
	{ PI_CAMERA_CONSOLE_COMMAND_GET_IMAGE_ROTATION,   &main_console_command_get_image_rotation,   "get ir|rot|rotation|image_rotation" },
	{ PI_CAMERA_CONSOLE_COMMAND_SET_IMAGE_ROTATION,   &main_console_command_set_image_rotation,   "set ir|rot|rotation|image_rotation value" },
	{ PI_CAMERA_CONSOLE_COMMAND_GET_VIDEO_BIT_RATE,   &main_console_command_get_video_bit_rate,   "get vbr|video_bit_rate" },
	{ PI_CAMERA_CONSOLE_COMMAND_SET_VIDEO_BIT_RATE,   &main_console_command_set_video_bit_rate,   "set vbr|video_bit_rate" },
	{ PI_CAMERA_CONSOLE_COMMAND_GET_VIDEO_FRAME_RATE, &main_console_command_get_video_frame_rate, "get vfr|video_frame_rate" },
	{ PI_CAMERA_CONSOLE_COMMAND_SET_VIDEO_FRAME_RATE, &main_console_command_set_video_frame_rate, "set vfr|video_frame_rate" },
	{ PI_CAMERA_CONSOLE_COMMAND_CAPTURE,              &main_console_command_capture,              "capture /path/to/file" },
	{ PI_CAMERA_CONSOLE_COMMAND_CAPTURE_VIDEO,        &main_console_command_capture_video,        "capture_video duration /path/to/file" }
};

template<AL::size_t ... INDEXES>
constexpr bool verify_console_commands(AL::Index_Sequence<INDEXES ...>)
{
	return ((CONSOLE_COMMANDS[INDEXES].type == INDEXES) && ...);
}

static_assert(verify_console_commands(typename AL::Make_Index_Sequence<PI_CAMERA_CONSOLE_COMMAND_COUNT>::Type {}));
