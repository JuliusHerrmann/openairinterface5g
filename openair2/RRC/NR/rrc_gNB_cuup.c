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

#include "common/ran_context.h"
#include "nr_rrc_defs.h"

int rrc_gNB_process_e1_setup_req(e1ap_setup_req_t *req)
{
  AssertFatal(req->supported_plmns <= PLMN_LIST_MAX_SIZE, "Supported PLMNs is more than PLMN_LIST_MAX_SIZE\n");
  gNB_RRC_INST *rrc = RC.nrrrc[0];
  AssertFatal(rrc->cuup == NULL, "cannot handle multiple CU-UPs\n");


  for (int i = 0; i < req->supported_plmns; i++) {
    PLMN_ID_t *id = &req->plmns[i];
    if (rrc->configuration.mcc[i] != id->mcc || rrc->configuration.mnc[i] != id->mnc) {
      LOG_E(NR_RRC,
            "PLMNs received from CUUP (mcc:%d, mnc:%d) did not match with PLMNs in RRC (mcc:%d, mnc:%d)\n",
            id->mcc,
            id->mnc,
            rrc->configuration.mcc[i],
            rrc->configuration.mnc[i]);
      return -1;
    }
  }

  LOG_I(RRC, "Accepting new CU-UP ID %ld name %s\n", req->gNB_cu_up_id, req->gNB_cu_up_name);
  rrc->cuup = malloc(sizeof(*rrc->cuup));
  AssertFatal(rrc->cuup, "out of memory\n");
  rrc->cuup->setup_req = malloc(sizeof(*rrc->cuup->setup_req));
  *rrc->cuup->setup_req = *req;

  MessageDef *msg_p = itti_alloc_new_message(TASK_RRC_GNB, 0, E1AP_SETUP_RESP);
  e1ap_setup_resp_t *resp = &E1AP_SETUP_RESP(msg_p);
  resp->transac_id = req->transac_id;
  itti_send_msg_to_task(TASK_CUCP_E1, 0, msg_p);

  return 0;
}