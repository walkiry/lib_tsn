// Copyright (c) 2014-2017, XMOS Ltd, All rights reserved
#include <xccompat.h>
#include <print.h>
#include "debug_print.h"
#include "avb.h"
#include "avb_conf.h"
#include "avb_1722_common.h"
#include "avb_1722_maap.h"
#include "avb_1722_maap_protocol.h"
#if AVB_ENABLE_1722_1
#include "avb_1722_1_common.h"
#include "avb_1722_1_acmp.h"
#include "avb_1722_1_adp.h"
#include "avb_1722_1_app_hooks.h"
#endif

#if AVB_ENABLE_1722_1

#define XMOS_VENDOR_ID 0x00229700

void avb_entity_on_new_entity_available(client interface avb_interface avb, const_guid_ref_t my_guid, avb_1722_1_entity_record *entity, client interface ethernet_tx_if i_eth)
{
  // If Talker is enabled, connect to the first XMOS listener we see
  if (AVB_1722_1_TALKER_ENABLED && AVB_1722_1_CONTROLLER_ENABLED)
  {
    if ((entity->vendor_id == XMOS_VENDOR_ID) &&
       ((entity->listener_capabilities & AVB_1722_1_ADP_LISTENER_CAPABILITIES_AUDIO_SINK) == AVB_1722_1_ADP_LISTENER_CAPABILITIES_AUDIO_SINK) &&
       (entity->listener_stream_sinks >= 1))
    {
      // Ensure that the listener knows our GUID
      avb_1722_1_adp_announce();

      avb_1722_1_controller_connect(my_guid, entity->guid, 0, 0, i_eth);
    }
  }
}

/* The controller has indicated that a listener is connecting to this talker stream */
void avb_talker_on_listener_connect(client interface avb_interface avb, int source_num, const_guid_ref_t listener_guid)
{
  avb_talker_on_listener_connect_default(avb, source_num, listener_guid);
}

void avb_talker_on_listener_connect_failed(client interface avb_interface avb, const_guid_ref_t my_guid, int source_num,
        const_guid_ref_t listener_guid, avb_1722_1_acmp_status_t status, client interface ethernet_tx_if i_eth)
{
  avb_talker_on_listener_connect_failed_default(avb, my_guid, source_num, listener_guid, status, i_eth);
}

/* The controller has indicated to connect this listener sink to a talker stream */
avb_1722_1_acmp_status_t avb_listener_on_talker_connect(client interface avb_interface avb,
                                                        int sink_num,
                                                        const_guid_ref_t talker_guid,
                                                        unsigned char dest_addr[6],
                                                        unsigned int stream_id[2],
                                                        unsigned short vlan_id,
                                                        const_guid_ref_t my_guid)
{
  return avb_listener_on_talker_connect_default(avb, sink_num, talker_guid, dest_addr, stream_id, vlan_id, my_guid);
}

/* The controller has indicated to disconnect this listener sink from a talker stream */
void avb_listener_on_talker_disconnect(client interface avb_interface avb,
                                       int sink_num,
                                       const_guid_ref_t talker_guid,
                                       unsigned char dest_addr[6],
                                       unsigned int stream_id[2],
                                       const_guid_ref_t my_guid)
{
  avb_listener_on_talker_disconnect_default(avb, sink_num, talker_guid, dest_addr, stream_id, my_guid);
}

/* The controller has indicated that a listener is disconnecting from this talker stream */
void avb_talker_on_listener_disconnect(client interface avb_interface avb,
                                       int source_num,
                                       const_guid_ref_t listener_guid,
                                       int connection_count)
{
  avb_talker_on_listener_disconnect_default(avb, source_num, listener_guid, connection_count);
}

void avb_talker_on_source_address_reserved(client interface avb_interface avb, int source_num, unsigned char mac_addr[6])
{
  avb_talker_on_source_address_reserved_default(avb, source_num, mac_addr);
}
#endif
