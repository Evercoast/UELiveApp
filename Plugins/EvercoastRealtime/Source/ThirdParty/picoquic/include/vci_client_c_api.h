// INTEL CONFIDENTIAL
//
// Copyright(C) 2022 Intel Corporation
//
// This software and the related documents are Intel copyrighted materials, and your use of them is governed
// by the express license under which they were provided to you("License").Unless the License provides
// otherwise, you may not use, modify, copy, publish, distribute, disclose or transmit this software or the
// related documents without Intel's prior written permission.
//
// This software and the related documents are provided as is, with no express or implied warranties, other
// than those that are expressly stated in the License.

#ifndef CLIENT_C_API_H_
#define CLIENT_C_API_H_
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef VCI_EXPORT_DECL
#if defined(_WIN32) || defined(__WIN32__)
#define VCI_EXPORT_DECL __declspec(dllimport)
#else
#define VCI_EXPORT_DECL
#endif
#endif

    typedef enum
    {
        vci_connection_type_sender = 0,
        vci_connection_type_receiver = 1
    } vci_connection_type_t;

    typedef enum
    {
        vci_status_not_yet_connected = 0,
        vci_status_connected,
        vci_status_failed_to_connect,
        vci_status_disconnected,
        vci_status_protocol_error,
        vci_status_handle_is_invalid,
        vci_status_failed_to_authenticate
    } vci_status_t;

#ifndef VCI_LIBRARY
    typedef void* vci_connection_handle_t;
#endif

    /**
     * @brief (Obsolete; no authentication) Creates connection to a server
     *
     * @param server Server address
     * @param port Server port to use
     * @param connection_type Whether this is a pure frame sender or frame receiver client. (Frame receiver can still send frames)
     * @return Connection handle
     */
    VCI_EXPORT_DECL vci_connection_handle_t vci_create_connection(const char* server, int port, vci_connection_type_t connection_type);

    /**
     * @brief Creates connection to a server
     *
     * @param server Server address. Must not be null.
     * @param port Server port to use
     * @param connection_type Whether this is a pure frame sender or frame receiver client. (Frame receiver can still send frames)
     * @param username User name
     * @param access_token Access token (preferably not an unchanging password but a token or a time-limited hashed salted password)
     * @param ca_crt_path The path to a custom CRT file for CA root verification of server TLS .
     * If username or password or server_name_indicator are set, ca_crt_path must also be set to a valid CRT file.
     * @param server_name_indicator If not null or empty, server name indicator to use for certificate verification (ca_crt_path must also be supplied). If null or empty, server address will be used instead.
     * @return Connection handle
     */
    VCI_EXPORT_DECL vci_connection_handle_t vci_create_connection_2(const char* server, int port,
        vci_connection_type_t connection_type, char* username, char* access_token,
        const char* ca_crt_path, const char* server_name_indicator);

    /**
     * @brief If disconnected, reconnect with original credentials
     *
     * @param handle Connection handle
     * @return true if reconnect process started
     */
    VCI_EXPORT_DECL int vci_reconnect(vci_connection_handle_t handle);

    /**
     * @brief Get the status of the connection (see vci_status_t enum)
     *
     * @param handle Connection handle
     * @return VCI_EXPORT_DECL
     */
    VCI_EXPORT_DECL int vci_get_status(vci_connection_handle_t handle);

    // Receiving frames
    /**
     * @brief Has the frame been received?
     *
     * @param handle Connection handle
     * @return VCI_EXPORT_DECL
     */
    VCI_EXPORT_DECL int vci_received_frame(vci_connection_handle_t handle);
    /**
     * @brief Get size of handle's data
     *
     * @param handle Connection handle
     * @return Size of data of the received frame. Invalidated by vci_pop_frame or vci_delete_connection.
     */
    VCI_EXPORT_DECL uint64_t vci_get_data_size(vci_connection_handle_t handle);

    /**
     * @brief Get size of handle's user data
     *
     * @param handle Connection handle
     * @return Size of user data of the received frame. Invalidated by vci_pop_frame or vci_delete_connection.
     */
    VCI_EXPORT_DECL uint64_t vci_get_user_data_size(vci_connection_handle_t handle);

    /**
     * @brief Get frame number of the received frame
     *
     * @param handle
     * @return Frame number. Invalidated by vci_pop_frame or vci_delete_connection.
     */
    VCI_EXPORT_DECL uint64_t vci_get_frame_number(vci_connection_handle_t handle);
    /**
     * @brief Get type and flags information for the received frame
     *
     * @param handle
     * @return Frame type and flags. Invalidated by vci_pop_frame or vci_delete_connection.
     */
    VCI_EXPORT_DECL uint64_t vci_get_type_and_flags(vci_connection_handle_t handle);
    /**
     * @brief Timestamp in microseconds
     *
     * @param handle Connection handle
     * @return Timestamp. Invalidated by vci_pop_frame or vci_delete_connection.
     */
    VCI_EXPORT_DECL uint64_t vci_get_timestamp(vci_connection_handle_t handle);

    /**
     * @brief Get pointer to frame's data
     *
     * @param handle Connection handle
     * @return Pointer to the data. The pointer is invalidated by vci_pop_frame or vci_delete_connection, do not keep using the returned pointer after calling vci_pop_frame.
     */
    VCI_EXPORT_DECL uint8_t* vci_get_data(vci_connection_handle_t handle);

    /**
     * @brief Get pointer to frame's user data
     *
     * @param handle Connection handle
     * @return Pointer to the user data. The pointer is invalidated by vci_pop_frame or vci_delete_connection, do not keep using the returned pointer after calling vci_pop_frame.
     */
    VCI_EXPORT_DECL uint8_t* vci_get_user_data(vci_connection_handle_t handle);

    /**
     * @brief Pop frame from the queue. Deallocates pointer returned by vci_get_data
     *
     * @param handle Connection handle
     */
    VCI_EXPORT_DECL void vci_pop_frame(vci_connection_handle_t handle);

    /**
     * @brief Sends a frame to the server
     *
     * @param handle Connection handle
     * @param frame_type Type of the frame (0 video, 1 audio, additional values can be used)
     * @param frame_number Frame sequence number (does not have to be contiguous)
     * @param timestamp Timestamp in microseconds
     * @param frame_data Frame data
     * @param frame_data_size Size of frame data
     * @param user_data Additional user data that you want to send alongside the frame (or null)
     * @param user_data_size Size of user data
     * @return 1 on success, 0 on failure (send queue full)
     */
    VCI_EXPORT_DECL int vci_send_frame(vci_connection_handle_t handle,
        int frame_type, uint64_t frame_number, uint64_t timestamp,
        const uint8_t* frame_data, uint64_t frame_data_size,
        const uint8_t* user_data, uint64_t user_data_size);

    /**
     * @brief Sends framerate to the server
     *
     * @param handle Connection handle
     * @param fps framerate in frames per second (15,30, etc)
     */
    VCI_EXPORT_DECL void vci_update_framerate(vci_connection_handle_t handle, double fps);

    /**
     * @brief Sets up writing of KPI data to a file (if KPI data is present)
     *
     * @param handle Connection handle
     * @param kpi_filename
     */
    VCI_EXPORT_DECL void vci_set_kpi_output(vci_connection_handle_t handle, const char* kpi_filename);

    /**
     * @brief Deletes connection to the server
     *
     * @param handle Connection handle
     */
    VCI_EXPORT_DECL void vci_delete_connection(vci_connection_handle_t handle);

#ifdef __cplusplus
}
#endif
#endif
