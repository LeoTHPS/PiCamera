#include "pi_camera.hpp"

#include <AL/OS/Shell.hpp>
#include <AL/OS/Thread.hpp>

#include <AL/Game/Loop.hpp>

#include <AL/Network/DNS.hpp>
#include <AL/Network/TcpSocket.hpp>
#include <AL/Network/SocketExtensions.hpp>

#include <AL/FileSystem/File.hpp>
#include <AL/FileSystem/Directory.hpp>

#include <AL/Collections/Array.hpp>
#include <AL/Collections/LinkedList.hpp>

#define PI_CAMERA_SERVICE_TICK_RATE 2

enum PI_CAMERA_TYPES : AL::uint8
{
	PI_CAMERA_TYPE_LOCAL,
	PI_CAMERA_TYPE_REMOTE,
	PI_CAMERA_TYPE_SERVICE,
	PI_CAMERA_TYPE_SESSION
};

enum PI_CAMERA_OPCODES : AL::uint8
{
	PI_CAMERA_OPCODE_IS_BUSY,

	PI_CAMERA_OPCODE_GET_EV,
	PI_CAMERA_OPCODE_SET_EV,

	PI_CAMERA_OPCODE_GET_ISO,
	PI_CAMERA_OPCODE_SET_ISO,

	PI_CAMERA_OPCODE_GET_CONFIG,
	PI_CAMERA_OPCODE_SET_CONFIG,

	PI_CAMERA_OPCODE_GET_CONTRAST,
	PI_CAMERA_OPCODE_SET_CONTRAST,

	PI_CAMERA_OPCODE_GET_SHARPNESS,
	PI_CAMERA_OPCODE_SET_SHARPNESS,

	PI_CAMERA_OPCODE_GET_BRIGHTNESS,
	PI_CAMERA_OPCODE_SET_BRIGHTNESS,

	PI_CAMERA_OPCODE_GET_SATURATION,
	PI_CAMERA_OPCODE_SET_SATURATION,

	PI_CAMERA_OPCODE_GET_WHITE_BALANCE,
	PI_CAMERA_OPCODE_SET_WHITE_BALANCE,

	PI_CAMERA_OPCODE_GET_SHUTTER_SPEED,
	PI_CAMERA_OPCODE_SET_SHUTTER_SPEED,

	PI_CAMERA_OPCODE_GET_EXPOSURE_MODE,
	PI_CAMERA_OPCODE_SET_EXPOSURE_MODE,

	PI_CAMERA_OPCODE_GET_METORING_MODE,
	PI_CAMERA_OPCODE_SET_METORING_MODE,

	PI_CAMERA_OPCODE_GET_JPG_QUALITY,
	PI_CAMERA_OPCODE_SET_JPG_QUALITY,

	PI_CAMERA_OPCODE_GET_IMAGE_SIZE,
	PI_CAMERA_OPCODE_SET_IMAGE_SIZE,

	PI_CAMERA_OPCODE_GET_IMAGE_EFFECT,
	PI_CAMERA_OPCODE_SET_IMAGE_EFFECT,

	PI_CAMERA_OPCODE_GET_IMAGE_ROTATION,
	PI_CAMERA_OPCODE_SET_IMAGE_ROTATION,

	PI_CAMERA_OPCODE_CAPTURE,

	PI_CAMERA_OPCODE_COUNT
};

#pragma pack(push, 1)
struct pi_camera_packet_header
{
	AL::uint8  opcode;
	AL::uint8  error_code;
	AL::uint32 buffer_size;
};
#pragma pack(pop)

typedef AL::Collections::Array<AL::uint8> pi_camera_packet_buffer;

typedef bool(*pi_camera_service_packet_handler)(struct pi_camera_service* camera_service, struct pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size);

struct pi_camera_service_packet_handler_context
{
	AL::uint8                        opcode;
	pi_camera_service_packet_handler packet_handler;
};

struct pi_camera
{
	AL::uint8 type;

	explicit pi_camera(AL::uint8 type)
		: type(type)
	{
	}
};

struct pi_camera_local
	: public pi_camera
{
	bool             is_busy = false;

	pi_camera_config config;
	AL::String       cli_params;

	pi_camera_local()
		: pi_camera(PI_CAMERA_TYPE_LOCAL),
		config
		{
			.ev                = PI_CAMERA_EV_DEFAULT,
			.iso               = PI_CAMERA_ISO_100,
			.contrast          = PI_CAMERA_CONTRAST_DEFAULT,
			.sharpness         = PI_CAMERA_SHARPNESS_DEFAULT,
			.brightness        = PI_CAMERA_BRIGHTNESS_DEFAULT,
			.saturation        = PI_CAMERA_SATURATION_DEFAULT,
			.white_balance     = PI_CAMERA_WHITE_BALANCE_AUTO,
			.shutter_speed     = AL::TimeSpan::FromMicroseconds(PI_CAMERA_SHUTTER_SPEED_AUTO),
			.exposure_mode     = PI_CAMERA_EXPOSURE_MODE_AUTO,
			.metoring_mode     = PI_CAMERA_METORING_MODE_MATRIX,
			.jpg_quality       = PI_CAMERA_JPG_QUALITY_DEFAULT,
			.image_effect      = PI_CAMERA_IMAGE_EFFECT_NONE,
			.image_rotation    = PI_CAMERA_IMAGE_ROTATION_DEFAULT,
			.image_size_width  = PI_CAMERA_IMAGE_SIZE_WIDTH_MAX,
			.image_size_height = PI_CAMERA_IMAGE_SIZE_HEIGHT_MAX
		}
	{
	}
};

struct pi_camera_remote
	: public pi_camera
{
	AL::Network::TcpSocket  socket;
	AL::Network::IPEndPoint remote_end_point;

	explicit pi_camera_remote(AL::Network::IPEndPoint&& remote_end_point)
		: pi_camera(PI_CAMERA_TYPE_REMOTE),
		socket(remote_end_point.Host.GetFamily()),
		remote_end_point(AL::Move(remote_end_point))
	{
	}
};

struct pi_camera_service;

struct pi_camera_session
	: public pi_camera
{
	AL::Network::TcpSocket socket;
	pi_camera_service*     service;

	explicit pi_camera_session(pi_camera_service* service, AL::Network::TcpSocket&& socket)
		: pi_camera(PI_CAMERA_TYPE_SESSION),
		socket(AL::Move(socket)),
		service(service)
	{
	}
};

typedef AL::Collections::LinkedList<pi_camera_session*> pi_camera_session_list;

struct pi_camera_service
	: public pi_camera
{
	bool                    is_thread_stopping = false;

	pi_camera_local         local;
	AL::Network::TcpSocket  socket;
	AL::OS::Thread          thread;
	pi_camera_session_list  sessions;
	AL::uint32              image_counter = 0;
	AL::size_t              max_connections;
	AL::Network::IPEndPoint local_end_point;

	pi_camera_service(AL::Network::IPEndPoint&& local_end_point, AL::size_t max_connections)
		: pi_camera(PI_CAMERA_TYPE_SERVICE),
		socket(local_end_point.Host.GetFamily()),
		max_connections(max_connections),
		local_end_point(AL::Move(local_end_point))
	{
	}
};

bool pi_camera_file_get_size(const char* path, AL::uint32& value)
{
	AL::uint64 value64;

	try
	{
		value64 = AL::FileSystem::File::GetSize(path);
	}
	catch (const AL::Exception& exception)
	{

		return false;
	}

	value = AL::Math::Clamp<AL::uint32>(value64, AL::Integer<AL::uint32>::Minimum, AL::Integer<AL::uint32>::Maximum);

	return true;
}
bool pi_camera_file_read(const char* path, void* buffer, AL::uint32 size)
{
	AL::FileSystem::File file(path);

	try
	{
		if (!file.Open(AL::FileSystem::FileOpenModes::Read | AL::FileSystem::FileOpenModes::Binary))
			return false;

		for (AL::size_t bytes_read, total_bytes_read = 0; total_bytes_read < size; total_bytes_read += bytes_read)
			bytes_read = file.Read(&reinterpret_cast<AL::uint8*>(buffer)[total_bytes_read], size - total_bytes_read);
	}
	catch (const AL::Exception& exception)
	{

		return false;
	}

	return true;
}
bool pi_camera_file_write(const char* path, const void* buffer, AL::uint32 size)
{
	AL::FileSystem::File file(path);

	try
	{
		file.Open(AL::FileSystem::FileOpenModes::Write | AL::FileSystem::FileOpenModes::Binary | AL::FileSystem::FileOpenModes::Truncate);

		for (AL::size_t bytes_written, total_bytes_written = 0; total_bytes_written < size; total_bytes_written += bytes_written)
			bytes_written = file.Write(&reinterpret_cast<const AL::uint8*>(buffer)[total_bytes_written], size - total_bytes_written);
	}
	catch (const AL::Exception& exception)
	{

		return false;
	}

	return true;
}
bool pi_camera_file_delete(const char* path)
{
	try
	{
		AL::FileSystem::File::Delete(path);
	}
	catch (const AL::Exception& exception)
	{

		return false;
	}

	return true;
}

void pi_camera_net_socket_close(AL::Network::TcpSocket& socket)
{
	socket.Close();
}
bool pi_camera_net_socket_listen(AL::Network::TcpSocket& socket, const AL::Network::IPEndPoint& local_end_point, AL::size_t backlog, bool block = false)
{
	socket.SetBlocking(block ? AL::True : AL::False);

	try
	{
		socket.Open();
		socket.Bind(local_end_point);
		socket.Listen(backlog);
	}
	catch (const AL::Exception& exception)
	{
		pi_camera_net_socket_close(socket);

		return false;
	}

	return true;
}
// @return 0 on error
// @return -1 if would block
int  pi_camera_net_socket_accept(AL::Network::TcpSocket& socket, AL::Network::TcpSocket& new_socket)
{
	try
	{
		if (!socket.Accept(new_socket))
		{

			return -1;
		}
	}
	catch (const AL::Exception& exception)
	{
		pi_camera_net_socket_close(socket);

		return 0;
	}

	return 1;
}
bool pi_camera_net_socket_connect(AL::Network::TcpSocket& socket, const AL::Network::IPEndPoint& remote_end_point, bool block = false)
{
	try
	{
		socket.Open();

		if (!socket.Connect(remote_end_point))
		{
			pi_camera_net_socket_close(socket);

			return false;
		}

		socket.SetBlocking(block ? AL::True : AL::False);
	}
	catch (const AL::Exception& exception)
	{
		pi_camera_net_socket_close(socket);

		return false;
	}

	return true;
}
bool pi_camera_net_socket_send(AL::Network::TcpSocket& socket, const void* buffer, AL::size_t size)
{
	AL::size_t number_of_bytes_sent;

	try
	{
		if (!AL::Network::SocketExtensions::SendAll(socket, buffer, size, number_of_bytes_sent))
		{
			pi_camera_net_socket_close(socket);

			return false;
		}
	}
	catch (const AL::Exception& exception)
	{
		pi_camera_net_socket_close(socket);

		return false;
	}

	return true;
}
// @return 0 on error
// @return -1 if would block
int  pi_camera_net_socket_receive(AL::Network::TcpSocket& socket, void* buffer, AL::size_t size, AL::size_t& number_of_bytes_received)
{
	try
	{
		if (!socket.Receive(buffer, size, number_of_bytes_received))
		{
			pi_camera_net_socket_close(socket);

			return 0;
		}
	}
	catch (const AL::Exception& exception)
	{
		pi_camera_net_socket_close(socket);

		return 0;
	}

	return (number_of_bytes_received > 0) ? 1 : -1;
}
// @return 0 on error
// @return -1 if would block
int  pi_camera_net_socket_receive_all(AL::Network::TcpSocket& socket, void* buffer, AL::size_t size, bool block_once = true)
{
	AL::size_t number_of_bytes_received;

	try
	{
		if ((block_once && !AL::Network::SocketExtensions::TryReceiveAll(socket, buffer, size, number_of_bytes_received)) ||
			(!block_once && !AL::Network::SocketExtensions::ReceiveAll(socket, buffer, size, number_of_bytes_received)))
		{

			return 0;
		}
	}
	catch (const AL::Exception& exception)
	{
		pi_camera_net_socket_close(socket);

		return 0;
	}

	return (number_of_bytes_received > 0) ? 1 : -1;
}
bool pi_camera_net_socket_resolve_end_point(AL::Network::IPEndPoint& value, const char* host, AL::uint16 port)
{
	value.Port = port;

	try
	{
		if (!AL::Network::DNS::Resolve(value.Host, host))
		{

			return false;
		}
	}
	catch (const AL::Exception& exception)
	{

		return false;
	}

	return true;
}

bool pi_camera_net_send_packet(AL::Network::TcpSocket& socket, AL::uint8 opcode, AL::uint8 error_code, const void* buffer, AL::uint32 size)
{
	pi_camera_packet_header packet_header =
	{
		.opcode      = AL::BitConverter::HostToNetwork(opcode),
		.error_code  = AL::BitConverter::HostToNetwork(error_code),
		.buffer_size = AL::BitConverter::HostToNetwork(size)
	};

	return pi_camera_net_socket_send(socket, &packet_header, sizeof(pi_camera_packet_header)) && ((size == 0) || (error_code != PI_CAMERA_ERROR_CODE_SUCCESS) || pi_camera_net_socket_send(socket, buffer, size));
}
// @return 0 on error
// @return -1 if would block
int  pi_camera_net_receive_packet(AL::Network::TcpSocket& socket, pi_camera_packet_header& header, pi_camera_packet_buffer& buffer, bool block_once = true)
{
	switch (pi_camera_net_socket_receive_all(socket, &header, sizeof(pi_camera_packet_header), block_once))
	{
		case 0:  return 0;
		case -1: return -1;
	}

	header.opcode      = AL::BitConverter::NetworkToHost(header.opcode);
	header.error_code  = AL::BitConverter::NetworkToHost(header.error_code);
	header.buffer_size = AL::BitConverter::NetworkToHost(header.buffer_size);

	if (header.error_code == PI_CAMERA_ERROR_CODE_SUCCESS)
	{
		buffer.SetCapacity(header.buffer_size);

		if (pi_camera_net_socket_receive_all(socket, &buffer[0], header.buffer_size, false) == 0)
			return 0;
	}

	return 1;
}

auto pi_camera_config_to_packet_buffer(const pi_camera_config& value)
{
	pi_camera_packet_buffer packet_buffer(26);
	packet_buffer[0]                                   = static_cast<AL::uint8>(value.ev);
	*reinterpret_cast<AL::uint16*>(&packet_buffer[1])  = AL::BitConverter::HostToNetwork(value.iso);
	packet_buffer[3]                                   = static_cast<AL::uint8>(value.contrast);
	packet_buffer[4]                                   = static_cast<AL::uint8>(value.sharpness);
	packet_buffer[5]                                   = static_cast<AL::uint8>(value.brightness);
	packet_buffer[6]                                   = static_cast<AL::uint8>(value.saturation);
	packet_buffer[7]                                   = value.white_balance;
	*reinterpret_cast<AL::uint64*>(&packet_buffer[8])  = AL::BitConverter::HostToNetwork(value.shutter_speed.ToNanoseconds());
	packet_buffer[16]                                  = value.exposure_mode;
	packet_buffer[17]                                  = value.metoring_mode;
	packet_buffer[18]                                  = value.jpg_quality;
	packet_buffer[19]                                  = value.image_effect;
	*reinterpret_cast<AL::uint16*>(&packet_buffer[20]) = AL::BitConverter::HostToNetwork(value.image_rotation);
	*reinterpret_cast<AL::uint16*>(&packet_buffer[22]) = AL::BitConverter::HostToNetwork(value.image_size_width);
	*reinterpret_cast<AL::uint16*>(&packet_buffer[24]) = AL::BitConverter::HostToNetwork(value.image_size_height);

	return packet_buffer;
}
auto pi_camera_config_from_packet_buffer(const void* buffer, AL::size_t size)
{
	auto packet_buffer = reinterpret_cast<const AL::uint8*>(buffer);

	return pi_camera_config
	{
		.ev                = static_cast<AL::int8>(packet_buffer[0]),
		.iso               = AL::BitConverter::NetworkToHost(*reinterpret_cast<const AL::uint16*>(&packet_buffer[1])),
		.contrast          = static_cast<AL::int8>(packet_buffer[3]),
		.sharpness         = static_cast<AL::int8>(packet_buffer[4]),
		.brightness        = static_cast<AL::int8>(packet_buffer[5]),
		.saturation        = static_cast<AL::int8>(packet_buffer[6]),
		.white_balance     = packet_buffer[7],
		.shutter_speed     = AL::TimeSpan::FromNanoseconds(AL::BitConverter::NetworkToHost(*reinterpret_cast<const AL::uint64*>(&packet_buffer[8]))),
		.exposure_mode     = packet_buffer[16],
		.metoring_mode     = packet_buffer[17],
		.jpg_quality       = packet_buffer[18],
		.image_effect      = packet_buffer[19],
		.image_rotation    = AL::BitConverter::NetworkToHost(*reinterpret_cast<const AL::uint16*>(&packet_buffer[20])),
		.image_size_width  = AL::BitConverter::NetworkToHost(*reinterpret_cast<const AL::uint16*>(&packet_buffer[22])),
		.image_size_height = AL::BitConverter::NetworkToHost(*reinterpret_cast<const AL::uint16*>(&packet_buffer[24]))
	};
}

AL::uint8 pi_camera_net_begin_is_busy(AL::Network::TcpSocket& socket, bool& value)
{
	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_IS_BUSY, PI_CAMERA_ERROR_CODE_SUCCESS, nullptr, 0))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	value = static_cast<bool>(packet_buffer[0]);

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_is_busy(AL::Network::TcpSocket& socket, AL::uint8 error_code, bool value)
{
	if (error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_IS_BUSY, error_code, nullptr, 0);

	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_IS_BUSY, PI_CAMERA_ERROR_CODE_SUCCESS, &value, sizeof(bool));
}

AL::uint8 pi_camera_net_begin_get_ev(AL::Network::TcpSocket& socket, AL::int8& value)
{
	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_EV, PI_CAMERA_ERROR_CODE_SUCCESS, nullptr, 0))
		return 0;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	value = static_cast<AL::int8>(packet_buffer[0]);

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_get_ev(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::int8 value)
{
	if (error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_EV, error_code, nullptr, 0);

	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_EV, PI_CAMERA_ERROR_CODE_SUCCESS, &value, sizeof(AL::int8));
}
AL::uint8 pi_camera_net_begin_set_ev(AL::Network::TcpSocket& socket, AL::int8 value)
{
	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_EV, PI_CAMERA_ERROR_CODE_SUCCESS, &value, sizeof(AL::int8)))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_set_ev(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::int8 value)
{
	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_EV, error_code, nullptr, 0);
}

AL::uint8 pi_camera_net_begin_get_iso(AL::Network::TcpSocket& socket, AL::uint16& value)
{
	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_ISO, PI_CAMERA_ERROR_CODE_SUCCESS, nullptr, 0))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	value = AL::BitConverter::NetworkToHost(*reinterpret_cast<const AL::uint16*>(&packet_buffer[0]));

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_get_iso(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::uint16 value)
{
	if (error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_ISO, error_code, nullptr, 0);

	value = AL::BitConverter::HostToNetwork(value);

	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_ISO, PI_CAMERA_ERROR_CODE_SUCCESS, &value, sizeof(AL::uint16));
}
AL::uint8 pi_camera_net_begin_set_iso(AL::Network::TcpSocket& socket, AL::uint16 value)
{
	value = AL::BitConverter::HostToNetwork(value);

	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_ISO, PI_CAMERA_ERROR_CODE_SUCCESS, &value, sizeof(AL::uint16)))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_set_iso(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::uint16 value)
{
	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_ISO, error_code, nullptr, 0);
}

AL::uint8 pi_camera_net_begin_get_config(AL::Network::TcpSocket& socket, pi_camera_config& value)
{
	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_CONFIG, PI_CAMERA_ERROR_CODE_SUCCESS, nullptr, 0))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	value = pi_camera_config_from_packet_buffer(&packet_buffer[0], packet_buffer.GetSize());

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_get_config(AL::Network::TcpSocket& socket, AL::uint8 error_code, const pi_camera_config& value)
{
	if (error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_CONFIG, error_code, nullptr, 0);

	auto packet_buffer = pi_camera_config_to_packet_buffer(value);

	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_CONFIG, PI_CAMERA_ERROR_CODE_SUCCESS, &packet_buffer[0], static_cast<AL::uint32>(packet_buffer.GetSize()));
}
AL::uint8 pi_camera_net_begin_set_config(AL::Network::TcpSocket& socket, const pi_camera_config& value)
{
	auto packet_buffer = pi_camera_config_to_packet_buffer(value);

	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_CONFIG, PI_CAMERA_ERROR_CODE_SUCCESS, &packet_buffer[0], static_cast<AL::uint32>(packet_buffer.GetSize())))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_set_config(AL::Network::TcpSocket& socket, AL::uint8 error_code, const pi_camera_config& value)
{
	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_CONFIG, error_code, nullptr, 0);
}

AL::uint8 pi_camera_net_begin_get_contrast(AL::Network::TcpSocket& socket, AL::int8& value)
{
	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_CONTRAST, PI_CAMERA_ERROR_CODE_SUCCESS, nullptr, 0))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	value = static_cast<AL::int8>(packet_buffer[0]);

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_get_contrast(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::int8 value)
{
	if (error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_CONTRAST, error_code, nullptr, 0);

	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_CONTRAST, PI_CAMERA_ERROR_CODE_SUCCESS, &value, sizeof(AL::int8));
}
AL::uint8 pi_camera_net_begin_set_contrast(AL::Network::TcpSocket& socket, AL::int8 value)
{
	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_CONTRAST, PI_CAMERA_ERROR_CODE_SUCCESS, &value, sizeof(AL::int8)))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_set_contrast(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::int8 value)
{
	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_CONTRAST, error_code, nullptr, 0);
}

AL::uint8 pi_camera_net_begin_get_sharpness(AL::Network::TcpSocket& socket, AL::int8& value)
{
	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_SHARPNESS, PI_CAMERA_ERROR_CODE_SUCCESS, nullptr, 0))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	value = static_cast<AL::int8>(packet_buffer[0]);

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_get_sharpness(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::int8 value)
{
	if (error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_SHARPNESS, error_code, nullptr, 0);

	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_SHARPNESS, PI_CAMERA_ERROR_CODE_SUCCESS, &value, sizeof(AL::int8));
}
AL::uint8 pi_camera_net_begin_set_sharpness(AL::Network::TcpSocket& socket, AL::int8 value)
{
	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_SHARPNESS, PI_CAMERA_ERROR_CODE_SUCCESS, &value, sizeof(AL::int8)))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_set_sharpness(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::int8 value)
{
	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_SHARPNESS, error_code, nullptr, 0);
}

AL::uint8 pi_camera_net_begin_get_brightness(AL::Network::TcpSocket& socket, AL::uint8& value)
{
	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_BRIGHTNESS, PI_CAMERA_ERROR_CODE_SUCCESS, nullptr, 0))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	value = packet_buffer[0];

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_get_brightness(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::uint8 value)
{
	if (error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_BRIGHTNESS, error_code, nullptr, 0);

	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_BRIGHTNESS, PI_CAMERA_ERROR_CODE_SUCCESS, &value, sizeof(AL::uint8));
}
AL::uint8 pi_camera_net_begin_set_brightness(AL::Network::TcpSocket& socket, AL::uint8 value)
{
	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_BRIGHTNESS, PI_CAMERA_ERROR_CODE_SUCCESS, &value, sizeof(AL::uint8)))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_set_brightness(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::uint8 value)
{
	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_BRIGHTNESS, error_code, nullptr, 0);
}

AL::uint8 pi_camera_net_begin_get_saturation(AL::Network::TcpSocket& socket, AL::int8& value)
{
	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_SATURATION, PI_CAMERA_ERROR_CODE_SUCCESS, nullptr, 0))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	value = static_cast<AL::int8>(packet_buffer[0]);

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_get_saturation(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::int8 value)
{
	if (error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_SATURATION, error_code, nullptr, 0);

	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_SATURATION, PI_CAMERA_ERROR_CODE_SUCCESS, &value, sizeof(AL::int8));
}
AL::uint8 pi_camera_net_begin_set_saturation(AL::Network::TcpSocket& socket, AL::int8& value)
{
	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_SATURATION, PI_CAMERA_ERROR_CODE_SUCCESS, &value, sizeof(AL::int8)))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_set_saturation(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::int8 value)
{
	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_SATURATION, error_code, nullptr, 0);
}

AL::uint8 pi_camera_net_begin_get_white_balance(AL::Network::TcpSocket& socket, AL::uint8& value)
{
	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_WHITE_BALANCE, PI_CAMERA_ERROR_CODE_SUCCESS, nullptr, 0))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	value = packet_buffer[0];

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_get_white_balance(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::uint8 value)
{
	if (error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_WHITE_BALANCE, error_code, nullptr, 0);

	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_WHITE_BALANCE, PI_CAMERA_ERROR_CODE_SUCCESS, &value, sizeof(AL::uint8));
}
AL::uint8 pi_camera_net_begin_set_white_balance(AL::Network::TcpSocket& socket, AL::uint8 value)
{
	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_WHITE_BALANCE, PI_CAMERA_ERROR_CODE_SUCCESS, &value, sizeof(AL::uint8)))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_set_white_balance(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::uint8 value)
{
	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_WHITE_BALANCE, error_code, nullptr, 0);
}

AL::uint8 pi_camera_net_begin_get_shutter_speed(AL::Network::TcpSocket& socket, AL::TimeSpan& value)
{
	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_SHUTTER_SPEED, PI_CAMERA_ERROR_CODE_SUCCESS, nullptr, 0))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	value = AL::TimeSpan::FromNanoseconds(AL::BitConverter::NetworkToHost(*reinterpret_cast<const AL::uint64*>(&packet_buffer[0])));

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_get_shutter_speed(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::TimeSpan value)
{
	if (error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_SHUTTER_SPEED, error_code, nullptr, 0);

	auto time = AL::BitConverter::HostToNetwork(value.ToNanoseconds());

	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_SHUTTER_SPEED, PI_CAMERA_ERROR_CODE_SUCCESS, &time, sizeof(AL::uint64));
}
AL::uint8 pi_camera_net_begin_set_shutter_speed(AL::Network::TcpSocket& socket, AL::TimeSpan value)
{
	auto time = AL::BitConverter::HostToNetwork(value.ToNanoseconds());

	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_SHUTTER_SPEED, PI_CAMERA_ERROR_CODE_SUCCESS, &time, sizeof(AL::uint64)))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_set_shutter_speed(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::TimeSpan value)
{
	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_SHUTTER_SPEED, error_code, nullptr, 0);
}

AL::uint8 pi_camera_net_begin_get_exposure_mode(AL::Network::TcpSocket& socket, AL::uint8& value)
{
	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_EXPOSURE_MODE, PI_CAMERA_ERROR_CODE_SUCCESS, nullptr, 0))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	value = packet_buffer[0];

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_get_exposure_mode(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::uint8 value)
{
	if (error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_EXPOSURE_MODE, error_code, nullptr, 0);

	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_EXPOSURE_MODE, PI_CAMERA_ERROR_CODE_SUCCESS, &value, sizeof(AL::uint8));
}
AL::uint8 pi_camera_net_begin_set_exposure_mode(AL::Network::TcpSocket& socket, AL::uint8 value)
{
	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_EXPOSURE_MODE, PI_CAMERA_ERROR_CODE_SUCCESS, &value, sizeof(AL::uint8)))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_set_exposure_mode(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::uint8 value)
{
	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_EXPOSURE_MODE, error_code, nullptr, 0);
}

AL::uint8 pi_camera_net_begin_get_metoring_mode(AL::Network::TcpSocket& socket, AL::uint8& value)
{
	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_METORING_MODE, PI_CAMERA_ERROR_CODE_SUCCESS, nullptr, 0))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	value = packet_buffer[0];

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_get_metoring_mode(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::uint8 value)
{
	if (error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_METORING_MODE, error_code, nullptr, 0);

	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_METORING_MODE, PI_CAMERA_ERROR_CODE_SUCCESS, &value, sizeof(AL::uint8));
}
AL::uint8 pi_camera_net_begin_set_metoring_mode(AL::Network::TcpSocket& socket, AL::uint8 value)
{
	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_METORING_MODE, PI_CAMERA_ERROR_CODE_SUCCESS, &value, sizeof(AL::uint8)))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_set_metoring_mode(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::uint8 value)
{
	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_METORING_MODE, error_code, nullptr, 0);
}

AL::uint8 pi_camera_net_begin_get_jpg_quality(AL::Network::TcpSocket& socket, AL::uint8& value)
{
	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_JPG_QUALITY, PI_CAMERA_ERROR_CODE_SUCCESS, nullptr, 0))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	value = packet_buffer[0];

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_get_jpg_quality(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::uint8 value)
{
	if (error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_JPG_QUALITY, error_code, nullptr, 0);

	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_JPG_QUALITY, PI_CAMERA_ERROR_CODE_SUCCESS, &value, sizeof(AL::uint8));
}
AL::uint8 pi_camera_net_begin_set_jpg_quality(AL::Network::TcpSocket& socket, AL::uint8 value)
{
	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_JPG_QUALITY, PI_CAMERA_ERROR_CODE_SUCCESS, &value, sizeof(AL::uint8)))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_set_jpg_quality(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::uint8 value)
{
	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_JPG_QUALITY, error_code, nullptr, 0);
}

AL::uint8 pi_camera_net_begin_get_image_size(AL::Network::TcpSocket& socket, AL::uint16& width, AL::uint16& height)
{
	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_IMAGE_SIZE, PI_CAMERA_ERROR_CODE_SUCCESS, nullptr, 0))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	width  = AL::BitConverter::NetworkToHost(*reinterpret_cast<const AL::uint16*>(&packet_buffer[0]));
	height = AL::BitConverter::NetworkToHost(*reinterpret_cast<const AL::uint16*>(&packet_buffer[2]));

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_get_image_size(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::uint16 width, AL::uint16 height)
{
	if (error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_IMAGE_ROTATION, error_code, nullptr, 0);

	AL::uint16 packet_buffer[2] =
	{
		AL::BitConverter::HostToNetwork(width),
		AL::BitConverter::HostToNetwork(height)
	};

	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_IMAGE_ROTATION, PI_CAMERA_ERROR_CODE_SUCCESS, packet_buffer, sizeof(packet_buffer));
}
AL::uint8 pi_camera_net_begin_set_image_size(AL::Network::TcpSocket& socket, AL::uint16 width, AL::uint16 height)
{
	AL::uint16 buffer[2] =
	{
		AL::BitConverter::HostToNetwork(width),
		AL::BitConverter::HostToNetwork(height)
	};

	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_IMAGE_SIZE, PI_CAMERA_ERROR_CODE_SUCCESS, buffer, sizeof(buffer)))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_set_image_size(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::uint16 width, AL::uint16 height)
{
	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_IMAGE_SIZE, error_code, nullptr, 0);
}

AL::uint8 pi_camera_net_begin_get_image_effect(AL::Network::TcpSocket& socket, AL::uint8& value)
{
	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_IMAGE_EFFECT, PI_CAMERA_ERROR_CODE_SUCCESS, nullptr, 0))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	value = packet_buffer[0];

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_get_image_effect(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::uint8 value)
{
	if (error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_IMAGE_EFFECT, error_code, nullptr, 0);

	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_IMAGE_EFFECT, PI_CAMERA_ERROR_CODE_SUCCESS, &value, sizeof(AL::uint8));
}
AL::uint8 pi_camera_net_begin_set_image_effect(AL::Network::TcpSocket& socket, AL::uint8 value)
{
	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_IMAGE_EFFECT, PI_CAMERA_ERROR_CODE_SUCCESS, &value, sizeof(AL::uint8)))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_set_image_effect(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::uint8 value)
{
	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_IMAGE_EFFECT, error_code, nullptr, 0);
}

AL::uint8 pi_camera_net_begin_get_image_rotation(AL::Network::TcpSocket& socket, AL::uint16& value)
{
	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_IMAGE_ROTATION, PI_CAMERA_ERROR_CODE_SUCCESS, nullptr, 0))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	value = AL::BitConverter::NetworkToHost(*reinterpret_cast<const AL::uint16*>(&packet_buffer[0]));

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_get_image_rotation(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::uint16 value)
{
	if (error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_IMAGE_ROTATION, error_code, nullptr, 0);

	value = AL::BitConverter::HostToNetwork(value);

	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_GET_IMAGE_ROTATION, PI_CAMERA_ERROR_CODE_SUCCESS, &value, sizeof(AL::uint16));
}
AL::uint8 pi_camera_net_begin_set_image_rotation(AL::Network::TcpSocket& socket, AL::uint16 value)
{
	auto rotation = AL::BitConverter::HostToNetwork(value);

	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_IMAGE_ROTATION, PI_CAMERA_ERROR_CODE_SUCCESS, &rotation, sizeof(AL::uint16)))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_set_image_rotation(AL::Network::TcpSocket& socket, AL::uint8 error_code, AL::uint16 value)
{
	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_SET_IMAGE_ROTATION, error_code, nullptr, 0);
}

AL::uint8 pi_camera_net_begin_capture(AL::Network::TcpSocket& socket, const char* file_path)
{
	if (!pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_CAPTURE, PI_CAMERA_ERROR_CODE_SUCCESS, nullptr, 0))
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	if (pi_camera_net_receive_packet(socket, packet_header, packet_buffer, false) == 0)
		return PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED;

	if (packet_header.error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return packet_header.error_code;

	if (!pi_camera_file_write(file_path, &packet_buffer[0], packet_header.buffer_size))
		return PI_CAMERA_ERROR_CODE_FILE_WRITE_ERROR;

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
bool      pi_camera_net_complete_capture(AL::Network::TcpSocket& socket, AL::uint8 error_code, const char* file_path)
{
	if (error_code != PI_CAMERA_ERROR_CODE_SUCCESS)
		return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_CAPTURE, error_code, nullptr, 0);

	AL::uint32 file_size;

	if (!pi_camera_file_get_size(file_path, file_size))
		return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_CAPTURE, PI_CAMERA_ERROR_CODE_FILE_STAT_ERROR, nullptr, 0);

	pi_camera_packet_buffer packet_buffer(file_size);

	if (!pi_camera_file_read(file_path, &packet_buffer[0], file_size))
		return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_CAPTURE, PI_CAMERA_ERROR_CODE_FILE_READ_ERROR, nullptr, 0);

	return pi_camera_net_send_packet(socket, PI_CAMERA_OPCODE_CAPTURE, PI_CAMERA_ERROR_CODE_SUCCESS, &packet_buffer[0], packet_buffer.GetSize());
}

bool pi_camera_service_packet_handler_is_busy(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	bool      value;
	AL::uint8 error_code = pi_camera_is_busy(camera_service, &value);

	return pi_camera_net_complete_is_busy(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_get_ev(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	AL::int8  value;
	AL::uint8 error_code = pi_camera_get_ev(camera_service, &value);

	return pi_camera_net_complete_get_ev(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_set_ev(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	auto      value      = static_cast<AL::int8>(buffer[0]);
	AL::uint8 error_code = pi_camera_set_ev(camera_service, value);

	return pi_camera_net_complete_set_ev(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_get_iso(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	AL::uint16 value;
	AL::uint8  error_code = pi_camera_get_iso(camera_service, &value);

	return pi_camera_net_complete_get_iso(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_set_iso(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	auto      value      = AL::BitConverter::NetworkToHost(*reinterpret_cast<const AL::uint16*>(buffer));
	AL::uint8 error_code = pi_camera_set_iso(camera_service, value);

	return pi_camera_net_complete_set_iso(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_get_config(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	pi_camera_config value;
	AL::uint8        error_code = pi_camera_get_config(camera_service, &value);

	return pi_camera_net_complete_get_config(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_set_config(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	auto      value      = pi_camera_config_from_packet_buffer(buffer, size);
	AL::uint8 error_code = pi_camera_set_config(camera_service, &value);

	return pi_camera_net_complete_set_config(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_get_contrast(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	AL::int8  value;
	AL::uint8 error_code = pi_camera_get_contrast(camera_service, &value);

	return pi_camera_net_complete_get_contrast(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_set_contrast(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	auto      value      = static_cast<AL::int8>(buffer[0]);
	AL::uint8 error_code = pi_camera_set_contrast(camera_service, value);

	return pi_camera_net_complete_set_contrast(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_get_sharpness(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	AL::int8  value;
	AL::uint8 error_code = pi_camera_get_sharpness(camera_service, &value);

	return pi_camera_net_complete_get_sharpness(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_set_sharpness(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	auto      value      = static_cast<AL::int8>(buffer[0]);
	AL::uint8 error_code = pi_camera_set_sharpness(camera_service, value);

	return pi_camera_net_complete_set_sharpness(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_get_brightness(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	AL::uint8 value;
	AL::uint8 error_code = pi_camera_get_brightness(camera_service, &value);

	return pi_camera_net_complete_get_brightness(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_set_brightness(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	auto      value      = buffer[0];
	AL::uint8 error_code = pi_camera_set_brightness(camera_service, value);

	return pi_camera_net_complete_set_brightness(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_get_saturation(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	AL::int8  value;
	AL::uint8 error_code = pi_camera_get_saturation(camera_service, &value);

	return pi_camera_net_complete_get_saturation(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_set_saturation(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	auto      value      = static_cast<AL::int8>(buffer[0]);
	AL::uint8 error_code = pi_camera_set_saturation(camera_service, value);

	return pi_camera_net_complete_set_saturation(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_get_white_balance(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	AL::uint8 value;
	AL::uint8 error_code = pi_camera_get_white_balance(camera_service, &value);

	return pi_camera_net_complete_get_white_balance(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_set_white_balance(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	auto      value      = buffer[0];
	AL::uint8 error_code = pi_camera_set_white_balance(camera_service, value);

	return pi_camera_net_complete_set_white_balance(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_get_shutter_speed(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	AL::TimeSpan value;
	AL::uint8    error_code = pi_camera_get_shutter_speed(camera_service, &value);

	return pi_camera_net_complete_get_shutter_speed(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_set_shutter_speed(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	auto      value      = AL::TimeSpan::FromNanoseconds(AL::BitConverter::NetworkToHost(*reinterpret_cast<const AL::uint64*>(buffer)));
	AL::uint8 error_code = pi_camera_set_shutter_speed(camera_service, value);

	return pi_camera_net_complete_set_shutter_speed(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_get_exposure_mode(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	AL::uint8 value;
	AL::uint8 error_code = pi_camera_get_exposure_mode(camera_service, &value);

	return pi_camera_net_complete_get_exposure_mode(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_set_exposure_mode(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	auto      value      = buffer[0];
	AL::uint8 error_code = pi_camera_set_exposure_mode(camera_service, value);

	return pi_camera_net_complete_set_exposure_mode(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_get_metoring_mode(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	AL::uint8 value;
	AL::uint8 error_code = pi_camera_get_metoring_mode(camera_service, &value);

	return pi_camera_net_complete_get_metoring_mode(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_set_metoring_mode(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	auto      value      = buffer[0];
	AL::uint8 error_code = pi_camera_set_metoring_mode(camera_service, value);

	return pi_camera_net_complete_set_metoring_mode(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_get_jpg_quality(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	AL::uint8 value;
	AL::uint8 error_code = pi_camera_get_jpg_quality(camera_service, &value);

	return pi_camera_net_complete_get_jpg_quality(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_set_jpg_quality(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	auto      value      = buffer[0];
	AL::uint8 error_code = pi_camera_set_jpg_quality(camera_service, value);

	return pi_camera_net_complete_set_jpg_quality(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_get_image_size(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	AL::uint16 width, height;
	AL::uint8  error_code = pi_camera_get_image_size(camera_service, &width, &height);

	return pi_camera_net_complete_get_image_size(camera_session->socket, error_code, width, height);
}
bool pi_camera_service_packet_handler_set_image_size(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	auto      width      = AL::BitConverter::NetworkToHost(*reinterpret_cast<const AL::uint16*>(buffer));
	auto      height     = AL::BitConverter::NetworkToHost(*reinterpret_cast<const AL::uint16*>(&buffer[2]));
	AL::uint8 error_code = pi_camera_set_image_size(camera_service, width, height);

	return pi_camera_net_complete_set_image_size(camera_session->socket, error_code, width, height);
}
bool pi_camera_service_packet_handler_get_image_effect(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	AL::uint8 value;
	AL::uint8 error_code = pi_camera_get_image_effect(camera_service, &value);

	return pi_camera_net_complete_get_image_effect(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_set_image_effect(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	auto      value      = buffer[0];
	AL::uint8 error_code = pi_camera_set_image_effect(camera_service, value);

	return pi_camera_net_complete_set_image_effect(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_get_image_rotation(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	AL::uint16 value;
	AL::uint8  error_code = pi_camera_get_image_rotation(camera_service, &value);

	return pi_camera_net_complete_get_image_rotation(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_set_image_rotation(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	auto      value      = AL::BitConverter::NetworkToHost(*reinterpret_cast<const AL::uint16*>(buffer));
	AL::uint8 error_code = pi_camera_set_image_rotation(camera_service, value);

	return pi_camera_net_complete_set_image_rotation(camera_session->socket, error_code, value);
}
bool pi_camera_service_packet_handler_capture(pi_camera_service* camera_service, pi_camera_session* camera_session, const pi_camera_packet_header& header, const AL::uint8* buffer, AL::size_t size)
{
	auto      file_path  = AL::String::Format("./pi_image_%lu.jpg", ++camera_service->image_counter);
	AL::uint8 error_code = pi_camera_capture(camera_service, file_path.GetCString());
	bool      result     = pi_camera_net_complete_capture(camera_session->socket, error_code, file_path.GetCString());

	pi_camera_file_delete(file_path.GetCString());

	return result;
}

constexpr pi_camera_service_packet_handler_context pi_camera_service_packet_handlers[PI_CAMERA_OPCODE_COUNT] =
{
	{ PI_CAMERA_OPCODE_IS_BUSY,            &pi_camera_service_packet_handler_is_busy },

	{ PI_CAMERA_OPCODE_GET_EV,             &pi_camera_service_packet_handler_get_ev },
	{ PI_CAMERA_OPCODE_SET_EV,             &pi_camera_service_packet_handler_set_ev },

	{ PI_CAMERA_OPCODE_GET_ISO,            &pi_camera_service_packet_handler_get_iso },
	{ PI_CAMERA_OPCODE_SET_ISO,            &pi_camera_service_packet_handler_set_iso },

	{ PI_CAMERA_OPCODE_GET_CONFIG,         &pi_camera_service_packet_handler_get_config },
	{ PI_CAMERA_OPCODE_SET_CONFIG,         &pi_camera_service_packet_handler_set_config },

	{ PI_CAMERA_OPCODE_GET_CONTRAST,       &pi_camera_service_packet_handler_get_contrast },
	{ PI_CAMERA_OPCODE_SET_CONTRAST,       &pi_camera_service_packet_handler_set_contrast },

	{ PI_CAMERA_OPCODE_GET_SHARPNESS,      &pi_camera_service_packet_handler_get_sharpness },
	{ PI_CAMERA_OPCODE_SET_SHARPNESS,      &pi_camera_service_packet_handler_set_sharpness },

	{ PI_CAMERA_OPCODE_GET_BRIGHTNESS,     &pi_camera_service_packet_handler_get_brightness },
	{ PI_CAMERA_OPCODE_SET_BRIGHTNESS,     &pi_camera_service_packet_handler_set_brightness },

	{ PI_CAMERA_OPCODE_GET_SATURATION,     &pi_camera_service_packet_handler_get_saturation },
	{ PI_CAMERA_OPCODE_SET_SATURATION,     &pi_camera_service_packet_handler_set_saturation },

	{ PI_CAMERA_OPCODE_GET_WHITE_BALANCE,  &pi_camera_service_packet_handler_get_white_balance },
	{ PI_CAMERA_OPCODE_SET_WHITE_BALANCE,  &pi_camera_service_packet_handler_set_white_balance },

	{ PI_CAMERA_OPCODE_GET_SHUTTER_SPEED,  &pi_camera_service_packet_handler_get_shutter_speed },
	{ PI_CAMERA_OPCODE_SET_SHUTTER_SPEED,  &pi_camera_service_packet_handler_set_shutter_speed },

	{ PI_CAMERA_OPCODE_GET_EXPOSURE_MODE,  &pi_camera_service_packet_handler_get_exposure_mode },
	{ PI_CAMERA_OPCODE_SET_EXPOSURE_MODE,  &pi_camera_service_packet_handler_set_exposure_mode },

	{ PI_CAMERA_OPCODE_GET_METORING_MODE,  &pi_camera_service_packet_handler_get_metoring_mode },
	{ PI_CAMERA_OPCODE_SET_METORING_MODE,  &pi_camera_service_packet_handler_set_metoring_mode },

	{ PI_CAMERA_OPCODE_GET_JPG_QUALITY,    &pi_camera_service_packet_handler_get_jpg_quality },
	{ PI_CAMERA_OPCODE_SET_JPG_QUALITY,    &pi_camera_service_packet_handler_set_jpg_quality },

	{ PI_CAMERA_OPCODE_GET_IMAGE_SIZE,     &pi_camera_service_packet_handler_get_image_size },
	{ PI_CAMERA_OPCODE_SET_IMAGE_SIZE,     &pi_camera_service_packet_handler_set_image_size },

	{ PI_CAMERA_OPCODE_GET_IMAGE_EFFECT,   &pi_camera_service_packet_handler_get_image_effect },
	{ PI_CAMERA_OPCODE_SET_IMAGE_EFFECT,   &pi_camera_service_packet_handler_set_image_effect },

	{ PI_CAMERA_OPCODE_GET_IMAGE_ROTATION, &pi_camera_service_packet_handler_get_image_rotation },
	{ PI_CAMERA_OPCODE_SET_IMAGE_ROTATION, &pi_camera_service_packet_handler_set_image_rotation },

	{ PI_CAMERA_OPCODE_CAPTURE,            &pi_camera_service_packet_handler_capture }
};

pi_camera_session* pi_camera_open_session(pi_camera_service* camera_service, AL::Network::TcpSocket&& socket);

bool pi_camera_service_accept_session(pi_camera_service* camera_service, pi_camera_session*& camera_session)
{
	AL::Network::TcpSocket socket(camera_service->socket.GetAddressFamily());

	switch (pi_camera_net_socket_accept(camera_service->socket, socket))
	{
		case 0:                            return false;
		case -1: camera_session = nullptr; return true;
	}

	camera_session = pi_camera_open_session(camera_service, AL::Move(socket));

	return true;
}
bool pi_camera_service_update_session(pi_camera_service* camera_service, pi_camera_session* camera_session)
{
	pi_camera_packet_header packet_header;
	pi_camera_packet_buffer packet_buffer;

	switch (pi_camera_net_receive_packet(camera_session->socket, packet_header, packet_buffer))
	{
		case 0:  return false;
		case -1: return true;
	}

	if (packet_header.opcode >= PI_CAMERA_OPCODE_COUNT)
	{
		pi_camera_net_socket_close(camera_session->socket);

		return false;
	}

	if (!pi_camera_service_packet_handlers[packet_header.opcode].packet_handler(camera_service, camera_session, packet_header, &packet_buffer[0], packet_header.buffer_size))
	{
		pi_camera_net_socket_close(camera_session->socket);

		return false;
	}

	return true;
}
bool pi_camera_service_update(pi_camera_service* camera_service)
{
	pi_camera_session* camera_session;

	while (camera_service->sessions.GetSize() < camera_service->max_connections)
	{
		if (!pi_camera_service_accept_session(camera_service, camera_session))
			return false;

		if (camera_session == nullptr)
			break;

		camera_service->sessions.PushBack(camera_session);
	}

	for (auto it = camera_service->sessions.begin(); it != camera_service->sessions.end(); )
	{
		if (!pi_camera_service_update_session(camera_service, *it))
		{
			camera_service->sessions.Erase(it++);

			continue;
		}

		++it;
	}

	return true;
}
void pi_camera_service_thread_main(pi_camera_service* camera_service)
{
	AL::Game::Loop::Run(PI_CAMERA_SERVICE_TICK_RATE, [camera_service](AL::TimeSpan delta)
	{
		if (camera_service->is_thread_stopping)
			return AL::False;

		if (!pi_camera_service_update(camera_service))
			return AL::False;

		return AL::True;
	});
}
bool pi_camera_service_thread_start(pi_camera_service* camera_service)
{
	try
	{
		camera_service->thread.Start([camera_service]()
		{
			pi_camera_service_thread_main(camera_service);
		});
	}
	catch (const AL::Exception& exception)
	{

		return false;
	}

	return true;
}
void pi_camera_service_thread_stop(pi_camera_service* camera_service)
{
	camera_service->is_thread_stopping = true;

	try
	{
		while (!camera_service->thread.Join())
		{
		}
	}
	catch (const AL::Exception& exception)
	{
	}

	camera_service->is_thread_stopping = false;
}
bool pi_camera_service_start(pi_camera_service* camera_service)
{
	if (!pi_camera_net_socket_listen(camera_service->socket, camera_service->local_end_point, camera_service->max_connections))
		return false;

	if (!pi_camera_service_thread_start(camera_service))
	{
		pi_camera_net_socket_close(camera_service->socket);

		return false;
	}

	return true;
}
void pi_camera_service_stop(pi_camera_service* camera_service)
{
	pi_camera_service_thread_stop(camera_service);
	pi_camera_net_socket_close(camera_service->socket);

	for (auto it = camera_service->sessions.begin(); it != camera_service->sessions.end(); )
	{
		pi_camera_close(*it);
		camera_service->sessions.Erase(it++);
	}
}

inline auto pi_camera_clamp_ev(AL::int8 value)
{
	return AL::Math::Clamp<AL::int8>(value, PI_CAMERA_EV_MIN, PI_CAMERA_EV_MAX);
}
inline auto pi_camera_clamp_iso(AL::uint16 value)
{
	return AL::Math::Clamp<AL::uint16>(value, PI_CAMERA_ISO_MIN, PI_CAMERA_ISO_MAX);
}
inline auto pi_camera_clamp_contrast(AL::int8 value)
{
	return AL::Math::Clamp<AL::int8>(value, PI_CAMERA_CONTRAST_MIN, PI_CAMERA_CONTRAST_MIN);
}
inline auto pi_camera_clamp_sharpness(AL::int8 value)
{
	return AL::Math::Clamp<AL::int8>(value, PI_CAMERA_SHARPNESS_MIN, PI_CAMERA_SHARPNESS_MIN);
}
inline auto pi_camera_clamp_brightness(AL::uint8 value)
{
	return AL::Math::Clamp<AL::uint8>(value, PI_CAMERA_BRIGHTNESS_MIN, PI_CAMERA_BRIGHTNESS_MIN);
}
inline auto pi_camera_clamp_saturation(AL::int8 value)
{
	return AL::Math::Clamp<AL::int8>(value, PI_CAMERA_SATURATION_MIN, PI_CAMERA_SATURATION_MIN);
}
inline auto pi_camera_clamp_shutter_speed(AL::TimeSpan value)
{
	return value;
}
inline auto pi_camera_clamp_jpg_quality(AL::uint8 value)
{
	return AL::Math::Clamp<AL::uint8>(value, PI_CAMERA_JPG_QUALITY_MIN, PI_CAMERA_JPG_QUALITY_MAX);
}
inline auto pi_camera_clamp_image_size_width(AL::uint16 value)
{
	return AL::Math::Clamp<AL::uint16>(value, 0, PI_CAMERA_IMAGE_SIZE_WIDTH_MAX);
}
inline auto pi_camera_clamp_image_size_height(AL::uint16 value)
{
	return AL::Math::Clamp<AL::uint16>(value, 0, PI_CAMERA_IMAGE_SIZE_HEIGHT_MAX);
}
inline auto pi_camera_clamp_image_rotation(AL::uint16 value)
{
	return AL::Math::Clamp<AL::uint16>(value, PI_CAMERA_IMAGE_ROTATION_MIN, PI_CAMERA_IMAGE_ROTATION_MAX);
}

AL::uint8 pi_camera_cli_execute(pi_camera_local* camera_local, const char* file_path)
{
	if (camera_local->is_busy)
		return PI_CAMERA_ERROR_CODE_CAMERA_BUSY;

	try
	{
		AL::OS::Shell::Execute(
			"raspistill",
			AL::String::Format("%s -o %s", camera_local->cli_params.GetCString(), file_path)
		);
	}
	catch (const AL::Exception& exception)
	{

		return PI_CAMERA_ERROR_CODE_CAMERA_FAILED;
	}

	return PI_CAMERA_ERROR_CODE_SUCCESS;
}
template<typename T>
void      pi_camera_cli_build_params_append(AL::StringBuilder& sb, const char* key, T value)
{
	if (sb.GetLength() != 0)
		sb.Append(' ');

	sb << key << ' ' << value;
}
void      pi_camera_cli_build_params_append_ev(AL::StringBuilder& sb, const pi_camera_config& camera_config)
{
	pi_camera_cli_build_params_append(sb, "-ev", camera_config.ev);
}
void      pi_camera_cli_build_params_append_iso(AL::StringBuilder& sb, const pi_camera_config& camera_config)
{
	pi_camera_cli_build_params_append(sb, "-ISO", camera_config.iso);
}
void      pi_camera_cli_build_params_append_contrast(AL::StringBuilder& sb, const pi_camera_config& camera_config)
{
	pi_camera_cli_build_params_append(sb, "-co", camera_config.contrast);
}
void      pi_camera_cli_build_params_append_sharpness(AL::StringBuilder& sb, const pi_camera_config& camera_config)
{
	pi_camera_cli_build_params_append(sb, "-sh", camera_config.sharpness);
}
void      pi_camera_cli_build_params_append_brightness(AL::StringBuilder& sb, const pi_camera_config& camera_config)
{
	pi_camera_cli_build_params_append(sb, "-br", camera_config.brightness);
}
void      pi_camera_cli_build_params_append_saturation(AL::StringBuilder& sb, const pi_camera_config& camera_config)
{
	pi_camera_cli_build_params_append(sb, "-sa", camera_config.saturation);
}
void      pi_camera_cli_build_params_append_white_balance(AL::StringBuilder& sb, const pi_camera_config& camera_config)
{
	switch (camera_config.white_balance)
	{
		case PI_CAMERA_WHITE_BALANCE_OFF:
			pi_camera_cli_build_params_append(sb, "-awb", "off");
			break;

		case PI_CAMERA_WHITE_BALANCE_AUTO:
			pi_camera_cli_build_params_append(sb, "-awb", "auto");
			break;

		case PI_CAMERA_WHITE_BALANCE_SUN:
			pi_camera_cli_build_params_append(sb, "-awb", "sun");
			break;

		case PI_CAMERA_WHITE_BALANCE_FLASH:
			pi_camera_cli_build_params_append(sb, "-awb", "flash");
			break;

		case PI_CAMERA_WHITE_BALANCE_SHADE:
			pi_camera_cli_build_params_append(sb, "-awb", "cloudshade");
			break;

		case PI_CAMERA_WHITE_BALANCE_CLOUDS:
			pi_camera_cli_build_params_append(sb, "-awb", "cloudshade");
			break;

		case PI_CAMERA_WHITE_BALANCE_HORIZON:
			pi_camera_cli_build_params_append(sb, "-awb", "horizon");
			break;

		case PI_CAMERA_WHITE_BALANCE_TUNGSTEN:
			pi_camera_cli_build_params_append(sb, "-awb", "tungsten");
			break;

		case PI_CAMERA_WHITE_BALANCE_FLUORESCENT:
			pi_camera_cli_build_params_append(sb, "-awb", "fluorescent");
			break;

		case PI_CAMERA_WHITE_BALANCE_INCANDESCENT:
			pi_camera_cli_build_params_append(sb, "-awb", "incandescent");
			break;
	}
}
void      pi_camera_cli_build_params_append_shutter_speed(AL::StringBuilder& sb, const pi_camera_config& camera_config)
{
	if (camera_config.shutter_speed.ToMicroseconds() != 0)
		pi_camera_cli_build_params_append(sb, "-ss", camera_config.shutter_speed.ToMicroseconds());
}
void      pi_camera_cli_build_params_append_exposure_mode(AL::StringBuilder& sb, const pi_camera_config& camera_config)
{
	switch (camera_config.exposure_mode)
	{
		case PI_CAMERA_EXPOSURE_MODE_OFF:
			pi_camera_cli_build_params_append(sb, "-ex", "off");
			break;

		case PI_CAMERA_EXPOSURE_MODE_AUTO:
			pi_camera_cli_build_params_append(sb, "-ex", "auto");
			break;

		case PI_CAMERA_EXPOSURE_MODE_SNOW:
			pi_camera_cli_build_params_append(sb, "-ex", "snow");
			break;

		case PI_CAMERA_EXPOSURE_MODE_BEACH:
			pi_camera_cli_build_params_append(sb, "-ex", "beach");
			break;

		case PI_CAMERA_EXPOSURE_MODE_NIGHT:
			pi_camera_cli_build_params_append(sb, "-ex", "night");
			break;

		case PI_CAMERA_EXPOSURE_MODE_SPORTS:
			pi_camera_cli_build_params_append(sb, "-ex", "sports");
			break;

		case PI_CAMERA_EXPOSURE_MODE_BACKLIGHT:
			pi_camera_cli_build_params_append(sb, "-ex", "backlight");
			break;

		case PI_CAMERA_EXPOSURE_MODE_SPOTLIGHT:
			pi_camera_cli_build_params_append(sb, "-ex", "spotlight");
			break;

		case PI_CAMERA_EXPOSURE_MODE_VERY_LONG:
			pi_camera_cli_build_params_append(sb, "-ex", "verylong");
			break;

		case PI_CAMERA_EXPOSURE_MODE_FIXED_FPS:
			pi_camera_cli_build_params_append(sb, "-ex", "fixedfps");
			break;

		case PI_CAMERA_EXPOSURE_MODE_FIREWORKS:
			pi_camera_cli_build_params_append(sb, "-ex", "fireworks");
			break;

		case PI_CAMERA_EXPOSURE_MODE_ANTI_SHAKE:
			pi_camera_cli_build_params_append(sb, "-ex", "antishake");
			break;

		case PI_CAMERA_EXPOSURE_MODE_NIGHT_PREVIEW:
			pi_camera_cli_build_params_append(sb, "-ex", "nightpreview");
			break;
	}
}
void      pi_camera_cli_build_params_append_metoring_mode(AL::StringBuilder& sb, const pi_camera_config& camera_config)
{
	switch (camera_config.metoring_mode)
	{
		case PI_CAMERA_METORING_MODE_SPOT:
			pi_camera_cli_build_params_append(sb, "-mm", "spot");
			break;

		case PI_CAMERA_METORING_MODE_MATRIX:
			pi_camera_cli_build_params_append(sb, "-mm", "matrix");
			break;

		case PI_CAMERA_METORING_MODE_AVERAGE:
			pi_camera_cli_build_params_append(sb, "-mm", "average");
			break;

		case PI_CAMERA_METORING_MODE_BACKLIT:
			pi_camera_cli_build_params_append(sb, "-mm", "backlit");
			break;
	}
}
void      pi_camera_cli_build_params_append_jpg_quality(AL::StringBuilder& sb, const pi_camera_config& camera_config)
{
	pi_camera_cli_build_params_append(sb, "-q", camera_config.jpg_quality);
}
void      pi_camera_cli_build_params_append_image_size(AL::StringBuilder& sb, const pi_camera_config& camera_config)
{
	pi_camera_cli_build_params_append(sb, "-w", camera_config.image_size_width);
	pi_camera_cli_build_params_append(sb, "-h", camera_config.image_size_height);
}
void      pi_camera_cli_build_params_append_image_effect(AL::StringBuilder& sb, const pi_camera_config& camera_config)
{
	switch (camera_config.image_effect)
	{
		case PI_CAMERA_IMAGE_EFFECT_NONE:
			break;

		case PI_CAMERA_IMAGE_EFFECT_NEGATIVE:
			pi_camera_cli_build_params_append(sb, "-ifx", "negative");
			break;

		case PI_CAMERA_IMAGE_EFFECT_SOLARISE:
			pi_camera_cli_build_params_append(sb, "-ifx", "solarise");
			break;

		case PI_CAMERA_IMAGE_EFFECT_WHITEBOARD:
			pi_camera_cli_build_params_append(sb, "-ifx", "whiteboard");
			break;

		case PI_CAMERA_IMAGE_EFFECT_BLACKBOARD:
			pi_camera_cli_build_params_append(sb, "-ifx", "blackboard");
			break;

		case PI_CAMERA_IMAGE_EFFECT_SKETCH:
			pi_camera_cli_build_params_append(sb, "-ifx", "sketch");
			break;

		case PI_CAMERA_IMAGE_EFFECT_DENOISE:
			pi_camera_cli_build_params_append(sb, "-ifx", "denoise");
			break;

		case PI_CAMERA_IMAGE_EFFECT_EMBOSS:
			pi_camera_cli_build_params_append(sb, "-ifx", "emboss");
			break;

		case PI_CAMERA_IMAGE_EFFECT_OIL_PAINT:
			pi_camera_cli_build_params_append(sb, "-ifx", "oilpaint");
			break;

		case PI_CAMERA_IMAGE_EFFECT_GRAPHITE_SKETCH:
			pi_camera_cli_build_params_append(sb, "-ifx", "gpen");
			break;

		case PI_CAMERA_IMAGE_EFFECT_CROSS_HATCH_SKETCH:
			pi_camera_cli_build_params_append(sb, "-ifx", "hatch");
			break;

		case PI_CAMERA_IMAGE_EFFECT_PASTEL:
			pi_camera_cli_build_params_append(sb, "-ifx", "pastel");
			break;

		case PI_CAMERA_IMAGE_EFFECT_WATERCOLOR:
			pi_camera_cli_build_params_append(sb, "-ifx", "watercolour");
			break;

		case PI_CAMERA_IMAGE_EFFECT_FILM:
			pi_camera_cli_build_params_append(sb, "-ifx", "film");
			break;

		case PI_CAMERA_IMAGE_EFFECT_BLUR:
			pi_camera_cli_build_params_append(sb, "-ifx", "blur");
			break;

		case PI_CAMERA_IMAGE_EFFECT_SATURATE:
			pi_camera_cli_build_params_append(sb, "-ifx", "saturation");
			break;
	}
}
void      pi_camera_cli_build_params_append_image_rotation(AL::StringBuilder& sb, const pi_camera_config& camera_config)
{
	pi_camera_cli_build_params_append(sb, "-rot", camera_config.image_rotation);
}
void      pi_camera_cli_build_params(pi_camera_local* camera_local)
{
	// https://www.raspberrypi.org/app/uploads/2013/07/RaspiCam-Documentation.pdf
	// https://github.com/raspberrypi/userland/blob/master/host_applications/linux/apps/raspicam/RaspiStill.c

	AL::StringBuilder sb;

	pi_camera_cli_build_params_append_ev(sb, camera_local->config);
	pi_camera_cli_build_params_append_iso(sb, camera_local->config);
	pi_camera_cli_build_params_append_contrast(sb, camera_local->config);
	pi_camera_cli_build_params_append_sharpness(sb, camera_local->config);
	pi_camera_cli_build_params_append_brightness(sb, camera_local->config);
	pi_camera_cli_build_params_append_saturation(sb, camera_local->config);
	pi_camera_cli_build_params_append_white_balance(sb, camera_local->config);
	pi_camera_cli_build_params_append_shutter_speed(sb, camera_local->config);
	pi_camera_cli_build_params_append_exposure_mode(sb, camera_local->config);
	pi_camera_cli_build_params_append_metoring_mode(sb, camera_local->config);
	pi_camera_cli_build_params_append_jpg_quality(sb, camera_local->config);
	pi_camera_cli_build_params_append_image_size(sb, camera_local->config);
	pi_camera_cli_build_params_append_image_effect(sb, camera_local->config);
	pi_camera_cli_build_params_append_image_rotation(sb, camera_local->config);

	camera_local->cli_params = sb.ToString();
}

const char* PI_CAMERA_API_CALL pi_camera_get_error_string(AL::uint8 value)
{
	switch (value)
	{
		case PI_CAMERA_ERROR_CODE_SUCCESS:           return "Success";
		case PI_CAMERA_ERROR_CODE_CAMERA_BUSY:       return "Camera busy";
		case PI_CAMERA_ERROR_CODE_CAMERA_FAILED:     return "Camera failed";
		case PI_CAMERA_ERROR_CODE_FILE_STAT_ERROR:   return "File stat error";
		case PI_CAMERA_ERROR_CODE_FILE_READ_ERROR:   return "File read error";
		case PI_CAMERA_ERROR_CODE_FILE_WRITE_ERROR:  return "File write error";
		case PI_CAMERA_ERROR_CODE_CONNECTION_CLOSED: return "Connection closed";
	}

	return "Undefined";
}

pi_camera* PI_CAMERA_API_CALL  pi_camera_open()
{
	auto camera = new pi_camera_local();

	pi_camera_cli_build_params(camera);

	return camera;
}
pi_camera* PI_CAMERA_API_CALL  pi_camera_open_remote(const char* remote_host, AL::uint16 remote_port)
{
	AL::Network::IPEndPoint remote_end_point;

	if (!pi_camera_net_socket_resolve_end_point(remote_end_point, remote_host, remote_port))
		return nullptr;

	auto camera = new pi_camera_remote(AL::Move(remote_end_point));

	if (!pi_camera_net_socket_connect(camera->socket, camera->remote_end_point))
	{
		delete camera;

		return nullptr;
	}

	return camera;
}
pi_camera* PI_CAMERA_API_CALL  pi_camera_open_service(const char* local_host, AL::uint16 local_port, AL::size_t max_connections)
{
	AL::Network::IPEndPoint local_end_point;

	if (!pi_camera_net_socket_resolve_end_point(local_end_point, local_host, local_port))
		return nullptr;

	auto camera = new pi_camera_service(AL::Move(local_end_point), max_connections);

	pi_camera_cli_build_params(&camera->local);

	if (!pi_camera_service_start(camera))
	{
		delete camera;

		return nullptr;
	}

	return camera;
}
pi_camera_session*             pi_camera_open_session(pi_camera_service* camera_service, AL::Network::TcpSocket&& socket)
{
	auto camera_session = new pi_camera_session(camera_service, AL::Move(socket));

	return camera_session;
}
void       PI_CAMERA_API_CALL  pi_camera_close(pi_camera* camera)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			break;

		case PI_CAMERA_TYPE_REMOTE:
			pi_camera_net_socket_close(static_cast<pi_camera_remote*>(camera)->socket);
			break;

		case PI_CAMERA_TYPE_SERVICE:
			pi_camera_service_stop(static_cast<pi_camera_service*>(camera));
			break;

		case PI_CAMERA_TYPE_SESSION:
			break;
	}

	delete camera;
}

AL::uint8  PI_CAMERA_API_CALL  pi_camera_is_busy(pi_camera* camera, bool* value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			*value = static_cast<pi_camera_local*>(camera)->is_busy;
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_is_busy(static_cast<pi_camera_remote*>(camera)->socket, *value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_is_busy(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_is_busy(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}
bool       PI_CAMERA_API_CALL  pi_camera_is_remote(pi_camera* camera)
{
	return camera->type == PI_CAMERA_TYPE_REMOTE;
}
bool       PI_CAMERA_API_CALL  pi_camera_is_service(pi_camera* camera)
{
	return camera->type == PI_CAMERA_TYPE_SERVICE;
}
bool                           pi_camera_is_session(pi_camera* camera)
{
	return camera->type == PI_CAMERA_TYPE_SESSION;
}
bool       PI_CAMERA_API_CALL  pi_camera_is_connected(pi_camera* camera)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			return false;

		case PI_CAMERA_TYPE_REMOTE:
			return static_cast<pi_camera_remote*>(camera)->socket.IsConnected();

		case PI_CAMERA_TYPE_SERVICE:
			return false;

		case PI_CAMERA_TYPE_SESSION:
			return static_cast<pi_camera_session*>(camera)->socket.IsConnected();
	}

	return false;
}

AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_ev(pi_camera* camera, AL::int8* value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			*value = static_cast<pi_camera_local*>(camera)->config.ev;
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_get_ev(static_cast<pi_camera_remote*>(camera)->socket, *value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_get_ev(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_get_ev(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}
AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_ev(pi_camera* camera, AL::int8 value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			static_cast<pi_camera_local*>(camera)->config.ev = pi_camera_clamp_ev(value);
			pi_camera_cli_build_params(static_cast<pi_camera_local*>(camera));
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_set_ev(static_cast<pi_camera_remote*>(camera)->socket, value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_set_ev(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_set_ev(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}

AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_iso(pi_camera* camera, AL::uint16* value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			*value = static_cast<pi_camera_local*>(camera)->config.iso;
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_get_iso(static_cast<pi_camera_remote*>(camera)->socket, *value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_get_iso(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_get_iso(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}
AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_iso(pi_camera* camera, AL::uint16 value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			static_cast<pi_camera_local*>(camera)->config.iso = pi_camera_clamp_iso(value);
			pi_camera_cli_build_params(static_cast<pi_camera_local*>(camera));
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_set_iso(static_cast<pi_camera_remote*>(camera)->socket, value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_set_iso(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_set_iso(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}

AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_config(pi_camera* camera, pi_camera_config* value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			*value = static_cast<pi_camera_local*>(camera)->config;
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_get_config(static_cast<pi_camera_remote*>(camera)->socket, *value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_get_config(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_get_config(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}
AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_config(pi_camera* camera, const pi_camera_config* value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			static_cast<pi_camera_local*>(camera)->config.ev                = pi_camera_clamp_ev(value->ev);
			static_cast<pi_camera_local*>(camera)->config.iso               = pi_camera_clamp_iso(value->iso);
			static_cast<pi_camera_local*>(camera)->config.contrast          = pi_camera_clamp_contrast(value->contrast);
			static_cast<pi_camera_local*>(camera)->config.sharpness         = pi_camera_clamp_sharpness(value->sharpness);
			static_cast<pi_camera_local*>(camera)->config.brightness        = pi_camera_clamp_brightness(value->brightness);
			static_cast<pi_camera_local*>(camera)->config.saturation        = pi_camera_clamp_saturation(value->saturation);
			static_cast<pi_camera_local*>(camera)->config.white_balance     = value->white_balance;
			static_cast<pi_camera_local*>(camera)->config.shutter_speed     = pi_camera_clamp_shutter_speed(value->shutter_speed);
			static_cast<pi_camera_local*>(camera)->config.exposure_mode     = value->exposure_mode;
			static_cast<pi_camera_local*>(camera)->config.metoring_mode     = value->metoring_mode;
			static_cast<pi_camera_local*>(camera)->config.jpg_quality       = pi_camera_clamp_jpg_quality(value->jpg_quality);
			static_cast<pi_camera_local*>(camera)->config.image_size_width  = pi_camera_clamp_image_size_width(value->image_size_width);
			static_cast<pi_camera_local*>(camera)->config.image_size_height = pi_camera_clamp_image_size_height(value->image_size_height);
			static_cast<pi_camera_local*>(camera)->config.image_effect      = value->image_effect;
			static_cast<pi_camera_local*>(camera)->config.image_rotation    = pi_camera_clamp_image_rotation(value->image_rotation);
			pi_camera_cli_build_params(static_cast<pi_camera_local*>(camera));
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_set_config(static_cast<pi_camera_remote*>(camera)->socket, *value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_set_config(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_set_config(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}

AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_contrast(pi_camera* camera, AL::int8* value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			*value = static_cast<pi_camera_local*>(camera)->config.contrast;
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_get_contrast(static_cast<pi_camera_remote*>(camera)->socket, *value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_get_contrast(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_get_contrast(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}
AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_contrast(pi_camera* camera, AL::int8 value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			static_cast<pi_camera_local*>(camera)->config.contrast = pi_camera_clamp_contrast(value);
			pi_camera_cli_build_params(static_cast<pi_camera_local*>(camera));
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_set_contrast(static_cast<pi_camera_remote*>(camera)->socket, value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_set_contrast(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_set_contrast(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}

AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_sharpness(pi_camera* camera, AL::int8* value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			*value = static_cast<pi_camera_local*>(camera)->config.sharpness;
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_get_sharpness(static_cast<pi_camera_remote*>(camera)->socket, *value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_get_sharpness(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_get_sharpness(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}
AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_sharpness(pi_camera* camera, AL::int8 value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			static_cast<pi_camera_local*>(camera)->config.sharpness = pi_camera_clamp_sharpness(value);
			pi_camera_cli_build_params(static_cast<pi_camera_local*>(camera));
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_set_sharpness(static_cast<pi_camera_remote*>(camera)->socket, value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_set_sharpness(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_set_sharpness(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}

AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_brightness(pi_camera* camera, AL::uint8* value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			*value = static_cast<pi_camera_local*>(camera)->config.brightness;
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_get_brightness(static_cast<pi_camera_remote*>(camera)->socket, *value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_get_brightness(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_get_brightness(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}
AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_brightness(pi_camera* camera, AL::uint8 value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			static_cast<pi_camera_local*>(camera)->config.brightness = pi_camera_clamp_brightness(value);
			pi_camera_cli_build_params(static_cast<pi_camera_local*>(camera));
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_set_brightness(static_cast<pi_camera_remote*>(camera)->socket, value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_set_brightness(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_set_brightness(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}

AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_saturation(pi_camera* camera, AL::int8* value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			*value = static_cast<pi_camera_local*>(camera)->config.saturation;
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_get_saturation(static_cast<pi_camera_remote*>(camera)->socket, *value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_get_saturation(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_get_saturation(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}
AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_saturation(pi_camera* camera, AL::int8 value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			static_cast<pi_camera_local*>(camera)->config.saturation = pi_camera_clamp_saturation(value);
			pi_camera_cli_build_params(static_cast<pi_camera_local*>(camera));
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_set_saturation(static_cast<pi_camera_remote*>(camera)->socket, value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_set_saturation(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_set_saturation(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}

AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_white_balance(pi_camera* camera, AL::uint8* value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			*value = static_cast<pi_camera_local*>(camera)->config.white_balance;
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_get_white_balance(static_cast<pi_camera_remote*>(camera)->socket, *value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_get_white_balance(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_get_white_balance(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}
AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_white_balance(pi_camera* camera, AL::uint8 value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			static_cast<pi_camera_local*>(camera)->config.white_balance = value;
			pi_camera_cli_build_params(static_cast<pi_camera_local*>(camera));
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_set_white_balance(static_cast<pi_camera_remote*>(camera)->socket, value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_set_white_balance(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_set_white_balance(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}

AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_shutter_speed(pi_camera* camera, AL::TimeSpan* value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			*value = static_cast<pi_camera_local*>(camera)->config.shutter_speed;
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_get_shutter_speed(static_cast<pi_camera_remote*>(camera)->socket, *value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_get_shutter_speed(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_get_shutter_speed(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}
AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_shutter_speed(pi_camera* camera, AL::TimeSpan value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			static_cast<pi_camera_local*>(camera)->config.shutter_speed = pi_camera_clamp_shutter_speed(value);
			pi_camera_cli_build_params(static_cast<pi_camera_local*>(camera));
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_set_shutter_speed(static_cast<pi_camera_remote*>(camera)->socket, value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_set_shutter_speed(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_set_shutter_speed(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}

AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_exposure_mode(pi_camera* camera, AL::uint8* value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			*value = static_cast<pi_camera_local*>(camera)->config.exposure_mode;
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_get_exposure_mode(static_cast<pi_camera_remote*>(camera)->socket, *value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_get_exposure_mode(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_get_exposure_mode(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}
AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_exposure_mode(pi_camera* camera, AL::uint8 value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			static_cast<pi_camera_local*>(camera)->config.exposure_mode = value;
			pi_camera_cli_build_params(static_cast<pi_camera_local*>(camera));
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_set_exposure_mode(static_cast<pi_camera_remote*>(camera)->socket, value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_set_exposure_mode(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_set_exposure_mode(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}

AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_metoring_mode(pi_camera* camera, AL::uint8* value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			*value = static_cast<pi_camera_local*>(camera)->config.metoring_mode;
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_get_metoring_mode(static_cast<pi_camera_remote*>(camera)->socket, *value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_get_metoring_mode(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_get_metoring_mode(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}
AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_metoring_mode(pi_camera* camera, AL::uint8 value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			static_cast<pi_camera_local*>(camera)->config.metoring_mode = value;
			pi_camera_cli_build_params(static_cast<pi_camera_local*>(camera));
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_set_metoring_mode(static_cast<pi_camera_remote*>(camera)->socket, value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_set_metoring_mode(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_set_metoring_mode(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}

AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_jpg_quality(pi_camera* camera, AL::uint8* value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			*value = static_cast<pi_camera_local*>(camera)->config.jpg_quality;
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_get_jpg_quality(static_cast<pi_camera_remote*>(camera)->socket, *value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_get_jpg_quality(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_get_jpg_quality(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}
AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_jpg_quality(pi_camera* camera, AL::uint8 value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			static_cast<pi_camera_local*>(camera)->config.jpg_quality = pi_camera_clamp_jpg_quality(value);
			pi_camera_cli_build_params(static_cast<pi_camera_local*>(camera));
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_set_jpg_quality(static_cast<pi_camera_remote*>(camera)->socket, value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_set_jpg_quality(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_set_jpg_quality(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}

AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_image_size(pi_camera* camera, AL::uint16* width, AL::uint16* height)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			*width  = static_cast<pi_camera_local*>(camera)->config.image_size_width;
			*height = static_cast<pi_camera_local*>(camera)->config.image_size_height;
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_get_image_size(static_cast<pi_camera_remote*>(camera)->socket, *width, *height);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_get_image_size(&static_cast<pi_camera_service*>(camera)->local, width, height);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_get_image_size(&static_cast<pi_camera_session*>(camera)->service->local, width, height);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}
AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_image_size(pi_camera* camera, AL::uint16 width, AL::uint16 height)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			static_cast<pi_camera_local*>(camera)->config.image_size_width  = pi_camera_clamp_image_size_width(width);
			static_cast<pi_camera_local*>(camera)->config.image_size_height = pi_camera_clamp_image_size_height(height);
			pi_camera_cli_build_params(static_cast<pi_camera_local*>(camera));
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_set_image_size(static_cast<pi_camera_remote*>(camera)->socket, width, height);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_set_image_size(&static_cast<pi_camera_service*>(camera)->local, width, height);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_set_image_size(&static_cast<pi_camera_session*>(camera)->service->local, width, height);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}

AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_image_effect(pi_camera* camera, AL::uint8* value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			*value = static_cast<pi_camera_local*>(camera)->config.image_effect;
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_get_image_effect(static_cast<pi_camera_remote*>(camera)->socket, *value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_get_image_effect(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_get_image_effect(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}
AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_image_effect(pi_camera* camera, AL::uint8 value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			static_cast<pi_camera_local*>(camera)->config.image_effect = value;
			pi_camera_cli_build_params(static_cast<pi_camera_local*>(camera));
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_set_image_effect(static_cast<pi_camera_remote*>(camera)->socket, value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_set_image_effect(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_set_image_effect(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}

AL::uint8  PI_CAMERA_API_CALL  pi_camera_get_image_rotation(pi_camera* camera, AL::uint16* value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			*value = static_cast<pi_camera_local*>(camera)->config.image_rotation;
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_get_image_rotation(static_cast<pi_camera_remote*>(camera)->socket, *value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_get_image_rotation(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_get_image_rotation(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}
AL::uint8  PI_CAMERA_API_CALL  pi_camera_set_image_rotation(pi_camera* camera, AL::uint16 value)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			static_cast<pi_camera_local*>(camera)->config.image_rotation = pi_camera_clamp_image_rotation(value);
			pi_camera_cli_build_params(static_cast<pi_camera_local*>(camera));
			return PI_CAMERA_ERROR_CODE_SUCCESS;

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_set_image_rotation(static_cast<pi_camera_remote*>(camera)->socket, value);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_set_image_rotation(&static_cast<pi_camera_service*>(camera)->local, value);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_set_image_rotation(&static_cast<pi_camera_session*>(camera)->service->local, value);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}

AL::uint8  PI_CAMERA_API_CALL  pi_camera_capture(pi_camera* camera, const char* file_path)
{
	switch (camera->type)
	{
		case PI_CAMERA_TYPE_LOCAL:
			return pi_camera_cli_execute(static_cast<pi_camera_local*>(camera), file_path);

		case PI_CAMERA_TYPE_REMOTE:
			return pi_camera_net_begin_capture(static_cast<pi_camera_remote*>(camera)->socket, file_path);

		case PI_CAMERA_TYPE_SERVICE:
			return pi_camera_capture(&static_cast<pi_camera_service*>(camera)->local, file_path);

		case PI_CAMERA_TYPE_SESSION:
			return pi_camera_capture(&static_cast<pi_camera_session*>(camera)->service->local, file_path);
	}

	return PI_CAMERA_ERROR_CODE_UNDEFINED;
}
