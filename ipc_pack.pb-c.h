/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: ipc_pack.proto */

#ifndef PROTOBUF_C_ipc_5fpack_2eproto__INCLUDED
#define PROTOBUF_C_ipc_5fpack_2eproto__INCLUDED

#include <protobuf-c/protobuf-c.h>

PROTOBUF_C__BEGIN_DECLS

#if PROTOBUF_C_VERSION_NUMBER < 1000000
# error This file was generated by a newer version of protoc-c which is incompatible with your libprotobuf-c headers. Please update your headers.
#elif 1000002 < PROTOBUF_C_MIN_COMPILER_VERSION
# error This file was generated by an older version of protoc-c which is incompatible with your libprotobuf-c headers. Please regenerate this file with a newer version of protoc-c.
#endif


typedef struct _RtpConfig RtpConfig;
typedef struct _RtpgenIPCmsgV1 RtpgenIPCmsgV1;


/* --- enums --- */

typedef enum _RtpgenIPCmsgV1__Request {
  RTPGEN_IPCMSG_V1__REQUEST__CREATE = 0,
  RTPGEN_IPCMSG_V1__REQUEST__READ = 1,
  RTPGEN_IPCMSG_V1__REQUEST__UPDATE = 2,
  RTPGEN_IPCMSG_V1__REQUEST__DELETE = 3
    PROTOBUF_C__FORCE_ENUM_TO_BE_INT_SIZE(RTPGEN_IPCMSG_V1__REQUEST)
} RtpgenIPCmsgV1__Request;
typedef enum _RtpgenIPCmsgV1__Response {
  RTPGEN_IPCMSG_V1__RESPONSE__SUCCESS = 200,
  RTPGEN_IPCMSG_V1__RESPONSE__ERROR_BAD_REQUEST = 400,
  RTPGEN_IPCMSG_V1__RESPONSE__ERROR_FORBIDDEN = 403,
  RTPGEN_IPCMSG_V1__RESPONSE__ERROR_NOT_FOUND = 404,
  RTPGEN_IPCMSG_V1__RESPONSE__ERROR_CONFLICT = 409,
  RTPGEN_IPCMSG_V1__RESPONSE__ERROR_SERVER_ERROR = 500,
  RTPGEN_IPCMSG_V1__RESPONSE__ERROR_SERVICE_UNAVAILABLE = 503
    PROTOBUF_C__FORCE_ENUM_TO_BE_INT_SIZE(RTPGEN_IPCMSG_V1__RESPONSE)
} RtpgenIPCmsgV1__Response;

/* --- messages --- */

struct  _RtpConfig
{
  ProtobufCMessage base;
  protobuf_c_boolean has_enabled;
  protobuf_c_boolean enabled;
  protobuf_c_boolean has_ip_dst_addr;
  uint32_t ip_dst_addr;
  protobuf_c_boolean has_ip_src_addr;
  uint32_t ip_src_addr;
  protobuf_c_boolean has_udp_dst_port;
  uint32_t udp_dst_port;
  protobuf_c_boolean has_udp_src_port;
  uint32_t udp_src_port;
  protobuf_c_boolean has_rtp_timestamp;
  uint32_t rtp_timestamp;
  protobuf_c_boolean has_rtp_sequence;
  uint32_t rtp_sequence;
  protobuf_c_boolean has_rtp_ssrc;
  uint32_t rtp_ssrc;
};
#define RTP_CONFIG__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&rtp_config__descriptor) \
    , 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0 }


struct  _RtpgenIPCmsgV1
{
  ProtobufCMessage base;
  protobuf_c_boolean has_request_code;
  RtpgenIPCmsgV1__Request request_code;
  protobuf_c_boolean has_response_code;
  RtpgenIPCmsgV1__Response response_code;
  protobuf_c_boolean has_portid;
  uint32_t portid;
  protobuf_c_boolean has_id_selector;
  int32_t id_selector;
  protobuf_c_boolean has_size;
  int32_t size;
  RtpConfig *rtp_config;
};
#define RTPGEN_IPCMSG_V1__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&rtpgen_ipcmsg_v1__descriptor) \
    , 0,0, 0,0, 0,0, 0,0, 0,0, NULL }


/* RtpConfig methods */
void   rtp_config__init
                     (RtpConfig         *message);
size_t rtp_config__get_packed_size
                     (const RtpConfig   *message);
size_t rtp_config__pack
                     (const RtpConfig   *message,
                      uint8_t             *out);
size_t rtp_config__pack_to_buffer
                     (const RtpConfig   *message,
                      ProtobufCBuffer     *buffer);
RtpConfig *
       rtp_config__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   rtp_config__free_unpacked
                     (RtpConfig *message,
                      ProtobufCAllocator *allocator);
/* RtpgenIPCmsgV1 methods */
void   rtpgen_ipcmsg_v1__init
                     (RtpgenIPCmsgV1         *message);
size_t rtpgen_ipcmsg_v1__get_packed_size
                     (const RtpgenIPCmsgV1   *message);
size_t rtpgen_ipcmsg_v1__pack
                     (const RtpgenIPCmsgV1   *message,
                      uint8_t             *out);
size_t rtpgen_ipcmsg_v1__pack_to_buffer
                     (const RtpgenIPCmsgV1   *message,
                      ProtobufCBuffer     *buffer);
RtpgenIPCmsgV1 *
       rtpgen_ipcmsg_v1__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   rtpgen_ipcmsg_v1__free_unpacked
                     (RtpgenIPCmsgV1 *message,
                      ProtobufCAllocator *allocator);
/* --- per-message closures --- */

typedef void (*RtpConfig_Closure)
                 (const RtpConfig *message,
                  void *closure_data);
typedef void (*RtpgenIPCmsgV1_Closure)
                 (const RtpgenIPCmsgV1 *message,
                  void *closure_data);

/* --- services --- */


/* --- descriptors --- */

extern const ProtobufCMessageDescriptor rtp_config__descriptor;
extern const ProtobufCMessageDescriptor rtpgen_ipcmsg_v1__descriptor;
extern const ProtobufCEnumDescriptor    rtpgen_ipcmsg_v1__request__descriptor;
extern const ProtobufCEnumDescriptor    rtpgen_ipcmsg_v1__response__descriptor;

PROTOBUF_C__END_DECLS


#endif  /* PROTOBUF_C_ipc_5fpack_2eproto__INCLUDED */
