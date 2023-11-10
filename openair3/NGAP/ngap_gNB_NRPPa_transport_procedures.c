/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*! \file ngap_gNB_NRPPa_transport_procedures.c
 * \brief NGAP gNb  procedure handler
 * \author  Adeel Malik
 * \date 2023
 * \email: adeel.malik@eurecom.fr
 * \version 1.0
 * @ingroup _ngap
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "assertions.h"
#include "conversions.h"

#include "intertask_interface.h"

#include "ngap_common.h"
#include "ngap_gNB_defs.h"
#include "ngap_gNB_itti_messaging.h"
#include "ngap_gNB_encoder.h"
#include "ngap_gNB_nnsf.h"
#include "ngap_gNB_ue_context.h"
#include "ngap_gNB_management_procedures.h"

#include "ngap_gNB_NRPPa_transport_procedures.h"




//UPLINK UE ASSOCIATED NRPPA TRANSPORT (9.2.9.2 of TS 38.413 Version 16.0.0.0 Release 16)
int ngap_gNB_UplinkUEAssociatedNRPPaTransport(instance_t instance, ngap_UplinkUEAssociatedNRPPa_t *ngap_UplinkUEAssociatedNRPPa_p){

  LOG_I(NGAP, "Initiating ngap_gNB_UplinkUEAssociatedNRPPaTransport \n");
  struct ngap_gNB_ue_context_s   *ue_context_p;
  ngap_gNB_instance_t            *ngap_gNB_instance_p;
  NGAP_NGAP_PDU_t pdu;
  uint8_t  *buffer;
  uint32_t  length;
  DevAssert(ngap_UplinkUEAssociatedNRPPa_p != NULL);
  /* Retrieve the NGAP gNB instance associated with Mod_id */
  ngap_gNB_instance_p = ngap_gNB_get_instance(instance);
  DevAssert(ngap_gNB_instance_p != NULL);

  if ((ue_context_p = ngap_get_ue_context(ngap_UplinkUEAssociatedNRPPa_p->gNB_ue_ngap_id)) == NULL) {
    /* The context for this gNB ue ngap id doesn't exist in the map of gNB UEs */
    NGAP_WARN("Failed to find ue context associated with gNB ue ngap id: %08x\n",
              ngap_UplinkUEAssociatedNRPPa_p->gNB_ue_ngap_id);
    return -1;
  }

  /* Prepare the NGAP message to encode */
  //IE: 9.3.1.1 Message Type UplinkUEAssociatedNRPPaTransport
  memset(&pdu, 0, sizeof(pdu));
  pdu.present = NGAP_NGAP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu.choice.initiatingMessage, head);
  head->procedureCode = NGAP_ProcedureCode_id_UplinkUEAssociatedNRPPaTransport;
  head->criticality = NGAP_Criticality_ignore;
  head->value.present = NGAP_InitiatingMessage__value_PR_UplinkUEAssociatedNRPPaTransport;

  NGAP_UplinkUEAssociatedNRPPaTransport_t *out = &head->value.choice.UplinkUEAssociatedNRPPaTransport;

  //IE: 9.3.3.1 AMF UE NGAP ID /* mandatory */
  {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_UplinkUEAssociatedNRPPaTransportIEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_AMF_UE_NGAP_ID;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_UplinkUEAssociatedNRPPaTransportIEs__value_PR_AMF_UE_NGAP_ID;
    asn_uint642INTEGER(&ie->value.choice.AMF_UE_NGAP_ID, ue_context_p->amf_ue_ngap_id);
  }

  //IE: 9.3.3.2 RAN UE NGAP ID  /* mandatory */
  {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_UplinkUEAssociatedNRPPaTransportIEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_RAN_UE_NGAP_ID;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_UplinkUEAssociatedNRPPaTransportIEs__value_PR_RAN_UE_NGAP_ID;
    ie->value.choice.RAN_UE_NGAP_ID = ue_context_p->gNB_ue_ngap_id;
  }

  //IE: 9.3.3.14 NRPPa-PDU   /* mandatory */
  {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_UplinkUEAssociatedNRPPaTransportIEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_NRPPa_PDU;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_UplinkUEAssociatedNRPPaTransportIEs__value_PR_NRPPa_PDU;
    ie->value.choice.NRPPa_PDU.buf = ngap_UplinkUEAssociatedNRPPa_p->nrppa_pdu.buffer;
    ie->value.choice.NRPPa_PDU.size = ngap_UplinkUEAssociatedNRPPa_p->nrppa_pdu.length;
  }

  //IE: 9.3.3.13 Routing ID  /* mandatory */
  /* TODO get Routing ID from downlink*/
  // store this routing ID in
  {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_UplinkUEAssociatedNRPPaTransportIEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_RoutingID;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_UplinkUEAssociatedNRPPaTransportIEs__value_PR_RoutingID;
    ie->value.choice.RoutingID.buf = ngap_UplinkUEAssociatedNRPPa_p->routing_id.buffer;
    ie->value.choice.RoutingID.size = ngap_UplinkUEAssociatedNRPPa_p->routing_id.length;
  }


  /* Encode NGAP message */
  if (ngap_gNB_encode_pdu(&pdu, &buffer, &length) < 0) {
    NGAP_ERROR("Failed to encode Uplink UE Associated NRPPa Transport\n");
    /* Encode procedure has failed... */
    return -1;
  }

 LOG_I(NGAP,"Sending sctp_data_req for ngap_gNB_UplinkUEAssociatedNRPPaTransport \n");
  /* UE associated signalling -> use the allocated stream */
  ngap_gNB_itti_send_sctp_data_req(ngap_gNB_instance_p->instance,
                                   ue_context_p->amf_ref->assoc_id, buffer,
                                   length, ue_context_p->tx_stream);

return 0;
}


//UPLINK NON UE ASSOCIATED NRPPA TRANSPORT (9.2.9.4 of TS 38.413 Version 16.0.0.0 Release 16)
int ngap_gNB_UplinkNonUEAssociatedNRPPaTransport(instance_t instance, ngap_UplinkNonUEAssociatedNRPPa_t *ngap_UplinkNonUEAssociatedNRPPa_p){

  ngap_gNB_instance_t           *ngap_gNB_instance_p;
  ngap_gNB_amf_data_t           *amf_desc_p;
  NGAP_NGAP_PDU_t pdu;
  uint8_t  *buffer;
  uint32_t  length;
  DevAssert(ngap_UplinkNonUEAssociatedNRPPa_p != NULL);
  /* Retrieve the NGAP gNB instance associated with Mod_id */
  ngap_gNB_instance_p = ngap_gNB_get_instance(instance);
  DevAssert(ngap_gNB_instance_p != NULL);

  /* Retrieve the NGAP gNB  amf data */
  amf_desc_p= ngap_gNB_get_AMF_from_instance(instance);
  DevAssert(amf_desc_p != NULL);


  /* Prepare the NGAP message to encode */

  //IE: 9.3.1.1 Message Type UplinkNonUEAssociatedNRPPaTransport
  memset(&pdu, 0, sizeof(pdu));
  pdu.present = NGAP_NGAP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu.choice.initiatingMessage, head);
  head->procedureCode = NGAP_ProcedureCode_id_UplinkNonUEAssociatedNRPPaTransport;
  head->criticality = NGAP_Criticality_ignore;
  head->value.present = NGAP_InitiatingMessage__value_PR_UplinkNonUEAssociatedNRPPaTransport;

  NGAP_UplinkNonUEAssociatedNRPPaTransport_t *out = &head->value.choice.UplinkNonUEAssociatedNRPPaTransport;

   //IE: 9.3.3.13 Routing ID /* mandatory */
  {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_UplinkNonUEAssociatedNRPPaTransportIEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_RoutingID;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_UplinkNonUEAssociatedNRPPaTransportIEs__value_PR_RoutingID;
    ie->value.choice.RoutingID.buf  = ngap_UplinkNonUEAssociatedNRPPa_p->routing_id.buffer;
    ie->value.choice.RoutingID.size = ngap_UplinkNonUEAssociatedNRPPa_p->routing_id.length;
  }

  //IE: 9.3.3.14 NRPPa-PDU /* mandatory */
  {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_UplinkNonUEAssociatedNRPPaTransportIEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_NRPPa_PDU;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_UplinkNonUEAssociatedNRPPaTransportIEs__value_PR_NRPPa_PDU;
    ie->value.choice.NRPPa_PDU.buf = ngap_UplinkNonUEAssociatedNRPPa_p->nrppa_pdu.buffer;
    ie->value.choice.NRPPa_PDU.size = ngap_UplinkNonUEAssociatedNRPPa_p->nrppa_pdu.length;
  }

  /* Encode NGAP message */
  if (ngap_gNB_encode_pdu(&pdu, &buffer, &length) < 0) {
    NGAP_ERROR("Failed to encode Uplink Non UE Associated NRPPa Transport\n");
    /* Encode procedure has failed... */
    return -1;
  }


/* Non UE-Associated signalling -> stream = 0 */
ngap_gNB_itti_send_sctp_data_req(ngap_gNB_instance_p->instance, amf_desc_p->assoc_id, buffer, length, 0);
return 0;
}



// handel DOWNLINK UE ASSOCIATED NRPPA TRANSPORT (9.2.9.1 of TS 38.413 Version 16.0.0.0 Release 16)
int ngap_gNB_handle_DownlinkUEAssociatedNRPPaTransport(uint32_t assoc_id, uint32_t stream, NGAP_NGAP_PDU_t *pdu){

  ngap_gNB_amf_data_t             *amf_desc_p        = NULL;
  ngap_gNB_ue_context_t           *ue_desc_p         = NULL;
  ngap_gNB_instance_t             *ngap_gNB_instance = NULL;
  NGAP_DownlinkUEAssociatedNRPPaTransport_t     *container;
  NGAP_DownlinkUEAssociatedNRPPaTransportIEs_t *ie;
  NGAP_RAN_UE_NGAP_ID_t            gnb_ue_ngap_id;
  uint64_t                         amf_ue_ngap_id;
  uint8_t *routingId_buffer= NULL;
  uint32_t routingId_buffer_length= 0;
  uint8_t *nrppa_pdu_buffer= NULL;
  uint32_t nrppa_pdu_length= 0;


  DevAssert(pdu != NULL);

  if ((amf_desc_p = ngap_gNB_get_AMF(NULL, assoc_id, 0)) == NULL) {
    NGAP_ERROR("[SCTP %d] Received NRPPa downlink message for non existing AMF context\n", assoc_id);
    return -1;
  }

  ngap_gNB_instance = amf_desc_p->ngap_gNB_instance;

/* Prepare the NGAP message for NRPPA */

//IE: 9.3.1.1 Message Type
  container = &pdu->choice.initiatingMessage->value.choice.DownlinkUEAssociatedNRPPaTransport;

//IE: 9.3.3.1 AMF UE NGAP ID
  NGAP_FIND_PROTOCOLIE_BY_ID(NGAP_DownlinkUEAssociatedNRPPaTransportIEs_t, ie, container,
                             NGAP_ProtocolIE_ID_id_AMF_UE_NGAP_ID, true);
  asn_INTEGER2ulong(&(ie->value.choice.AMF_UE_NGAP_ID), &amf_ue_ngap_id);

//IE: 9.3.3.2 RAN UE NGAP ID
  NGAP_FIND_PROTOCOLIE_BY_ID(NGAP_DownlinkUEAssociatedNRPPaTransportIEs_t, ie, container,
                             NGAP_ProtocolIE_ID_id_RAN_UE_NGAP_ID, true);
  gnb_ue_ngap_id = ie->value.choice.RAN_UE_NGAP_ID;

  if ((ue_desc_p = ngap_get_ue_context(gnb_ue_ngap_id)) == NULL) {
    NGAP_ERROR("[SCTP %d] Received NRPPa downlink message for non existing UE context gNB_UE_NGAP_ID: 0x%lx\n",
               assoc_id,
               gnb_ue_ngap_id);
    return -1;
  }
//printf("Test 1 [NGAP]Adeel: NGAP handel_DownlinkUEAssociatedNRPPa gNB_UE_NGAP_ID= %d\n", gnb_ue_ngap_id);
//printf("Test 1 [NGAP]Adeel: NGAP handel_DownlinkUEAssociatedNRPPa gNB_UE_NGAP_ID: 0x%lx\n", gnb_ue_ngap_id);
/* todo ad**l
  if (0 == ue_desc_p->rx_stream) {
    ue_desc_p->rx_stream = stream;
  } else if (stream != ue_desc_p->rx_stream) {
    NGAP_ERROR("[SCTP %d] Received UE-related procedure on stream %u, expecting %u\n",
               assoc_id, stream, ue_desc_p->rx_stream);
    return -1;
  } */

  /* Is it the first outcome of the AMF for this UE ? If so store the amf
   * UE ngap id.
   */
  if (ue_desc_p->amf_ue_ngap_id == 0) {
    ue_desc_p->amf_ue_ngap_id = amf_ue_ngap_id;
  } else {
    /* We already have a amf ue ngap id check the received is the same */
    if (ue_desc_p->amf_ue_ngap_id != amf_ue_ngap_id) {
      NGAP_ERROR("[SCTP %d] Mismatch in AMF UE NGAP ID (0x%lx != 0x%" PRIx64 "\n", assoc_id, amf_ue_ngap_id, (uint64_t)ue_desc_p->amf_ue_ngap_id);
      return -1;
    }
  }

//IE: 9.3.3.13 Routing ID
  NGAP_FIND_PROTOCOLIE_BY_ID(NGAP_DownlinkUEAssociatedNRPPaTransportIEs_t, ie, container,
                             NGAP_ProtocolIE_ID_id_RoutingID, true);
  routingId_buffer=  ie->value.choice.RoutingID.buf;
  routingId_buffer_length= ie->value.choice.RoutingID.size;
/*printf("Test 1 Adeel: NGAP itti send_DownlinkUEAssociatedNRPPa Routing pdu buffer size =%d and buffer is \n ", ie->value.choice.RoutingID.size);
uint8_t *rId_buffer= ie->value.choice.RoutingID.buf ;
printf("Routing ID buffer startind addr %p  and value \n", rId_buffer);
for (int i = 0; i < ie->value.choice.RoutingID.size; i++){
printf("%02x ", *rId_buffer++);
//printf("%d ", *rId_buffer++);
//printf("%p ", rId_buffer++);
}*/


//IE: 9.3.3.14 NRPPa-PDU
  NGAP_FIND_PROTOCOLIE_BY_ID(NGAP_DownlinkUEAssociatedNRPPaTransportIEs_t, ie, container,
                             NGAP_ProtocolIE_ID_id_NRPPa_PDU, true);
  nrppa_pdu_buffer = ie->value.choice.NRPPa_PDU.buf;
  nrppa_pdu_length= ie->value.choice.NRPPa_PDU.size;

// ie->value.choice.NRPPa_PDU.buf, ie->value.choice.NRPPa_PDU.size);
//printf("Test 1 Adeel: NGAP itti send_DownlinkUEAssociatedNRPPa Nrppa pdu buffer size =%d and buffer is \n ", ie->value.choice.NRPPa_PDU.size);
/*uint8_t *nrp_buffer= ie->value.choice.NRPPa_PDU.buf ;
printf("Nrppa pdu buffer startind addr %p and value \n", nrp_buffer);
for (int i = 0; i < ie->value.choice.NRPPa_PDU.size; i++){
printf("%02x ", *nrp_buffer++);
//printf("%d ", *nrp_buffer++);
//printf("%p ", nrp_buffer++);
}*/

/*printf("Test 1 Adeel: NGAP itti send_DownlinkUEAssociatedNRPPa Routing pdu buffer size =%d and buffer is \n ", routingId_buffer_length);
printf("[NGAP] Routing ID buffer startind addr %p  and value \n", routingId_buffer);
for (int i = 0; i < routingId_buffer_length; i++){
printf("%02x ", *routingId_buffer++);
//printf("%d ", *rId_buffer++);
//printf("%p ", rId_buffer++);
}*/

/*printf("Test 1 Adeel: NGAP itti send_DownlinkUEAssociatedNRPPa Nrppa pdu buffer size =%d and buffer is \n ", ie->value.choice.NRPPa_PDU.size);
printf("[NGAP] Nrppa pdu buffer startind addr %p and value \n", nrppa_pdu_buffer);
for (int i = 0; i < nrppa_pdu_length; i++){
printf("%02x ", *nrppa_pdu_buffer++);
//printf("%d ", *nrppa_pdu_buffer++);
//printf("%p ", nrppa_pdu_buffer++);
}*/

/* Forward the NRPPA PDU to NRPPA */
//ngap_gNB_itti_send_DownlinkUEAssociatedNRPPa(ngap_gNB_instance->instance, gnb_ue_ngap_id, amf_ue_ngap_id, ie->value.choice.RoutingID.buf, ie->value.choice.RoutingID.size,ie->value.choice.NRPPa_PDU.buf, ie->value.choice.NRPPa_PDU.size); //ad**l todo
ngap_gNB_itti_send_DownlinkUEAssociatedNRPPa(ngap_gNB_instance->instance, gnb_ue_ngap_id, amf_ue_ngap_id, routingId_buffer, routingId_buffer_length,nrppa_pdu_buffer, nrppa_pdu_length); //ad**l todo

return 0;
}


//DOWNLINK NON UE ASSOCIATED NRPPA TRANSPORT (9.2.9.3 of TS 38.413 Version 16.0.0.0 Release 16)
int ngap_gNB_handle_DownlinkNonUEAssociatedNRPPaTransport(uint32_t assoc_id, uint32_t stream, NGAP_NGAP_PDU_t *pdu){
  ngap_gNB_amf_data_t             *amf_desc_p        = NULL;
  ngap_gNB_instance_t             *ngap_gNB_instance = NULL;
  NGAP_DownlinkNonUEAssociatedNRPPaTransport_t     *container;
  NGAP_DownlinkNonUEAssociatedNRPPaTransportIEs_t *ie;
  uint8_t *routingId_buffer= NULL;
  uint32_t routingId_buffer_length= 0;
  uint8_t *nrppa_pdu_buffer= NULL;
  uint32_t nrppa_pdu_length= 0;

  DevAssert(pdu != NULL);

  if ((amf_desc_p = ngap_gNB_get_AMF(NULL, assoc_id, 0)) == NULL) {
    NGAP_ERROR("[SCTP %d] Received NRPPa downlink message for non existing AMF context\n", assoc_id);
    return -1;
  }

  ngap_gNB_instance = amf_desc_p->ngap_gNB_instance;

  /* Prepare the NGAP message to forward to NRPPA */

//IE: 9.3.1.1 Message Type
  container = &pdu->choice.initiatingMessage->value.choice.DownlinkNonUEAssociatedNRPPaTransport;

//IE: 9.3.3.13 Routing ID
  NGAP_FIND_PROTOCOLIE_BY_ID(NGAP_DownlinkNonUEAssociatedNRPPaTransportIEs_t, ie, container,
                             NGAP_ProtocolIE_ID_id_RoutingID, true);
  routingId_buffer=  ie->value.choice.RoutingID.buf;
  routingId_buffer_length= ie->value.choice.RoutingID.size;

//IE: 9.3.3.14 NRPPa-PDU
  NGAP_FIND_PROTOCOLIE_BY_ID(NGAP_DownlinkNonUEAssociatedNRPPaTransportIEs_t, ie, container,
                             NGAP_ProtocolIE_ID_id_NRPPa_PDU, true);
  nrppa_pdu_buffer = ie->value.choice.NRPPa_PDU.buf;
  nrppa_pdu_length= ie->value.choice.NRPPa_PDU.size;

/* Forward the NRPPA PDU to NRPPA */
//ngap_gNB_itti_send_DownlinkNonUEAssociatedNRPPa(ngap_gNB_instance->instance, ie->value.choice.RoutingID.buf, ie->value.choice.RoutingID.size, ie->value.choice.NRPPa_PDU.buf, ie->value.choice.NRPPa_PDU.size); //ad**l todo
ngap_gNB_itti_send_DownlinkNonUEAssociatedNRPPa(ngap_gNB_instance->instance, routingId_buffer, routingId_buffer_length, nrppa_pdu_buffer, nrppa_pdu_length); //ad**l todo

return 0;
}



