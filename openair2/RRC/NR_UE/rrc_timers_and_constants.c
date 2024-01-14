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

#include "openair2/RRC/NR_UE/rrc_proto.h"

void init_SI_timers(NR_UE_RRC_SI_INFO *SInfo)
{
  // delete any stored version of a SIB after 3 hours
  // from the moment it was successfully confirmed as valid
  SInfo->sib1_timer.active = false;
  nr_timer_setup(&SInfo->sib1_timer, 10800000, 10);
  SInfo->sib2_timer.active = false;
  nr_timer_setup(&SInfo->sib2_timer, 10800000, 10);
  SInfo->sib3_timer.active = false;
  nr_timer_setup(&SInfo->sib3_timer, 10800000, 10);
  SInfo->sib4_timer.active = false;
  nr_timer_setup(&SInfo->sib4_timer, 10800000, 10);
  SInfo->sib5_timer.active = false;
  nr_timer_setup(&SInfo->sib5_timer, 10800000, 10);
  SInfo->sib6_timer.active = false;
  nr_timer_setup(&SInfo->sib6_timer, 10800000, 10);
  SInfo->sib7_timer.active = false;
  nr_timer_setup(&SInfo->sib7_timer, 10800000, 10);
  SInfo->sib8_timer.active = false;
  nr_timer_setup(&SInfo->sib8_timer, 10800000, 10);
  SInfo->sib9_timer.active = false;
  nr_timer_setup(&SInfo->sib9_timer, 10800000, 10);
  SInfo->sib10_timer.active = false;
  nr_timer_setup(&SInfo->sib10_timer, 10800000, 10);
  SInfo->sib11_timer.active = false;
  nr_timer_setup(&SInfo->sib11_timer, 10800000, 10);
  SInfo->sib12_timer.active = false;
  nr_timer_setup(&SInfo->sib12_timer, 10800000, 10);
  SInfo->sib13_timer.active = false;
  nr_timer_setup(&SInfo->sib13_timer, 10800000, 10);
  SInfo->sib14_timer.active = false;
  nr_timer_setup(&SInfo->sib14_timer, 10800000, 10);
}

void nr_rrc_SI_timers(NR_UE_RRC_SI_INFO *SInfo)
{
  if (SInfo->sib1 && SInfo->sib1_timer.active)
    nr_timer_tick(&SInfo->sib1_timer);
  if (SInfo->sib2 && SInfo->sib2_timer.active)
    nr_timer_tick(&SInfo->sib2_timer);
  if (SInfo->sib3 && SInfo->sib3_timer.active)
    nr_timer_tick(&SInfo->sib3_timer);
  if (SInfo->sib4 && SInfo->sib4_timer.active)
    nr_timer_tick(&SInfo->sib4_timer);
  if (SInfo->sib5 && SInfo->sib5_timer.active)
    nr_timer_tick(&SInfo->sib5_timer);
  if (SInfo->sib6 && SInfo->sib6_timer.active)
    nr_timer_tick(&SInfo->sib6_timer);
  if (SInfo->sib7 && SInfo->sib7_timer.active)
    nr_timer_tick(&SInfo->sib7_timer);
  if (SInfo->sib8 && SInfo->sib8_timer.active)
    nr_timer_tick(&SInfo->sib8_timer);
  if (SInfo->sib9 && SInfo->sib9_timer.active)
    nr_timer_tick(&SInfo->sib9_timer);
  if (SInfo->sib10 && SInfo->sib10_timer.active)
    nr_timer_tick(&SInfo->sib10_timer);
  if (SInfo->sib11 && SInfo->sib11_timer.active)
    nr_timer_tick(&SInfo->sib11_timer);
  if (SInfo->sib12 && SInfo->sib12_timer.active)
    nr_timer_tick(&SInfo->sib12_timer);
  if (SInfo->sib13 && SInfo->sib13_timer.active)
    nr_timer_tick(&SInfo->sib13_timer);
  if (SInfo->sib14 && SInfo->sib14_timer.active)
    nr_timer_tick(&SInfo->sib14_timer);
}

void nr_rrc_handle_timers(NR_UE_RRC_INST_t *rrc, instance_t instance)
{
  NR_UE_Timers_Constants_t *timers = &rrc->timers_and_constants;

  if (timers->T300.active) {
    nr_timer_tick(&timers->T300);
    if(nr_timer_expired(timers->T300)) {
      nr_timer_stop(&timers->T300);
      handle_t300_expiry(instance);
    }
  }
  // T304
  if (timers->T304.active) {
    nr_timer_tick(&timers->T304);
    if(nr_timer_expired(timers->T304)) {
      // TODO
      // For T304 of MCG, in case of the handover from NR or intra-NR
      // handover, initiate the RRC re-establishment procedure;
      // In case of handover to NR, perform the actions defined in the
      // specifications applicable for the source RAT.
    }
  }
  if (timers->T310.active) {
    nr_timer_tick(&timers->T310);
    if(nr_timer_expired(timers->T310)) {
      // TODO
      // handle detection of radio link failure
      // as described in 5.3.10.3 of 38.331
      AssertFatal(false, "Radio link failure! Not handled yet!\n");
    }
  }
  if (timers->T311.active) {
    nr_timer_tick(&timers->T311);
    if(nr_timer_expired(timers->T311)) {
      // Upon T311 expiry, the UE shall perform the actions upon going to RRC_IDLE
      // with release cause 'RRC connection failure'
      nr_rrc_going_to_IDLE(instance, RRC_CONNECTION_FAILURE, NULL);
    }
  }
}

void nr_rrc_set_T304(NR_timer_t *T304, NR_ReconfigurationWithSync_t *reconfigurationWithSync)
{
  if(reconfigurationWithSync) {
    int target = 0;
    switch (reconfigurationWithSync->t304) {
      case NR_ReconfigurationWithSync__t304_ms50 :
        target = 50;
        break;
      case NR_ReconfigurationWithSync__t304_ms100 :
        target = 100;
        break;
      case NR_ReconfigurationWithSync__t304_ms150 :
        target = 150;
        break;
      case NR_ReconfigurationWithSync__t304_ms200 :
        target = 200;
        break;
      case NR_ReconfigurationWithSync__t304_ms500 :
        target = 500;
        break;
      case NR_ReconfigurationWithSync__t304_ms1000 :
        target = 1000;
        break;
      case NR_ReconfigurationWithSync__t304_ms2000 :
        target = 2000;
        break;
      case NR_ReconfigurationWithSync__t304_ms10000 :
        target = 10000;
        break;
      default :
        AssertFatal(false, "Invalid T304 %ld\n", reconfigurationWithSync->t304);
    }
    nr_timer_setup(T304, target, 10); // 10ms step
  }
}

void set_rlf_sib1_timers_and_constants(NR_UE_Timers_Constants_t *tac, NR_SIB1_t *sib1)
{
  if(sib1 && sib1->ue_TimersAndConstants) {
    int k = 0;
    switch (sib1->ue_TimersAndConstants->t301) {
      case NR_UE_TimersAndConstants__t301_ms100 :
        k = 100;
        break;
      case NR_UE_TimersAndConstants__t301_ms200 :
        k = 200;
        break;
      case NR_UE_TimersAndConstants__t301_ms300 :
        k = 300;
        break;
      case NR_UE_TimersAndConstants__t301_ms400 :
        k = 400;
        break;
      case NR_UE_TimersAndConstants__t301_ms600 :
        k = 600;
        break;
      case NR_UE_TimersAndConstants__t301_ms1000 :
        k = 1000;
        break;
      case NR_UE_TimersAndConstants__t301_ms1500 :
        k = 1500;
        break;
      case NR_UE_TimersAndConstants__t301_ms2000 :
        k = 2000;
        break;
      default :
        AssertFatal(false, "Invalid T301 %ld\n", sib1->ue_TimersAndConstants->t301);
    }
    nr_timer_setup(&tac->T301, k, 10); // 10ms step
    switch (sib1->ue_TimersAndConstants->t310) {
      case NR_UE_TimersAndConstants__t310_ms0 :
        k = 0;
        break;
      case NR_UE_TimersAndConstants__t310_ms50 :
        k = 50;
        break;
      case NR_UE_TimersAndConstants__t310_ms100 :
        k = 100;
        break;
      case NR_UE_TimersAndConstants__t310_ms200 :
        k = 200;
        break;
      case NR_UE_TimersAndConstants__t310_ms500 :
        k = 500;
        break;
      case NR_UE_TimersAndConstants__t310_ms1000 :
        k = 1000;
        break;
      case NR_UE_TimersAndConstants__t310_ms2000 :
        k = 2000;
        break;
      default :
        AssertFatal(false, "Invalid T310 %ld\n", sib1->ue_TimersAndConstants->t310);
    }
    nr_timer_setup(&tac->T310, k, 10); // 10ms step
    switch (sib1->ue_TimersAndConstants->t311) {
      case NR_UE_TimersAndConstants__t311_ms1000 :
        k = 1000;
        break;
      case NR_UE_TimersAndConstants__t311_ms3000 :
        k = 3000;
        break;
      case NR_UE_TimersAndConstants__t311_ms5000 :
        k = 5000;
        break;
      case NR_UE_TimersAndConstants__t311_ms10000 :
        k = 10000;
        break;
      case NR_UE_TimersAndConstants__t311_ms15000 :
        k = 15000;
        break;
      case NR_UE_TimersAndConstants__t311_ms20000 :
        k = 20000;
        break;
      case NR_UE_TimersAndConstants__t311_ms30000 :
        k = 30000;
        break;
      default :
        AssertFatal(false, "Invalid T311 %ld\n", sib1->ue_TimersAndConstants->t311);
    }
    nr_timer_setup(&tac->T311, k, 10); // 10ms step
    switch (sib1->ue_TimersAndConstants->n310) {
      case NR_UE_TimersAndConstants__n310_n1 :
        tac->N310_k = 1;
        break;
      case NR_UE_TimersAndConstants__n310_n2 :
        tac->N310_k = 2;
        break;
      case NR_UE_TimersAndConstants__n310_n3 :
        tac->N310_k = 3;
        break;
      case NR_UE_TimersAndConstants__n310_n4 :
        tac->N310_k = 4;
        break;
      case NR_UE_TimersAndConstants__n310_n6 :
        tac->N310_k = 6;
        break;
      case NR_UE_TimersAndConstants__n310_n8 :
        tac->N310_k = 8;
        break;
      case NR_UE_TimersAndConstants__n310_n10 :
        tac->N310_k = 10;
        break;
      case NR_UE_TimersAndConstants__n310_n20 :
        tac->N310_k = 20;
        break;
      default :
        AssertFatal(false, "Invalid N310 %ld\n", sib1->ue_TimersAndConstants->n310);
    }
    switch (sib1->ue_TimersAndConstants->n311) {
      case NR_UE_TimersAndConstants__n311_n1 :
        tac->N311_k = 1;
        break;
      case NR_UE_TimersAndConstants__n311_n2 :
        tac->N311_k = 2;
        break;
      case NR_UE_TimersAndConstants__n311_n3 :
        tac->N311_k = 3;
        break;
      case NR_UE_TimersAndConstants__n311_n4 :
        tac->N311_k = 4;
        break;
      case NR_UE_TimersAndConstants__n311_n5 :
        tac->N311_k = 5;
        break;
      case NR_UE_TimersAndConstants__n311_n6 :
        tac->N311_k = 6;
        break;
      case NR_UE_TimersAndConstants__n311_n8 :
        tac->N311_k = 8;
        break;
      case NR_UE_TimersAndConstants__n311_n10 :
        tac->N311_k = 10;
        break;
      default :
        AssertFatal(false, "Invalid N311 %ld\n", sib1->ue_TimersAndConstants->n311);
    }
  }
  else
    LOG_E(NR_RRC,"SIB1 should not be NULL and neither UE_Timers_Constants\n");
}

void nr_rrc_set_sib1_timers_and_constants(NR_UE_Timers_Constants_t *tac, NR_SIB1_t *sib1)
{
  set_rlf_sib1_timers_and_constants(tac, sib1);
  if(sib1 && sib1->ue_TimersAndConstants) {
    int k = 0;
    switch (sib1->ue_TimersAndConstants->t300) {
      case NR_UE_TimersAndConstants__t300_ms100 :
        k = 100;
        break;
      case NR_UE_TimersAndConstants__t300_ms200 :
        k = 200;
        break;
      case NR_UE_TimersAndConstants__t300_ms300 :
        k = 300;
        break;
      case NR_UE_TimersAndConstants__t300_ms400 :
        k = 400;
        break;
      case NR_UE_TimersAndConstants__t300_ms600 :
        k = 600;
        break;
      case NR_UE_TimersAndConstants__t300_ms1000 :
        k = 1000;
        break;
      case NR_UE_TimersAndConstants__t300_ms1500 :
        k = 1500;
        break;
      case NR_UE_TimersAndConstants__t300_ms2000 :
        k = 2000;
        break;
      default :
        AssertFatal(false, "Invalid T300 %ld\n", sib1->ue_TimersAndConstants->t300);
    }
    nr_timer_setup(&tac->T300, k, 10); // 10ms step
    switch (sib1->ue_TimersAndConstants->t319) {
      case NR_UE_TimersAndConstants__t319_ms100 :
        k = 100;
        break;
      case NR_UE_TimersAndConstants__t319_ms200 :
        k = 200;
        break;
      case NR_UE_TimersAndConstants__t319_ms300 :
        k = 300;
        break;
      case NR_UE_TimersAndConstants__t319_ms400 :
        k = 400;
        break;
      case NR_UE_TimersAndConstants__t319_ms600 :
        k = 600;
        break;
      case NR_UE_TimersAndConstants__t319_ms1000 :
        k = 1000;
        break;
      case NR_UE_TimersAndConstants__t319_ms1500 :
        k = 1500;
        break;
      case NR_UE_TimersAndConstants__t319_ms2000 :
        k = 2000;
        break;
      default :
        AssertFatal(false, "Invalid T319 %ld\n", sib1->ue_TimersAndConstants->t319);
    }
    nr_timer_setup(&tac->T319, k, 10); // 10ms step
  }
  else
    LOG_E(NR_RRC,"SIB1 should not be NULL and neither UE_Timers_Constants\n");
}

void nr_rrc_handle_SetupRelease_RLF_TimersAndConstants(NR_UE_RRC_INST_t *rrc,
                                                       struct NR_SetupRelease_RLF_TimersAndConstants *rlf_TimersAndConstants)
{
  if(rlf_TimersAndConstants == NULL)
    return;

  NR_UE_Timers_Constants_t *tac = &rrc->timers_and_constants;
  NR_RLF_TimersAndConstants_t *rlf_tac = NULL;
  switch(rlf_TimersAndConstants->present){
    case NR_SetupRelease_RLF_TimersAndConstants_PR_release :
      // use values for timers T301, T310, T311 and constants N310, N311, as included in ue-TimersAndConstants received in SIB1
      set_rlf_sib1_timers_and_constants(tac, rrc->perNB[0].SInfo.sib1);
      break;
    case NR_SetupRelease_RLF_TimersAndConstants_PR_setup :
      rlf_tac = rlf_TimersAndConstants->choice.setup;
      if (rlf_tac == NULL)
        return;
      // (re-)configure the value of timers and constants in accordance with received rlf-TimersAndConstants
      int k = 0;
      switch (rlf_tac->t310) {
        case NR_RLF_TimersAndConstants__t310_ms0 :
          k = 0;
          break;
        case NR_RLF_TimersAndConstants__t310_ms50 :
          k = 50;
          break;
        case NR_RLF_TimersAndConstants__t310_ms100 :
          k = 100;
          break;
        case NR_RLF_TimersAndConstants__t310_ms200 :
          k = 200;
          break;
        case NR_RLF_TimersAndConstants__t310_ms500 :
          k = 500;
          break;
        case NR_RLF_TimersAndConstants__t310_ms1000 :
          k = 1000;
          break;
        case NR_RLF_TimersAndConstants__t310_ms2000 :
          k = 2000;
          break;
        case NR_RLF_TimersAndConstants__t310_ms4000 :
          k = 4000;
          break;
        case NR_RLF_TimersAndConstants__t310_ms6000 :
          k = 6000;
          break;
        default :
          AssertFatal(false, "Invalid T310 %ld\n", rlf_tac->t310);
      }
      nr_timer_setup(&tac->T310, k, 10); // 10ms step
      switch (rlf_tac->n310) {
        case NR_RLF_TimersAndConstants__n310_n1 :
          tac->N310_k = 1;
          break;
        case NR_RLF_TimersAndConstants__n310_n2 :
          tac->N310_k = 2;
          break;
        case NR_RLF_TimersAndConstants__n310_n3 :
          tac->N310_k = 3;
          break;
        case NR_RLF_TimersAndConstants__n310_n4 :
          tac->N310_k = 4;
          break;
        case NR_RLF_TimersAndConstants__n310_n6 :
          tac->N310_k = 6;
          break;
        case NR_RLF_TimersAndConstants__n310_n8 :
          tac->N310_k = 8;
          break;
        case NR_RLF_TimersAndConstants__n310_n10 :
          tac->N310_k = 10;
          break;
        case NR_RLF_TimersAndConstants__n310_n20 :
          tac->N310_k = 20;
          break;
        default :
          AssertFatal(false, "Invalid N310 %ld\n", rlf_tac->n310);
      }
      switch (rlf_tac->n311) {
        case NR_RLF_TimersAndConstants__n311_n1 :
          tac->N311_k = 1;
          break;
        case NR_RLF_TimersAndConstants__n311_n2 :
          tac->N311_k = 2;
          break;
        case NR_RLF_TimersAndConstants__n311_n3 :
          tac->N311_k = 3;
          break;
        case NR_RLF_TimersAndConstants__n311_n4 :
          tac->N311_k = 4;
          break;
        case NR_RLF_TimersAndConstants__n311_n5 :
          tac->N311_k = 5;
          break;
        case NR_RLF_TimersAndConstants__n311_n6 :
          tac->N311_k = 6;
          break;
        case NR_RLF_TimersAndConstants__n311_n8 :
          tac->N311_k = 8;
          break;
        case NR_RLF_TimersAndConstants__n311_n10 :
          tac->N311_k = 10;
          break;
        default :
          AssertFatal(false, "Invalid N311 %ld\n", rlf_tac->n311);
      }
      if (rlf_tac->ext1) {
        switch (rlf_tac->ext1->t311) {
          case NR_RLF_TimersAndConstants__ext1__t311_ms1000 :
            k = 1000;
            break;
          case NR_RLF_TimersAndConstants__ext1__t311_ms3000 :
            k = 3000;
            break;
          case NR_RLF_TimersAndConstants__ext1__t311_ms5000 :
            k = 5000;
            break;
          case NR_RLF_TimersAndConstants__ext1__t311_ms10000 :
            k = 10000;
            break;
          case NR_RLF_TimersAndConstants__ext1__t311_ms15000 :
            k = 15000;
            break;
          case NR_RLF_TimersAndConstants__ext1__t311_ms20000 :
            k = 20000;
            break;
          case NR_RLF_TimersAndConstants__ext1__t311_ms30000 :
            k = 30000;
            break;
          default :
            AssertFatal(false, "Invalid T311 %ld\n", rlf_tac->ext1->t311);
        }
        nr_timer_setup(&tac->T311, k, 10); // 10ms step
      }
      reset_rlf_timers_and_constants(tac);
      break;
    default :
      AssertFatal(false, "Invalid rlf_TimersAndConstants\n");
  }
}

void handle_rlf_sync(NR_UE_Timers_Constants_t *tac,
                     nr_sync_msg_t sync_msg)
{
  if (sync_msg == IN_SYNC) {
    tac->N310_cnt = 0;
    if (tac->T310.active) {
      tac->N311_cnt++;
      // Upon receiving N311 consecutive "in-sync" indications
      if (tac->N311_cnt >= tac->N311_k) {
        // stop timer T310
        nr_timer_stop(&tac->T310);
        tac->N311_cnt = 0;
      }
    }
  }
  else {
    // OUT_OF_SYNC
    tac->N311_cnt = 0;
    if(tac->T300.active ||
       tac->T301.active ||
       tac->T304.active ||
       tac->T310.active ||
       tac->T311.active ||
       tac->T319.active)
      return;
    tac->N310_cnt++;
    // upon receiving N310 consecutive "out-of-sync" indications
    if (tac->N310_cnt >= tac->N310_k) {
      // start timer T310
      nr_timer_start(&tac->T310);
      tac->N310_cnt = 0;
    }
  }
}

void set_default_timers_and_constants(NR_UE_Timers_Constants_t *tac)
{
  // 38.331 9.2.3 Default values timers and constants
  nr_timer_setup(&tac->T310, 1000, 10); // 10ms step
  nr_timer_setup(&tac->T310, 30000, 10); // 10ms step
  tac->N310_k = 1;
  tac->N311_k = 1;
}

void reset_rlf_timers_and_constants(NR_UE_Timers_Constants_t *tac)
{
  // stop timer T310 for this cell group, if running
  nr_timer_stop(&tac->T310);
  // reset the counters N310 and N311
  tac->N310_cnt = 0;
  tac->N311_cnt = 0;
}
