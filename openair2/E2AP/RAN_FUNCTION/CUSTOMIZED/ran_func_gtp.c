#include "ran_func_gtp.h"
#include "openair2/E2AP/flexric/test/rnd/fill_rnd_data_gtp.h"
#include <assert.h>

static
const int mod_id = 0;

void read_gtp_sm(void * data)
{
  assert(data != NULL);

  gtp_ind_data_t* gtp = (gtp_ind_data_t*)(data);
  //fill_gtp_ind_data(gtp);

  gtp->msg.tstamp = time_now_us();

  NR_UEs_t *UE_info = &RC.nrmac[mod_id]->UE_info;
  size_t num_ues = 0;
  UE_iterator(UE_info->list, ue) {
    if (ue)
      num_ues += 1;
  }

  gtp->msg.len = num_ues;
  if(gtp->msg.len > 0){
    gtp->msg.ngut = calloc(gtp->msg.len, sizeof(gtp_ngu_t_stats_t) );
    assert(gtp->msg.ngut != NULL);
  }

  size_t i = 0;
  UE_iterator(UE_info->list, UE)
  {
    uint16_t const rnti = UE->rnti;
    struct rrc_gNB_ue_context_s *ue_context_p = rrc_gNB_get_ue_context_by_rnti(RC.nrrrc[mod_id], rnti);
    if (ue_context_p != NULL) {
      gtp->msg.ngut[i].rnti = ue_context_p->ue_context.rnti;
      int nb_pdu_session = ue_context_p->ue_context.nb_of_pdusessions;
      if (nb_pdu_session > 0) {
        int nb_pdu_idx = nb_pdu_session - 1;
        gtp->msg.ngut[i].teidgnb = ue_context_p->ue_context.pduSession[nb_pdu_idx].param.gNB_teid_N3;
        gtp->msg.ngut[i].teidupf = ue_context_p->ue_context.pduSession[nb_pdu_idx].param.UPF_teid_N3;
        // TODO: one PDU session has multiple QoS Flow
        int nb_qos_flow = ue_context_p->ue_context.pduSession[nb_pdu_idx].param.nb_qos;
        if (nb_qos_flow > 0) {
          gtp->msg.ngut[i].qfi = ue_context_p->ue_context.pduSession[nb_pdu_idx].param.qos[nb_qos_flow - 1].qfi;
        }
      }
    } else {
      LOG_W(NR_RRC,"rrc_gNB_get_ue_context return NULL\n");
      // if (gtp->msg.ngut != NULL) free(gtp->msg.ngut);
    }
    i++;
  }

}

void read_gtp_setup_sm(void* data)
{
  assert(data != NULL);
  assert(0 !=0 && "Not supported");
}

sm_ag_if_ans_t write_ctrl_gtp_sm(void const* src)
{
  assert(src != NULL);
  assert(0 !=0 && "Not supported");
}

