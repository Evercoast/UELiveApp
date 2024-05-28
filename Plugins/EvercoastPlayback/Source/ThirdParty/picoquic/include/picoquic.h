#pragma once

#if (PLATFORM_ANDROID || PLATFORM_LINUX || PLATFORM_MAC || PLATFORM_IOS)
// For link the .so
#ifdef __cplusplus
extern "C" {
#endif

	typedef enum {
		vci_connection_type_sender = 0,
		vci_connection_type_receiver = 1
	} vci_connection_type_t;


	typedef void* vci_connection_handle_t;
	vci_connection_handle_t vci_create_connection(const char* server_name, int port, vci_connection_type_t connection_type);
	vci_connection_handle_t vci_create_connection_2(const char* server, int port,
		vci_connection_type_t connection_type, char* username, char* access_token,
		const char* ca_crt_path, const char* server_name_indicator);
	int vci_reconnect(vci_connection_handle_t handle);
	void vci_delete_connection(vci_connection_handle_t handle);
	int vci_get_status(vci_connection_handle_t handle);
	int vci_received_frame(vci_connection_handle_t handle);
	uint64_t vci_get_data_size(vci_connection_handle_t hanlde);
	uint64_t vci_get_user_data_size(vci_connection_handle_t hanlde);
	uint64_t vci_get_frame_number(vci_connection_handle_t hanlde);
	uint64_t vci_get_timestamp(vci_connection_handle_t hanlde);
	uint64_t vci_get_type_and_flags(vci_connection_handle_t hanlde);
	uint8_t* vci_get_data(vci_connection_handle_t handle);
	uint8_t* vci_get_user_data(vci_connection_handle_t handle);
	void vci_pop_frame(vci_connection_handle_t handle);
	void vci_update_framerate(vci_connection_handle_t handle, double fps);
	void vci_set_kpi_output(vci_connection_handle_t handle, const char* output_filename);

#ifdef __cplusplus
}
#endif 
#endif

namespace PicoQuic
{
	enum class vci_connection_type_t {
		vci_connection_type_sender = 0,
		vci_connection_type_receiver = 1
	};

	enum class Status
	{
		NotYetConnected,
		Connected,
		FailedToConnect,
		Disconnected,
		ProtocolError,
		HandleIsInvalid,
		FailedToAuthenticate
	};

	typedef void* vci_connection_handle_t;

#if PLATFORM_WINDOWS
	typedef vci_connection_handle_t	(__cdecl *vci_create_connection)(	const char* server_name, int port, vci_connection_type_t connection_type);
	typedef vci_connection_handle_t (__cdecl *vci_create_connection_2)(	const char* server, int port,
																		vci_connection_type_t connection_type, char* username, char* access_token,
																		const char* ca_crt_path, const char* server_name_indicator);

	typedef int			(__cdecl *vci_reconnect)(vci_connection_handle_t handle);
	typedef void		(__cdecl *vci_delete_connection)(vci_connection_handle_t handle);
	typedef int 		(__cdecl *vci_get_status)(vci_connection_handle_t handle);
	typedef int 		(__cdecl *vci_received_frame)(vci_connection_handle_t handle);
	typedef uint64_t	(__cdecl *vci_get_data_size)(vci_connection_handle_t hanlde);
	typedef uint64_t	(__cdecl *vci_get_user_data_size)(vci_connection_handle_t hanlde);
	typedef uint64_t	(__cdecl *vci_get_frame_number)(vci_connection_handle_t hanlde);
	typedef uint64_t	(__cdecl *vci_get_timestamp)(vci_connection_handle_t hanlde);
	typedef uint64_t	(__cdecl *vci_get_type_and_flags)(vci_connection_handle_t hanlde);
	typedef uint8_t*	(__cdecl *vci_get_data)(vci_connection_handle_t handle);
	typedef uint8_t*	(__cdecl *vci_get_user_data)(vci_connection_handle_t handle);
	typedef void		(__cdecl *vci_pop_frame)(vci_connection_handle_t handle);
	typedef void		(__cdecl *vci_update_framerate)(vci_connection_handle_t handle, double fps);
	typedef void		(__cdecl* vci_set_kpi_output)(vci_connection_handle_t handle, const char* output_filename);

	vci_create_connection 		create_connection = nullptr;
	vci_create_connection_2		create_connection_2 = nullptr;
	vci_reconnect				reconnect = nullptr;
	vci_delete_connection		delete_connection = nullptr;
	vci_get_status				get_status = nullptr;
	vci_received_frame			received_frame = nullptr;
	vci_pop_frame				pop_frame = nullptr;
	vci_get_data				get_data = nullptr;
	vci_get_user_data 			get_user_data = nullptr;
	vci_get_data_size			get_data_size = nullptr;
	vci_get_user_data_size		get_user_data_size = nullptr;
	vci_get_frame_number		get_frame_number = nullptr;
	vci_get_timestamp			get_timestamp = nullptr;
	vci_get_type_and_flags		get_type_and_flags = nullptr;
	vci_set_kpi_output			set_kpi_output = nullptr;
	vci_update_framerate		update_framerate = nullptr;

#elif (PLATFORM_ANDROID || PLATFORM_LINUX || PLATFORM_MAC || PLATFORM_IOS)

	inline vci_connection_handle_t create_connection(const char* server_name, int port, vci_connection_type_t connection_type)
	{
		return (vci_connection_handle_t)vci_create_connection(server_name, port, (::vci_connection_type_t)connection_type);
	}

	inline vci_connection_handle_t create_connection_2(const char* server_name, int port, vci_connection_type_t connection_type, 
		char* username, char* access_token, const char* ca_crt_path, const char* server_name_indicator)
	{
		return (vci_connection_handle_t)vci_create_connection_2(server_name, port, (::vci_connection_type_t)connection_type, username, access_token, ca_crt_path, server_name_indicator);
	}

	inline int reconnect(vci_connection_handle_t handle)
	{
		return vci_reconnect((::vci_connection_handle_t)handle);
	}

	inline void delete_connection(vci_connection_handle_t handle)
	{
		vci_delete_connection((::vci_connection_handle_t)handle);
	}

	inline int get_status(vci_connection_handle_t handle)
	{
		return vci_get_status((::vci_connection_handle_t)handle);
	}

	inline int received_frame(vci_connection_handle_t handle)
	{
		return vci_received_frame((::vci_connection_handle_t)handle);
	}

	inline uint64_t get_data_size(vci_connection_handle_t handle)
	{
		return vci_get_data_size((::vci_connection_handle_t)handle);
	}

	inline uint64_t get_user_data_size(vci_connection_handle_t handle)
	{
		return vci_get_user_data_size((::vci_connection_handle_t)handle);
	}

	inline uint64_t get_frame_number(vci_connection_handle_t handle)
	{
		return vci_get_frame_number((::vci_connection_handle_t)handle);
	}

	inline uint64_t get_timestamp(vci_connection_handle_t handle)
	{
		return vci_get_timestamp((::vci_connection_handle_t)handle);
	}

	inline uint64_t get_type_and_flags(vci_connection_handle_t handle)
	{
		return vci_get_type_and_flags((::vci_connection_handle_t)handle);
	}

	inline uint8_t* get_data(vci_connection_handle_t handle)
	{
		return vci_get_data((::vci_connection_handle_t)handle);
	}

	inline uint8_t* get_user_data(vci_connection_handle_t handle)
	{
		return vci_get_user_data((::vci_connection_handle_t)handle);
	}

	inline void pop_frame(vci_connection_handle_t handle)
	{
		vci_pop_frame((::vci_connection_handle_t)handle);
	}

	inline void set_kpi_output(vci_connection_handle_t handle, const char* output_filename)
	{
		vci_set_kpi_output((::vci_connection_handle_t)handle, output_filename);
	}

	inline void update_framerate(vci_connection_handle_t handle, double fps)
	{
		vci_update_framerate((::vci_connection_handle_t)handle, fps);
	}
#else
#error PicoQuic unsupported platform!
#endif
}
