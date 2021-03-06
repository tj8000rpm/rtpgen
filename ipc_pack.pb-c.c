/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: ipc_pack.proto */

/* Do not generate deprecated warnings for self */
#ifndef PROTOBUF_C__NO_DEPRECATED
#define PROTOBUF_C__NO_DEPRECATED
#endif

#include "ipc_pack.pb-c.h"
void   rtp_config_v1__init
                     (RtpConfigV1         *message)
{
  static RtpConfigV1 init_value = RTP_CONFIG_V1__INIT;
  *message = init_value;
}
size_t rtp_config_v1__get_packed_size
                     (const RtpConfigV1 *message)
{
  assert(message->base.descriptor == &rtp_config_v1__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t rtp_config_v1__pack
                     (const RtpConfigV1 *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &rtp_config_v1__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t rtp_config_v1__pack_to_buffer
                     (const RtpConfigV1 *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &rtp_config_v1__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
RtpConfigV1 *
       rtp_config_v1__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (RtpConfigV1 *)
     protobuf_c_message_unpack (&rtp_config_v1__descriptor,
                                allocator, len, data);
}
void   rtp_config_v1__free_unpacked
                     (RtpConfigV1 *message,
                      ProtobufCAllocator *allocator)
{
  assert(message->base.descriptor == &rtp_config_v1__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   rtpgen_ipcmsg_v1__init
                     (RtpgenIPCmsgV1         *message)
{
  static RtpgenIPCmsgV1 init_value = RTPGEN_IPCMSG_V1__INIT;
  *message = init_value;
}
size_t rtpgen_ipcmsg_v1__get_packed_size
                     (const RtpgenIPCmsgV1 *message)
{
  assert(message->base.descriptor == &rtpgen_ipcmsg_v1__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t rtpgen_ipcmsg_v1__pack
                     (const RtpgenIPCmsgV1 *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &rtpgen_ipcmsg_v1__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t rtpgen_ipcmsg_v1__pack_to_buffer
                     (const RtpgenIPCmsgV1 *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &rtpgen_ipcmsg_v1__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
RtpgenIPCmsgV1 *
       rtpgen_ipcmsg_v1__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (RtpgenIPCmsgV1 *)
     protobuf_c_message_unpack (&rtpgen_ipcmsg_v1__descriptor,
                                allocator, len, data);
}
void   rtpgen_ipcmsg_v1__free_unpacked
                     (RtpgenIPCmsgV1 *message,
                      ProtobufCAllocator *allocator)
{
  assert(message->base.descriptor == &rtpgen_ipcmsg_v1__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
static const ProtobufCFieldDescriptor rtp_config_v1__field_descriptors[8] =
{
  {
    "enabled",
    1,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_BOOL,
    offsetof(RtpConfigV1, has_enabled),
    offsetof(RtpConfigV1, enabled),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "ip_dst_addr",
    3,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_UINT32,
    offsetof(RtpConfigV1, has_ip_dst_addr),
    offsetof(RtpConfigV1, ip_dst_addr),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "ip_src_addr",
    4,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_UINT32,
    offsetof(RtpConfigV1, has_ip_src_addr),
    offsetof(RtpConfigV1, ip_src_addr),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "udp_dst_port",
    5,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_UINT32,
    offsetof(RtpConfigV1, has_udp_dst_port),
    offsetof(RtpConfigV1, udp_dst_port),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "udp_src_port",
    6,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_UINT32,
    offsetof(RtpConfigV1, has_udp_src_port),
    offsetof(RtpConfigV1, udp_src_port),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "rtp_timestamp",
    7,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_UINT32,
    offsetof(RtpConfigV1, has_rtp_timestamp),
    offsetof(RtpConfigV1, rtp_timestamp),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "rtp_sequence",
    8,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_UINT32,
    offsetof(RtpConfigV1, has_rtp_sequence),
    offsetof(RtpConfigV1, rtp_sequence),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "rtp_ssrc",
    9,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_UINT32,
    offsetof(RtpConfigV1, has_rtp_ssrc),
    offsetof(RtpConfigV1, rtp_ssrc),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned rtp_config_v1__field_indices_by_name[] = {
  0,   /* field[0] = enabled */
  1,   /* field[1] = ip_dst_addr */
  2,   /* field[2] = ip_src_addr */
  6,   /* field[6] = rtp_sequence */
  7,   /* field[7] = rtp_ssrc */
  5,   /* field[5] = rtp_timestamp */
  3,   /* field[3] = udp_dst_port */
  4,   /* field[4] = udp_src_port */
};
static const ProtobufCIntRange rtp_config_v1__number_ranges[2 + 1] =
{
  { 1, 0 },
  { 3, 1 },
  { 0, 8 }
};
const ProtobufCMessageDescriptor rtp_config_v1__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "RtpConfigV1",
  "RtpConfigV1",
  "RtpConfigV1",
  "",
  sizeof(RtpConfigV1),
  8,
  rtp_config_v1__field_descriptors,
  rtp_config_v1__field_indices_by_name,
  2,  rtp_config_v1__number_ranges,
  (ProtobufCMessageInit) rtp_config_v1__init,
  NULL,NULL,NULL    /* reserved[123] */
};
const ProtobufCEnumValue rtpgen_ipcmsg_v1__request__enum_values_by_number[4] =
{
  { "CREATE", "RTPGEN_IPCMSG_V1__REQUEST__CREATE", 0 },
  { "READ", "RTPGEN_IPCMSG_V1__REQUEST__READ", 1 },
  { "UPDATE", "RTPGEN_IPCMSG_V1__REQUEST__UPDATE", 2 },
  { "DELETE", "RTPGEN_IPCMSG_V1__REQUEST__DELETE", 3 },
};
static const ProtobufCIntRange rtpgen_ipcmsg_v1__request__value_ranges[] = {
{0, 0},{0, 4}
};
const ProtobufCEnumValueIndex rtpgen_ipcmsg_v1__request__enum_values_by_name[4] =
{
  { "CREATE", 0 },
  { "DELETE", 3 },
  { "READ", 1 },
  { "UPDATE", 2 },
};
const ProtobufCEnumDescriptor rtpgen_ipcmsg_v1__request__descriptor =
{
  PROTOBUF_C__ENUM_DESCRIPTOR_MAGIC,
  "RtpgenIPCmsgV1.Request",
  "Request",
  "RtpgenIPCmsgV1__Request",
  "",
  4,
  rtpgen_ipcmsg_v1__request__enum_values_by_number,
  4,
  rtpgen_ipcmsg_v1__request__enum_values_by_name,
  1,
  rtpgen_ipcmsg_v1__request__value_ranges,
  NULL,NULL,NULL,NULL   /* reserved[1234] */
};
const ProtobufCEnumValue rtpgen_ipcmsg_v1__response__enum_values_by_number[7] =
{
  { "SUCCESS", "RTPGEN_IPCMSG_V1__RESPONSE__SUCCESS", 200 },
  { "ERROR_BAD_REQUEST", "RTPGEN_IPCMSG_V1__RESPONSE__ERROR_BAD_REQUEST", 400 },
  { "ERROR_FORBIDDEN", "RTPGEN_IPCMSG_V1__RESPONSE__ERROR_FORBIDDEN", 403 },
  { "ERROR_NOT_FOUND", "RTPGEN_IPCMSG_V1__RESPONSE__ERROR_NOT_FOUND", 404 },
  { "ERROR_CONFLICT", "RTPGEN_IPCMSG_V1__RESPONSE__ERROR_CONFLICT", 409 },
  { "ERROR_SERVER_ERROR", "RTPGEN_IPCMSG_V1__RESPONSE__ERROR_SERVER_ERROR", 500 },
  { "ERROR_SERVICE_UNAVAILABLE", "RTPGEN_IPCMSG_V1__RESPONSE__ERROR_SERVICE_UNAVAILABLE", 503 },
};
static const ProtobufCIntRange rtpgen_ipcmsg_v1__response__value_ranges[] = {
{200, 0},{400, 1},{403, 2},{409, 4},{500, 5},{503, 6},{0, 7}
};
const ProtobufCEnumValueIndex rtpgen_ipcmsg_v1__response__enum_values_by_name[7] =
{
  { "ERROR_BAD_REQUEST", 1 },
  { "ERROR_CONFLICT", 4 },
  { "ERROR_FORBIDDEN", 2 },
  { "ERROR_NOT_FOUND", 3 },
  { "ERROR_SERVER_ERROR", 5 },
  { "ERROR_SERVICE_UNAVAILABLE", 6 },
  { "SUCCESS", 0 },
};
const ProtobufCEnumDescriptor rtpgen_ipcmsg_v1__response__descriptor =
{
  PROTOBUF_C__ENUM_DESCRIPTOR_MAGIC,
  "RtpgenIPCmsgV1.Response",
  "Response",
  "RtpgenIPCmsgV1__Response",
  "",
  7,
  rtpgen_ipcmsg_v1__response__enum_values_by_number,
  7,
  rtpgen_ipcmsg_v1__response__enum_values_by_name,
  6,
  rtpgen_ipcmsg_v1__response__value_ranges,
  NULL,NULL,NULL,NULL   /* reserved[1234] */
};
static const ProtobufCFieldDescriptor rtpgen_ipcmsg_v1__field_descriptors[6] =
{
  {
    "request_code",
    1,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_ENUM,
    offsetof(RtpgenIPCmsgV1, has_request_code),
    offsetof(RtpgenIPCmsgV1, request_code),
    &rtpgen_ipcmsg_v1__request__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "response_code",
    2,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_ENUM,
    offsetof(RtpgenIPCmsgV1, has_response_code),
    offsetof(RtpgenIPCmsgV1, response_code),
    &rtpgen_ipcmsg_v1__response__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "portid",
    3,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_UINT32,
    offsetof(RtpgenIPCmsgV1, has_portid),
    offsetof(RtpgenIPCmsgV1, portid),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "id_selector",
    4,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_INT32,
    offsetof(RtpgenIPCmsgV1, has_id_selector),
    offsetof(RtpgenIPCmsgV1, id_selector),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "size",
    5,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_INT32,
    offsetof(RtpgenIPCmsgV1, has_size),
    offsetof(RtpgenIPCmsgV1, size),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "rtp_config",
    6,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    offsetof(RtpgenIPCmsgV1, rtp_config),
    &rtp_config_v1__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned rtpgen_ipcmsg_v1__field_indices_by_name[] = {
  3,   /* field[3] = id_selector */
  2,   /* field[2] = portid */
  0,   /* field[0] = request_code */
  1,   /* field[1] = response_code */
  5,   /* field[5] = rtp_config */
  4,   /* field[4] = size */
};
static const ProtobufCIntRange rtpgen_ipcmsg_v1__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 6 }
};
const ProtobufCMessageDescriptor rtpgen_ipcmsg_v1__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "RtpgenIPCmsgV1",
  "RtpgenIPCmsgV1",
  "RtpgenIPCmsgV1",
  "",
  sizeof(RtpgenIPCmsgV1),
  6,
  rtpgen_ipcmsg_v1__field_descriptors,
  rtpgen_ipcmsg_v1__field_indices_by_name,
  1,  rtpgen_ipcmsg_v1__number_ranges,
  (ProtobufCMessageInit) rtpgen_ipcmsg_v1__init,
  NULL,NULL,NULL    /* reserved[123] */
};
