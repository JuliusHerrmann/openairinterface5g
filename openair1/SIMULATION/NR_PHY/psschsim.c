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

#include <string.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "common/config/config_userapi.h"
#include "common/utils/load_module_shlib.h"
#include "common/utils/LOG/log.h"
#include "common/ran_context.h"
#include "PHY/types.h"
#include "PHY/defs_nr_common.h"
#include "PHY/defs_nr_UE.h"
#include "PHY/defs_gNB.h"
#include "PHY/INIT/phy_init.h"
#include "PHY/NR_REFSIG/refsig_defs_ue.h"
#include "PHY/MODULATION/nr_modulation.h"
#include "PHY/MODULATION/modulation_eNB.h"
#include "PHY/MODULATION/modulation_UE.h"
#include "PHY/NR_TRANSPORT/nr_transport_proto.h"
#include "PHY/NR_TRANSPORT/nr_dlsch.h"
#include "PHY/NR_TRANSPORT/nr_ulsch.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"

#include "SCHED_NR/sched_nr.h"
#include "openair1/SIMULATION/TOOLS/sim.h"
#include "openair1/SIMULATION/RF/rf.h"
#include "openair1/SIMULATION/NR_PHY/nr_unitary_defs.h"
#include "openair1/SIMULATION/NR_PHY/nr_dummy_functions.c"
#include "common/utils/threadPool/thread-pool.h"
#include "openair2/LAYER2/NR_MAC_COMMON/nr_mac_common.h"
#include "executables/nr-uesoftmodem.h"
#include "PHY/impl_defs_top.h"
#include "PHY/MODULATION/nr_modulation.h"
#include "PHY/MODULATION/modulation_common.h"

#define DEBUG_NR_SLSCHSIM 1

// typedef struct {
//   uint8_t priority;
//   uint8_t freq_res;
//   uint8_t time_res;
//   uint8_t period;
//   uint16_t dmrs_pattern;
//   uint8_t mcs;
//   uint8_t beta_offset;
//   uint8_t dmrs_port;
// } SCI_1_A;

typedef struct {
  double scs;
  double bw;
  double fs;
} BW;

// THREAD_STRUCT thread_struct;
// PHY_VARS_NR_UE *txUE;
// PHY_VARS_NR_UE *rxUE;

#define HNA_SIZE 6 * 68 * 384 // [hna] 16 segments, 68*Zc
#define SCI2_LEN_SIZE 35
RAN_CONTEXT_t RC;
double cpuf;
uint16_t NB_UE_INST = 1;
openair0_config_t openair0_cfg[MAX_CARDS];
uint8_t const nr_rv_round_map[4] = {0, 2, 3, 1};
const short conjugate[8]__attribute__((aligned(16))) = {-1,1,-1,1,-1,1,-1,1};
const short conjugate2[8]__attribute__((aligned(16))) = {1,-1,1,-1,1,-1,1,-1};

uint64_t get_softmodem_optmask(void) {return 0;}
static softmodem_params_t softmodem_params;
softmodem_params_t *get_softmodem_params(void) {
  return &softmodem_params;
}

void init_downlink_harq_status(NR_DL_UE_HARQ_t *dl_harq) {}

double snr0 = 100;
double snr1 = 2.0;
uint8_t snr1set = 0;
int n_trials = 1;
uint8_t n_tx = 1;
uint8_t n_rx = 1;
int ssb_subcarrier_offset = 0;
FILE *input_fd = NULL;
SCM_t channel_model = AWGN;
int nb_rb = 106;
int N_RB_UL = 106;
int N_RB_DL = 106;
int mu = 1;
int loglvl = OAILOG_WARNING;
int seed = 0;

static void get_sim_cl_opts(int argc, char **argv)
{
    char c;
    while ((c = getopt(argc, argv, "F:g:hIL:m:M:n:N:o:O:p:P:r:R:s:S:x:y:z:")) != -1) {
    switch (c) {
      case 'F':
        input_fd = fopen(optarg, "r");
        if (input_fd == NULL) {
          printf("Problem with filename %s. Exiting.\n", optarg);
          exit(-1);
        }
        break;

      case 'g':
        switch((char)*optarg) {
          case 'A':
            channel_model=SCM_A;
            break;

          case 'B':
            channel_model=SCM_B;
            break;

          case 'C':
            channel_model=SCM_C;
            break;

          case 'D':
            channel_model=SCM_D;
            break;

          case 'E':
            channel_model=EPA;
            break;

          case 'F':
            channel_model=EVA;
            break;

          case 'G':
            channel_model=ETU;
            break;

          default:
            printf("Unsupported channel model! Exiting.\n");
            exit(-1);
          }
        break;

      case 'L':
        loglvl = atoi(optarg);
        break;

      case 'm':
        mu = atoi(optarg);
        break;

      case 'n':
        n_trials = atoi(optarg);
        break;

      case 'R':
        N_RB_UL = atoi(optarg);
        break;

      case 'r':
        nb_rb = atoi(optarg);
        break;

      case 's':
        snr0 = atof(optarg);
        break;

      case 'S':
        snr1 = atof(optarg);
        snr1set = 1;
        break;

      case 'y':
        n_tx = atoi(optarg);
        if ((n_tx == 0) || (n_tx > 2)) {
          printf("Unsupported number of TX antennas %d. Exiting.\n", n_tx);
          exit(-1);
        }
        break;

      case 'z':
        n_rx = atoi(optarg);
        if ((n_rx == 0) || (n_rx > 2)) {
          printf("Unsupported number of RX antennas %d. Exiting.\n", n_rx);
          exit(-1);
        }
        break;

      default:
      case 'h':
          printf("%s -h(elp) -g channel_model -n n_frames -s snr0 -S snr1 -p(extended_prefix) -y TXant -z RXant -M -N cell_id -R -F input_filename -m -l -r\n", argv[0]);
          //printf("%s -h(elp) -p(extended_prefix) -N cell_id -f output_filename -F input_filename -g channel_model -n n_frames -t Delayspread -s snr0 -S snr1 -x transmission_mode -y TXant -z RXant -i Intefrence0 -j Interference1 -A interpolation_file -C(alibration offset dB) -N CellId\n", argv[0]);
          printf("-h This message\n");
          printf("-g [A,B,C,D,E,F,G] Use 3GPP SCM (A,B,C,D) or 36-101 (E-EPA,F-EVA,G-ETU) models (ignores delay spread and Ricean factor)\n");
          printf("-n Number of frames to simulate\n");
          printf("-s Starting SNR, runs from SNR0 to SNR0 + 5 dB.  If n_frames is 1 then just SNR is simulated\n");
          printf("-S Ending SNR, runs from SNR0 to SNR1\n");
          printf("-p Use extended prefix mode\n");
          printf("-y Number of TX antennas used in eNB\n");
          printf("-z Number of RX antennas used in UE\n");
          printf("-W number of layer\n");
          printf("-R N_RB_UL\n");
          printf("-r nb_rb\n");
          printf("-F Input filename (.txt format) for RX conformance testing\n");
          printf("-m MCS\n");
          printf("-l number of symbol\n");
          printf("-r number of RB\n");
        exit (-1);
        break;
    }
  }
}


void nr_phy_config_request_psschsim(PHY_VARS_NR_UE *ue,
                                    int N_RB_UL,
                                    int N_RB_DL,
                                    int mu,
                                    uint64_t position_in_burst)
{
  NR_DL_FRAME_PARMS *fp                 = &ue->frame_parms;
  fapi_nr_config_request_t *nrUE_config = &ue->nrUE_config;
  uint64_t rev_burst=0;
  for (int i = 0; i < 64; i++)
    rev_burst |= (((position_in_burst >> (63 - i)) & 0x01) << i);

  nrUE_config->ssb_config.scs_common               = mu;
  nrUE_config->ssb_table.ssb_subcarrier_offset     = ssb_subcarrier_offset;
  nrUE_config->ssb_table.ssb_offset_point_a        = (N_RB_UL - 20) >> 1;
  nrUE_config->ssb_table.ssb_mask_list[1].ssb_mask = (rev_burst)&(0xFFFFFFFF);
  nrUE_config->ssb_table.ssb_mask_list[0].ssb_mask = (rev_burst >> 32)&(0xFFFFFFFF);
  nrUE_config->cell_config.frame_duplex_type       = TDD;
  nrUE_config->ssb_table.ssb_period                = 1; //10ms
  nrUE_config->carrier_config.dl_grid_size[mu]     = N_RB_DL;
  nrUE_config->carrier_config.ul_grid_size[mu]     = N_RB_UL;
  nrUE_config->carrier_config.num_tx_ant           = fp->nb_antennas_tx;
  nrUE_config->carrier_config.num_rx_ant           = fp->nb_antennas_rx;
  nrUE_config->tdd_table.tdd_period                = 0;

  ue->mac_enabled = 1;
  ue->is_synchronized_sl = 1;
  if (mu == 0) {
    fp->dl_CarrierFreq = 2600000000;
    fp->ul_CarrierFreq = 2600000000;
    fp->nr_band = 38;
  } else if (mu == 1) {
    fp->dl_CarrierFreq = 3600000000;
    fp->ul_CarrierFreq = 3600000000;
    fp->sl_CarrierFreq = 2600000000;
    fp->nr_band = 78;
  } else if (mu == 3) {
    fp->dl_CarrierFreq = 27524520000;
    fp->ul_CarrierFreq = 27524520000;
    fp->nr_band = 261;
  }

  fp->threequarter_fs = 0;
  nrUE_config->carrier_config.dl_bandwidth = config_bandwidth(mu, N_RB_DL, fp->nr_band);

  nr_init_frame_parms_ue(fp, nrUE_config, fp->nr_band);
  fp->ofdm_offset_divisor = UINT_MAX;
  ue->configured = 1;
  LOG_I(NR_PHY, "tx UE configured\n");
}

void set_sci(SCI_1_A *sci, uint8_t mcs) {
  sci->period = 0;
  sci->dmrs_pattern = 0b0001000001000; // LSB is slot 1 and MSB is slot 13
  sci->beta_offset = 0;
  sci->dmrs_port = 0;
  sci->priority = 0;
  sci->freq_res = 1;
  sci->time_res = 1;
  sci->mcs = mcs;
}

void set_fs_bw(PHY_VARS_NR_UE *UE, int mu, int N_RB, BW *bw_setting) {
  double scs = 0, fs = 0, bw = 0;
  switch (mu) {
    case 1:
      scs = 30000;
      UE->frame_parms.Lmax = 1;
      if (N_RB == 217) {
        fs = 122.88e6;
        bw = 80e6;
      }
      else if (N_RB == 245) {
        fs = 122.88e6;
        bw = 90e6;
      }
      else if (N_RB == 273) {
        fs = 122.88e6;
        bw = 100e6;
      }
      else if (N_RB == 106) {
        fs = 61.44e6;
        bw = 40e6;
      }
      else AssertFatal(1 == 0, "Unsupported numerology for mu %d, N_RB %d\n", mu, N_RB);
      break;
    case 3:
      UE->frame_parms.Lmax = 64;
      scs = 120000;
      if (N_RB == 66) {
        fs = 122.88e6;
        bw = 100e6;
      }
      else AssertFatal(1 == 0, "Unsupported numerology for mu %d, N_RB %d\n", mu, N_RB);
      break;
    default:
      AssertFatal(1 == 0, "Unsupported numerology for mu %d, N_RB %d\n", mu, N_RB);
      break;
  }
  bw_setting->scs = scs;
  bw_setting->fs = fs;
  bw_setting->bw = bw;
  return;
}

nrUE_params_t nrUE_params;
nrUE_params_t *get_nrUE_params(void) {
  return &nrUE_params;
}

int main(int argc, char **argv)
{
  get_softmodem_params()->sl_mode = 2;
  if (load_configmodule(argc, argv, CONFIG_ENABLECMDLINEONLY) == 0) {
    exit_fun("[NR_PSSCHSIM] Error, configuration module init failed\n");
  }
  get_sim_cl_opts(argc, argv);
  randominit(0);
  // logging initialization
  logInit();
  set_glog(loglvl);
  load_nrLDPClib(NULL);

  PHY_VARS_NR_UE *txUE = malloc(sizeof(PHY_VARS_NR_UE));
  txUE->frame_parms.N_RB_DL = N_RB_DL;
  txUE->frame_parms.N_RB_UL = N_RB_UL;
  txUE->frame_parms.Ncp = NORMAL;
  txUE->frame_parms.nb_antennas_tx = 1;
  txUE->frame_parms.nb_antennas_rx = n_rx;
  txUE->max_ldpc_iterations = 5;
  PHY_VARS_NR_UE *rxUE = malloc(sizeof(PHY_VARS_NR_UE));
  rxUE->frame_parms.nb_antennas_tx = n_tx;
  rxUE->frame_parms.nb_antennas_rx = 1;
  initTpool("n", &rxUE->threadPool, true);
  initNotifiedFIFO(&rxUE->respDecode);

  uint64_t burst_position = 0x01;
  nr_phy_config_request_psschsim(txUE, N_RB_UL, N_RB_DL, mu, burst_position);
  nr_phy_config_request_psschsim(rxUE, N_RB_UL, N_RB_DL, mu, burst_position);

  BW *bw_setting = malloc(sizeof(BW));
  set_fs_bw(txUE, mu, N_RB_UL, bw_setting);


  double DS_TDL = 300e-9; //.03;
  channel_desc_t *UE2UE = new_channel_desc_scm(n_tx, n_rx, channel_model,
                                               bw_setting->fs,
                                               bw_setting->bw,
                                               DS_TDL,
                                               0, 0, 0, 0);

  if (UE2UE == NULL) {
    printf("Problem generating channel model. Exiting.\n");
    free(bw_setting);
    exit(-1);
  }

  if (init_nr_ue_signal(txUE, 1) != 0 || init_nr_ue_signal(rxUE, 1) != 0) {
    printf("Error at UE NR initialization.\n");
    free(bw_setting);
    free(txUE);
    free(rxUE);
    exit(-1);
  }
#ifdef DEBUG_NR_SLSCHSIM
  for (int sf = 0; sf < 2; sf++) {
    txUE->slsch[sf][0] = new_nr_ue_ulsch(nb_rb, 8, &txUE->frame_parms);
    if (!txUE->slsch[sf][0]) {
      printf("Can't get ue ulsch structures.\n");
      exit(-1);
    }
  }
#endif
  get_softmodem_params()->sync_ref = false;
  init_nr_ue_transport(txUE);
  get_softmodem_params()->sync_ref = true;
  init_nr_ue_transport(rxUE);

  // s_re = malloc(n_tx*sizeof(double*));
  // s_im = malloc(n_tx*sizeof(double*));
  // r_re = malloc(n_rx*sizeof(double*));
  // r_im = malloc(n_rx*sizeof(double*));


  uint8_t UE_id = 0;
  uint16_t nb_symb_sch = 12;
  uint8_t dmrsConfigType = 0;
  uint8_t nb_re_dmrs = 6;
  uint8_t Nl = 1; // number of layers
  uint8_t Imcs = 9;
  uint8_t  SCI2_mod_order = 2;
  uint16_t dmrsSymbPos = 16 + 1024; // symbol 4 and 10
  uint8_t length_dmrs = get_num_dmrs(dmrsSymbPos);
  unsigned char harq_pid = 0;


  NR_UE_ULSCH_t *slsch_ue = txUE->slsch[0][0];

  if ((Nl == 4) || (Nl == 3))
    nb_re_dmrs = nb_re_dmrs * 2;

  uint8_t mod_order = nr_get_Qm_ul(Imcs, 0);
  uint16_t code_rate = nr_get_code_rate_ul(Imcs, 0);
  unsigned int available_bits = nr_get_G(nb_rb, nb_symb_sch, nb_re_dmrs, length_dmrs, mod_order, Nl);
  unsigned int TBS = nr_compute_tbs(mod_order, code_rate, nb_rb, nb_symb_sch, nb_re_dmrs * length_dmrs, 0, 0, Nl);
  printf("\nAvailable bits %u TBS %u mod_order %d\n", available_bits, TBS, mod_order);

  NR_UE_DLSCH_t *slsch_ue_rx = rxUE->slsch_rx[0][0][0];
  NR_DL_UE_HARQ_t *harq_process_rxUE = slsch_ue_rx->harq_processes[harq_pid];
  harq_process_rxUE->Nl = Nl;
  harq_process_rxUE->Qm = mod_order;
  harq_process_rxUE->nb_rb = nb_rb;
  harq_process_rxUE->TBS = TBS >> 3;
  harq_process_rxUE->n_dmrs_cdm_groups = 1;
  harq_process_rxUE->dlDmrsSymbPos = dmrsSymbPos;
  harq_process_rxUE->mcs = Imcs;
  harq_process_rxUE->dmrsConfigType = dmrsConfigType;
  harq_process_rxUE->R = code_rate;
  harq_process_rxUE->codeword = 0;
  nfapi_nr_pssch_pdu_t *rel16_sl_rx = &harq_process_rxUE->pssch_pdu;
  rel16_sl_rx->mcs_index            = Imcs;
  rel16_sl_rx->pssch_data.rv_index  = 0;
  rel16_sl_rx->target_code_rate     = code_rate;
  rel16_sl_rx->pssch_data.tb_size   = TBS >> 3; // bytes
  rel16_sl_rx->pssch_data.sci2_size = SCI2_LEN_SIZE >> 3;
  rel16_sl_rx->maintenance_parms_v3.ldpcBaseGraph = get_BG(TBS, code_rate);
  rel16_sl_rx->nr_of_symbols  = nb_symb_sch; // number of symbols per slot
  rel16_sl_rx->start_symbol_index = 1;
  rel16_sl_rx->ul_dmrs_symb_pos = harq_process_rxUE->dlDmrsSymbPos;
  rel16_sl_rx->nrOfLayers = harq_process_rxUE->Nl;
  rel16_sl_rx->num_dmrs_cdm_grps_no_data = 1;


  NR_UL_UE_HARQ_t *harq_process_txUE = txUE->slsch[0][0]->harq_processes[harq_pid];
  DevAssert(harq_process_txUE);
  uint8_t N_PRB_oh = 0;
  uint16_t N_RE_prime = NR_NB_SC_PER_RB * nb_symb_sch - nb_re_dmrs - N_PRB_oh;
  uint8_t nb_codewords = 1;
  harq_process_txUE->pssch_pdu.mcs_index = Imcs;
  harq_process_txUE->pssch_pdu.nrOfLayers = Nl;
  harq_process_txUE->pssch_pdu.rb_size = nb_rb;
  harq_process_txUE->pssch_pdu.nr_of_symbols = nb_symb_sch;
  harq_process_txUE->pssch_pdu.dmrs_config_type = dmrsConfigType;
  harq_process_txUE->num_of_mod_symbols = N_RE_prime * nb_rb * nb_codewords;
  harq_process_txUE->pssch_pdu.pssch_data.rv_index = 0;
  harq_process_txUE->pssch_pdu.pssch_data.tb_size  = TBS >> 3;
  harq_process_txUE->pssch_pdu.pssch_data.sci2_size = SCI2_LEN_SIZE >> 3;
  harq_process_txUE->pssch_pdu.target_code_rate = code_rate;
  harq_process_txUE->pssch_pdu.qam_mod_order = mod_order;
  harq_process_txUE->pssch_pdu.sl_dmrs_symb_pos = dmrsSymbPos;
  harq_process_txUE->pssch_pdu.num_dmrs_cdm_grps_no_data = 1;
  harq_process_txUE->pssch_pdu.start_symbol_index = 1;
  unsigned char *test_input = harq_process_txUE->a;
  uint64_t *sci_input = harq_process_txUE->a_sci2;

  SCI_1_A *sci1 = &harq_process_txUE->pssch_pdu.sci1;
  set_sci(sci1, Imcs);

  crcTableInit();
  for (int i = 0; i < TBS / 8; i++)
    test_input[i] = (unsigned char) (i+3);//rand();

  uint64_t u = pow(2,SCI2_LEN_SIZE) - 1;
  *sci_input = u;//rand() % (u - 0 + 1);
  printf("the sci2 is:%"PRIu64"\n",*sci_input);

#ifdef DEBUG_NR_PSSCHSIM
  for (int i = 0; i < TBS / 8; i++) printf("i = %d / %d test_input[i]  =%hhu \n", i, TBS / 8, test_input[i]);
#endif

  int frame = 0;
  int slot = 0;
  int32_t **txdata = txUE->common_vars.txdata;
  unsigned int G = nr_get_G(nb_rb, nb_symb_sch,
                            nb_re_dmrs, length_dmrs, mod_order, Nl);
  nr_ue_slsch_tx_procedures(txUE, harq_pid, frame, slot);

  nr_ue_pssch_common_procedures(txUE, slot, &txUE->frame_parms, Nl, NR_LINK_TYPE_SL);

  unsigned int G_SCI2_bits = harq_process_txUE->B_sci2;
  uint32_t M_SCI2_bits = G_SCI2_bits * Nl;

  printf("tx is done\n");


  /////////////////////////LLRs computation/////////////////////////
  // this is a small hack :D
  slsch_ue_rx->harq_processes[0]->B_sci2 = slsch_ue->harq_processes[harq_pid]->B_sci2;
  NR_UE_PSSCH *pssch_vars = rxUE->pssch_vars[UE_id];
  int16_t **ulsch_llr = rxUE->pssch_vars[UE_id]->llr;
  int16_t **ulsch_llr_layers = rxUE->pssch_vars[UE_id]->llr_layers;
  int16_t **ulsch_llr_layers_adj = rxUE->pssch_vars[UE_id]->llr_layers_adj;
  int nb_re_SCI2 = slsch_ue_rx->harq_processes[0]->B_sci2 / SCI2_mod_order;

  uint16_t num_sci2_symbs = (M_SCI2_bits << 1) / SCI2_mod_order;
  uint16_t num_data_symbs = (G << 1) / mod_order;
  uint16_t num_sci2_samples = num_sci2_symbs >> 1;

  int32_t **rxdataF = rxUE->common_vars.common_vars_rx_data_per_thread[0].rxdataF;
  UE_nr_rxtx_proc_t proc;
  proc.thread_id = 0;
  int start_symbol = rel16_sl_rx->start_symbol_index;
  uint8_t number_of_symbols = rel16_sl_rx->nr_of_symbols;

  //uint8_t  symbol = 5;
  int32_t a = 1;
  int32_t b = 1;

  //unsigned int errors_bit_uncoded = 0;
  unsigned int errors_bit = 0;
  unsigned int n_errors = 0;
  unsigned int n_false_positive = 0;
  //double modulated_input[HNA_SIZE];
  unsigned char test_input_bit[HNA_SIZE];
  //short channel_output_fixed[HNA_SIZE];
  //short channel_output_uncoded[HNA_SIZE];
  unsigned char estimated_output_bit[HNA_SIZE];
  double snr_step = 0.2;
  snr1 = snr1set == 0 ? snr0 + snr_step * 1 : snr1;
  int frame_length_complex_samples = txUE->frame_parms.samples_per_subframe * NR_NUMBER_OF_SUBFRAMES_PER_FRAME;
  double **r_re = malloc(NR_MAX_NB_LAYERS_SL * sizeof(double*));
  double **r_im = malloc(NR_MAX_NB_LAYERS_SL * sizeof(double*));
  for (int i = 0; i < NR_MAX_NB_LAYERS_SL; i++) {
    r_re[i] = malloc16_clear(frame_length_complex_samples * sizeof(double));
    r_im[i] = malloc16_clear(frame_length_complex_samples * sizeof(double));
  }
  for (double SNR = snr0; SNR < snr1; SNR += snr_step) {
    n_errors = 0;
    n_false_positive = 0;

    //double SNR_lin = pow(10, SNR / 10.0);
    //double sigma = 1.0 / sqrt(2 * SNR_lin);
    for (int trial = 0; trial < n_trials; trial++) {

      for (int i = 0; i < frame_length_complex_samples; i++) {
        for (int aa = 0; aa < txUE->frame_parms.nb_antennas_tx; aa++) {
          r_re[aa][i] = ((double)(((short *)txdata[aa]))[(i << 1)]);
          r_im[aa][i] = ((double)(((short *)txdata[aa]))[(i << 1) + 1]);
        }
      }

      for (int i = 0; i < frame_length_complex_samples; i++) {
        //double sigma2_dB = 20 * log10((double)AMP / 4) - SNR;
        //double sigma2 = pow(10, sigma2_dB / 10);
        for (int aa = 0; aa < rxUE->frame_parms.nb_antennas_rx; aa++) {
          ((short*) rxUE->common_vars.rxdata[aa])[2 * i] = (short) ((r_re[aa][i]));
          ((short*) rxUE->common_vars.rxdata[aa])[2 * i + 1] = (short) ((r_im[aa][i]));
        }
      }

      uint32_t rxdataF_ext_offset = 0;
      uint32_t nb_re_sci2 = nb_re_SCI2;
      printf("start_symbol %d \n", start_symbol);
      printf("number_of_symbols %u \n", number_of_symbols);
      printf("M_SCI2_bits %u \n", M_SCI2_bits);
      printf("num_sci2_samples %u \n", num_sci2_samples);
      uint32_t data_offset = num_sci2_samples;
      uint32_t sci2_offset = 0;

      for (int aa = 0; aa < rxUE->frame_parms.nb_antennas_rx; aa++) {
        for (int ofdm_symbol = 0; ofdm_symbol < NR_NUMBER_OF_SYMBOLS_PER_SLOT; ofdm_symbol++) {
            nr_slot_fep_ul(&rxUE->frame_parms, rxUE->common_vars.rxdata[aa], rxdataF[aa], ofdm_symbol, slot, 0);
        }
        apply_nr_rotation_ul(&rxUE->frame_parms, rxdataF[aa], slot, 0, NR_NUMBER_OF_SYMBOLS_PER_SLOT, NR_LINK_TYPE_SL);
      }
      for (uint8_t symbol = start_symbol; symbol < number_of_symbols; symbol++) {
        uint8_t dmrs_symbol_flag = (rel16_sl_rx->ul_dmrs_symb_pos >> symbol) & 0x01;
        uint16_t nb_re_sci1 = 0;
        if (1 <= symbol && symbol <= 3) {
          nb_re_sci1 = NR_NB_SC_PER_RB * NB_RB_SCI1;
        }
        uint32_t allocatable_sci2_re = min(nb_re_sci2, NR_NB_SC_PER_RB * harq_process_rxUE->nb_rb / 2 - nb_re_sci1);

        nr_slsch_extract_rbs(rxdataF,
                             pssch_vars,
                             slot,
                             symbol,
                             dmrs_symbol_flag,
                             rel16_sl_rx,
                             &rxUE->frame_parms,
                             harq_process_rxUE);

        for (int aatx = 0; aatx < Nl; aatx++) {
          if (dmrs_symbol_flag == 0) {
            nr_slsch_compute_llr(&pssch_vars->rxdataF_ext[aatx*rxUE->frame_parms.nb_antennas_rx][symbol * harq_process_rxUE->nb_rb * NR_NB_SC_PER_RB],
                                &a, &b,
                                &ulsch_llr_layers[aatx*rxUE->frame_parms.nb_antennas_rx][symbol * harq_process_rxUE->nb_rb * NR_NB_SC_PER_RB],
                                (harq_process_rxUE->nb_rb * NR_NB_SC_PER_RB - nb_re_sci1) / NR_NB_SC_PER_RB,
                                harq_process_rxUE->nb_rb * NR_NB_SC_PER_RB - nb_re_sci1,
                                symbol, mod_order);

            memcpy(&ulsch_llr_layers_adj[aatx*rxUE->frame_parms.nb_antennas_rx][data_offset * 2],
                    &ulsch_llr_layers[aatx*rxUE->frame_parms.nb_antennas_rx][symbol * harq_process_rxUE->nb_rb * NR_NB_SC_PER_RB],
                    sizeof(uint32_t) * (harq_process_rxUE->nb_rb * NR_NB_SC_PER_RB - nb_re_sci1));

            rxdataF_ext_offset += harq_process_rxUE->nb_rb * NR_NB_SC_PER_RB - nb_re_sci1;
            data_offset += harq_process_rxUE->nb_rb * NR_NB_SC_PER_RB - nb_re_sci1;
          } else {
            if (allocatable_sci2_re > 0) {
              nr_slsch_compute_llr(&pssch_vars->rxdataF_ext[aatx*rxUE->frame_parms.nb_antennas_rx][symbol * harq_process_rxUE->nb_rb * NR_NB_SC_PER_RB],
                                  &a, &b,
                                  &ulsch_llr_layers[aatx*rxUE->frame_parms.nb_antennas_rx][symbol * harq_process_rxUE->nb_rb * NR_NB_SC_PER_RB],
                                  allocatable_sci2_re / 6,
                                  allocatable_sci2_re,
                                  symbol, SCI2_mod_order);

              memcpy(&ulsch_llr_layers_adj[aatx*rxUE->frame_parms.nb_antennas_rx][sci2_offset * 2],
                     &ulsch_llr_layers[aatx*rxUE->frame_parms.nb_antennas_rx][symbol * harq_process_rxUE->nb_rb * NR_NB_SC_PER_RB],
                     sizeof(uint32_t) * allocatable_sci2_re);

              sci2_offset += allocatable_sci2_re;
            }
            uint32_t diff_re = NR_NB_SC_PER_RB * harq_process_rxUE->nb_rb / 2 - nb_re_sci1 - allocatable_sci2_re;
            if (diff_re > 0) {
              uint32_t offset = allocatable_sci2_re;
              nr_slsch_compute_llr(&pssch_vars->rxdataF_ext[aatx*rxUE->frame_parms.nb_antennas_rx][symbol * harq_process_rxUE->nb_rb * NR_NB_SC_PER_RB + offset],
                                  &a, &b,
                                  &ulsch_llr_layers[aatx*rxUE->frame_parms.nb_antennas_rx][symbol * harq_process_rxUE->nb_rb * NR_NB_SC_PER_RB + offset],
                                  diff_re / NR_NB_SC_PER_RB,
                                  diff_re,
                                  symbol, mod_order);

              memcpy(&ulsch_llr_layers_adj[aatx*rxUE->frame_parms.nb_antennas_rx][data_offset * 2],
                    &ulsch_llr_layers[aatx*rxUE->frame_parms.nb_antennas_rx][symbol * harq_process_rxUE->nb_rb * NR_NB_SC_PER_RB + offset],
                    sizeof(uint32_t) * diff_re);

              data_offset += diff_re;
            }
            rxdataF_ext_offset += harq_process_rxUE->nb_rb * NR_NB_SC_PER_RB - nb_re_sci1;
            if (allocatable_sci2_re > 0) {
              nb_re_sci2 -= allocatable_sci2_re;
            }
          }
        }
      } //symbol

       // For SCI2
      nr_dlsch_layer_demapping(rxUE->pssch_vars[UE_id]->llr,
                             harq_process_rxUE->Nl,
                             SCI2_mod_order,
                             num_sci2_symbs,
                             harq_process_rxUE->codeword,
                             -1,
                             rxUE->pssch_vars[UE_id]->llr_layers_adj);

      int16_t *dst_data = ulsch_llr[0] + num_sci2_symbs * harq_process_rxUE->Nl;
      int16_t *src_data = ulsch_llr_layers_adj[0] + num_sci2_symbs;

      // For Data
      nr_dlsch_layer_demapping(&dst_data,
                              harq_process_rxUE->Nl,
                              mod_order,
                              num_data_symbs,
                              harq_process_rxUE->codeword,
                              -1,
                              &src_data);

      /////////////////////////SLSCH descrambling/////////////////////////
      // hacky [TODO]: size and Nid should be calcualted in receiver
      // Nid is Nidx calcualted in scrambler from CRC
      nr_codeword_unscrambling_sl(ulsch_llr[0], harq_process_txUE->B_multiplexed, slsch_ue_rx->harq_processes[0]->B_sci2, txUE->slsch[0][0]->Nidx, Nl);


      uint32_t ret = nr_slsch_decoding(rxUE, &proc, ulsch_llr[0],
                                &rxUE->frame_parms, slsch_ue_rx,
                                slsch_ue_rx->harq_processes[0], frame,
                                nb_symb_sch, slot, harq_pid);

      if (ret)
        n_errors++;

      errors_bit = 0;
      for (int i = 0; i < min(80, TBS); i++) {
        estimated_output_bit[i] = (harq_process_rxUE->b[i / 8] & (1 << (i & 7))) >> (i & 7);
        test_input_bit[i] = (test_input[i / 8] & (1 << (i & 7))) >> (i & 7); // Further correct for multiple segments
        if(i % 8 == 0){
           printf("TxByte : %2u  vs  %2u : RxByte\n", test_input[i / 8], harq_process_rxUE->b[i / 8]);
        }
#if DEBUG_NR_PSSCHSIM
        printf("tx bit: %u, rx bit: %u\n",test_input_bit[i],estimated_output_bit[i]);
#endif
        if (estimated_output_bit[i] != test_input_bit[i]) {
          errors_bit++;
        }
      }
      if (errors_bit > 0) {
        n_false_positive++;
        if (n_trials == 1)
          printf("errors_bit %u (trial %d)\n", errors_bit, trial);
      }
    } // trial

    printf("*****************************************\n");
    printf("SNR %f, BLER %f (false positive %f)\n", SNR,
          (float) n_errors / (float) n_trials,
          (float) n_false_positive / (float) n_trials);
    printf("*****************************************\n");
    printf("\n");

    if (n_errors == 0) {
      printf("PSSCH test OK\n");
      printf("\n");
      break;
    }
    printf("\n");

  } // snr

  //term_nr_ue_transport(txUE);
  term_nr_ue_transport(rxUE);
  term_nr_ue_signal(rxUE, 1);
  term_nr_ue_signal(txUE, 1);
  free(txUE);
  free(rxUE);

  free_channel_desc_scm(UE2UE);
  free(bw_setting);

  loader_reset();
  logTerm();
  return (0);
}