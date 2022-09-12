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

/*! \file l2_interface.c
 * \brief layer 2 interface, used to support different RRC sublayer
 * \author Raymond Knopp and Navid Nikaein
 * \date 2010-2014
 * \version 1.0
 * \company Eurecom
 * \email: raymond.knopp@eurecom.fr
 */

#include "platform_types.h"
#include "rrc_defs.h"
#include "rrc_extern.h"
#include "common/utils/LOG/log.h"
#include "rrc_eNB_UE_context.h"
#include "pdcp.h"
#include "common/ran_context.h"

#include "intertask_interface.h"

#include "openair3/SS/ss_eNB_context.h"
//#define RRC_DATA_REQ_DEBUG
//#define DEBUG_RRC 1

extern RAN_CONTEXT_t RC;
//------------------------------------------------------------------------------
uint8_t
rrc_data_req(
  const protocol_ctxt_t   *const ctxt_pP,
  const rb_id_t                  rb_idP,
  const mui_t                    muiP,
  const confirm_t                confirmP,
  const sdu_size_t               sdu_sizeP,
  uint8_t                 *const buffer_pP,
  const pdcp_transmission_mode_t modeP
)
//------------------------------------------------------------------------------
{
  if(sdu_sizeP == 255) {
    LOG_I(RRC,"sdu_sizeP == 255");
    return false;
  }

  MessageDef *message_p;
  // Uses a new buffer to avoid issue with PDCP buffer content that could be changed by PDCP (asynchronous message handling).
  uint8_t *message_buffer;
  message_buffer = itti_malloc (
                     ctxt_pP->enb_flag ? TASK_RRC_ENB : TASK_RRC_UE,
                     ctxt_pP->enb_flag ? TASK_PDCP_ENB : TASK_PDCP_UE,
                     sdu_sizeP);
  memcpy (message_buffer, buffer_pP, sdu_sizeP);
  message_p = itti_alloc_new_message (ctxt_pP->enb_flag ? TASK_RRC_ENB : TASK_RRC_UE, 0, RRC_DCCH_DATA_REQ);
  RRC_DCCH_DATA_REQ (message_p).frame     = ctxt_pP->frame;
  RRC_DCCH_DATA_REQ (message_p).enb_flag  = ctxt_pP->enb_flag;
  RRC_DCCH_DATA_REQ (message_p).rb_id     = rb_idP;
  RRC_DCCH_DATA_REQ (message_p).muip      = muiP;
  RRC_DCCH_DATA_REQ (message_p).confirmp  = confirmP;
  RRC_DCCH_DATA_REQ (message_p).sdu_size  = sdu_sizeP;
  RRC_DCCH_DATA_REQ (message_p).sdu_p     = message_buffer;
  //memcpy (RRC_DCCH_DATA_REQ (message_p).sdu_p, buffer_pP, sdu_sizeP);
  RRC_DCCH_DATA_REQ (message_p).mode      = modeP;
  RRC_DCCH_DATA_REQ (message_p).module_id = ctxt_pP->module_id;
  RRC_DCCH_DATA_REQ(message_p).rnti = ctxt_pP->rntiMaybeUEid;
  RRC_DCCH_DATA_REQ (message_p).eNB_index = ctxt_pP->eNB_index;
  itti_send_msg_to_task (
    ctxt_pP->enb_flag ? TASK_PDCP_ENB : TASK_PDCP_UE,
    ctxt_pP->instance,
    message_p);
  LOG_I(RRC,"sent RRC_DCCH_DATA_REQ to TASK_PDCP_ENB\n");

  return true; // TODO should be changed to a CNF message later, currently RRC lite does not used the returned value anyway.
}

//------------------------------------------------------------------------------
void
rrc_data_ind(
  const protocol_ctxt_t *const ctxt_pP,
  const rb_id_t                Srb_id,
  const sdu_size_t             sdu_sizeP,
  const uint8_t   *const       buffer_pP
)
//------------------------------------------------------------------------------
{
  rb_id_t    DCCH_index = Srb_id;

  if (ctxt_pP->enb_flag == ENB_FLAG_NO) {
    LOG_I(RRC, "[UE %x] Frame %d: received a DCCH %ld message on SRB %ld with Size %d from eNB %d\n",
          ctxt_pP->module_id, ctxt_pP->frame, DCCH_index,Srb_id,sdu_sizeP,  ctxt_pP->eNB_index);
  } else {
    LOG_D(RRC, "[eNB %d] Frame %d: received a DCCH %ld message on SRB %ld with Size %d from UE %lx\n", ctxt_pP->module_id, ctxt_pP->frame, DCCH_index, Srb_id, sdu_sizeP, ctxt_pP->rntiMaybeUEid);

//#ifdef ENB_SS
    if (RC.ss.mode >= SS_SOFTMODEM && SS_context.SSCell_list[0].State >= SS_STATE_CELL_ACTIVE)
    {
      LOG_I(RRC,"L2 Interface Sending DCCH PDU_IND /SS_RRC_PDU_IND (msg_Id:%d )to TASK_SS_SRB\n", SS_RRC_PDU_IND);
      MessageDef *message_p = itti_alloc_new_message (TASK_RRC_ENB, 0, SS_RRC_PDU_IND);
      if (message_p) {
        /* Populate the message and send to SS */
        //Find the CC_id from module_id and rnti
        int UE_id = find_UE_id(ctxt_pP->module_id,ctxt_pP->rntiMaybeUEid);
        int CC_id = UE_id>=0? UE_PCCID(ctxt_pP->module_id,UE_id):0;
        SS_RRC_PDU_IND (message_p).physCellId = RC.rrc[ctxt_pP->module_id]->carrier[CC_id].physCellId;
        SS_RRC_PDU_IND (message_p).sdu_size = sdu_sizeP;
        SS_RRC_PDU_IND (message_p).srb_id = DCCH_index;
        SS_RRC_PDU_IND (message_p).frame = ctxt_pP->frame;
        SS_RRC_PDU_IND (message_p).rnti = ctxt_pP->rntiMaybeUEid;
        SS_RRC_PDU_IND (message_p).subframe = ctxt_pP->subframe;
        memset (SS_RRC_PDU_IND (message_p).sdu, 0, CCCH_SDU_SIZE);
        memcpy (SS_RRC_PDU_IND (message_p).sdu, buffer_pP, sdu_sizeP);

        int send_res = itti_send_msg_to_task (TASK_SS_SRB, 0, message_p);
        if(send_res < 0) {
          LOG_E(RRC,"Error in sending PDU_IND /SS_RRC_PDU_IND (msg_Id:%d )to TASK_SS_SRB\n", SS_RRC_PDU_IND);
        }
      }
    }
//#endif /** ENB_SS */

  }

  {
    MessageDef *message_p;
    // Uses a new buffer to avoid issue with PDCP buffer content that could be changed by PDCP (asynchronous message handling).
    uint8_t *message_buffer;
    message_buffer = itti_malloc (ctxt_pP->enb_flag ? TASK_PDCP_ENB : TASK_PDCP_UE, ctxt_pP->enb_flag ? TASK_RRC_ENB : TASK_RRC_UE, sdu_sizeP);
    memcpy (message_buffer, buffer_pP, sdu_sizeP);
    message_p = itti_alloc_new_message (ctxt_pP->enb_flag ? TASK_PDCP_ENB : TASK_PDCP_UE, 0, RRC_DCCH_DATA_IND);
    RRC_DCCH_DATA_IND (message_p).frame      = ctxt_pP->frame;
    RRC_DCCH_DATA_IND (message_p).dcch_index = DCCH_index;
    RRC_DCCH_DATA_IND (message_p).sdu_size   = sdu_sizeP;
    RRC_DCCH_DATA_IND (message_p).sdu_p      = message_buffer;
    RRC_DCCH_DATA_IND(message_p).rnti = ctxt_pP->rntiMaybeUEid;
    RRC_DCCH_DATA_IND (message_p).module_id  = ctxt_pP->module_id;
    RRC_DCCH_DATA_IND (message_p).eNB_index  = ctxt_pP->eNB_index;
    itti_send_msg_to_task (ctxt_pP->enb_flag ? TASK_RRC_ENB : TASK_RRC_UE, ctxt_pP->instance, message_p);
  }
}
