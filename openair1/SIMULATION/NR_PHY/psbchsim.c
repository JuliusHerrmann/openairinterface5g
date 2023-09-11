
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "common/config/config_userapi.h"
#include "common/ran_context.h"
#include "PHY/types.h"
#include "PHY/defs_nr_common.h"
#include "PHY/defs_nr_UE.h"
#include "PHY/defs_gNB.h"
#include "PHY/phy_vars.h"
#include "NR_MasterInformationBlockSidelink.h"
#include "PHY/INIT/phy_init.h"
#include "openair2/LAYER2/NR_MAC_COMMON/nr_mac_common.h"
#include "openair1/SIMULATION/TOOLS/sim.h"
#include "common/utils/nr/nr_common.h"
#include "openair2/RRC/NR/nr_rrc_extern.h"
#include "openair2/RRC/LTE/rrc_vars.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "PHY/INIT/nr_phy_init.h"
#include "SIMULATION/RF/rf.h"
#include "common/utils/load_module_shlib.h"
#include "openair1/PHY/NR_REFSIG/sss_nr.h"
#include <executables/softmodem-common.h>
#include "openair1/SCHED_NR_UE/defs.h"
#include "openair1/SIMULATION/NR_PHY/nr_unitary_defs.h"
#include "openair1/SIMULATION/NR_PHY/nr_dummy_functions.c"
#include <executables/nr-uesoftmodem.h>

RAN_CONTEXT_t RC;
double cpuf;
openair0_config_t openair0_cfg[MAX_CARDS];
uint8_t const nr_rv_round_map[4] = {0, 2, 3, 1};
nrUE_params_t nrUE_params = {0};
nrUE_params_t *get_nrUE_params(void)
{
  return &nrUE_params;
}
uint64_t get_softmodem_optmask(void)
{
  return 0;
}
static softmodem_params_t softmodem_params;
softmodem_params_t *get_softmodem_params(void)
{
  return &softmodem_params;
}
void init_downlink_harq_status(NR_DL_UE_HARQ_t *dl_harq)
{
}

bool nr_ue_dlsch_procedures(PHY_VARS_NR_UE *ue,
                            UE_nr_rxtx_proc_t *proc,
                            NR_UE_DLSCH_t dlsch[2],
                            int16_t* llr[2]) {return 0;}

int nr_ue_pdsch_procedures(PHY_VARS_NR_UE *ue,
                           UE_nr_rxtx_proc_t *proc,
                           NR_UE_DLSCH_t dlsch[2],
                           int16_t *llr[2],
                           c16_t rxdataF[][ue->frame_parms.samples_per_slot_wCP]) {return 0;}

int nr_ue_pdcch_procedures(PHY_VARS_NR_UE *ue,
                           UE_nr_rxtx_proc_t *proc,
                           int32_t pdcch_est_size,
                           int32_t pdcch_dl_ch_estimates[][pdcch_est_size],
                           nr_phy_data_t *phy_data,
                           int n_ss,
                           c16_t rxdataF[][ue->frame_parms.samples_per_slot_wCP]) {return 0;}

void nr_fill_dl_indication(nr_downlink_indication_t *dl_ind,
                           fapi_nr_dci_indication_t *dci_ind,
                           fapi_nr_rx_indication_t *rx_ind,
                           UE_nr_rxtx_proc_t *proc,
                           PHY_VARS_NR_UE *ue,
                           void *phy_data) {}
void nr_fill_rx_indication(fapi_nr_rx_indication_t *rx_ind,
                           uint8_t pdu_type,
                           PHY_VARS_NR_UE *ue,
                           NR_UE_DLSCH_t *dlsch0,
                           NR_UE_DLSCH_t *dlsch1,
                           uint16_t n_pdus,
                           UE_nr_rxtx_proc_t *proc,
                           void *typeSpecific,
                           uint8_t *b) {}
//////////////////////////////////////////////////////////////////////////
static void prepare_mib_bits(uint8_t *buf, uint32_t frame_tx, uint32_t slot_tx) {

  NR_MasterInformationBlockSidelink_t *sl_mib;
  asn_enc_rval_t enc_rval;

  void *buffer = (void *)buf;

  sl_mib = CALLOC(1, sizeof(NR_MasterInformationBlockSidelink_t));

  sl_mib->inCoverage_r16 = 0;//TRUE;

  // allocate buffer for 7 bits slotnumber
  sl_mib->slotIndex_r16.size = 1;
  sl_mib->slotIndex_r16.buf = CALLOC(1, sl_mib->slotIndex_r16.size);
  sl_mib->slotIndex_r16.bits_unused = sl_mib->slotIndex_r16.size*8 - 7;
  sl_mib->slotIndex_r16.buf[0] = slot_tx << sl_mib->slotIndex_r16.bits_unused;

  sl_mib->directFrameNumber_r16.size = 2;
  sl_mib->directFrameNumber_r16.buf = CALLOC(1, sl_mib->directFrameNumber_r16.size);
  sl_mib->directFrameNumber_r16.bits_unused = sl_mib->directFrameNumber_r16.size*8 - 10;
  sl_mib->directFrameNumber_r16.buf[0] = frame_tx >> (8 - sl_mib->directFrameNumber_r16.bits_unused);
  sl_mib->directFrameNumber_r16.buf[1] = frame_tx << sl_mib->directFrameNumber_r16.bits_unused;

  enc_rval = uper_encode_to_buffer(&asn_DEF_NR_MasterInformationBlockSidelink,
                                    NULL,
                                    (void *)sl_mib,
                                    buffer,
                                    100);

  AssertFatal (enc_rval.encoded > 0, "ASN1 message encoding failed (%s, %lu)!\n",
                enc_rval.failed_type->name, enc_rval.encoded);

  asn_DEF_NR_MasterInformationBlockSidelink.op->free_struct(&asn_DEF_NR_MasterInformationBlockSidelink, sl_mib, ASFM_FREE_EVERYTHING);

}

static int test_rx_mib(uint8_t *decoded_output, uint16_t frame, uint16_t slot) {

  uint32_t sl_mib = *(uint32_t *)decoded_output;

  uint32_t fn = 0, sl = 0;
  fn = (((sl_mib & 0x0700) >> 1) | ((sl_mib & 0xFE0000) >> 17));
  sl = (((sl_mib  & 0x010000) >> 10) | ((sl_mib & 0xFC000000) >> 26));

  printf("decoded output:%x, TX %d:%d, timing decoded from sl-MIB %d:%d\n",
                        *(uint32_t *)decoded_output, frame, slot, fn, sl);

  if (frame == fn && slot == sl)
    return 0;
  
  return -1;
}

//////////////////////////////////////////////////////////////////////////

static void configure_NR_UE(PHY_VARS_NR_UE *UE, int mu, int N_RB) {

  fapi_nr_config_request_t config;
  NR_DL_FRAME_PARMS *fp = &UE->frame_parms;

  config.ssb_config.scs_common               = mu;
  config.cell_config.frame_duplex_type = TDD;
  config.carrier_config.dl_grid_size[mu]     = N_RB;
  config.carrier_config.ul_grid_size[mu]     = N_RB;
  config.carrier_config.sl_grid_size[mu]     = N_RB;
  config.carrier_config.dl_frequency = 0;
  config.carrier_config.uplink_frequency = 0;
  fp->N_RB_SL = N_RB;

  int band;
  if (mu == 1) band = 78;
  if (mu == 0) band = 34;
  nr_init_frame_parms_ue(fp, &config, band);
  fp->ofdm_offset_divisor = 8;
  nr_dump_frame_parms(fp);

  if (init_nr_ue_signal(UE, 1) != 0) {
    printf("Error at UE NR initialisation\n");
    exit(-1);
  }
}

static void configure_SL_UE(PHY_VARS_NR_UE *UE, int mu, int N_RB, int ssb_offset, int slss_id) {

  sl_nr_phy_config_request_t *config = &UE->SL_UE_PHY_PARAMS.sl_config;
  NR_DL_FRAME_PARMS *fp = &UE->SL_UE_PHY_PARAMS.sl_frame_params;

  config->sl_bwp_config.sl_scs = mu;
  config->sl_bwp_config.sl_ssb_offset_point_a = ssb_offset;
  config->sl_carrier_config.sl_bandwidth = N_RB;
  config->sl_carrier_config.sl_grid_size = N_RB;
  config->sl_sync_source.rx_slss_id = slss_id;

  sl_init_frame_parameters(UE);
  sl_ue_phy_init(UE);
  LOG_I(PHY, "Dumping Sidelink Frame Parameters\n");
  nr_dump_frame_parms(fp);
}

static int freq_domain_loopback(PHY_VARS_NR_UE *UE_tx, PHY_VARS_NR_UE *UE_rx, int frame, int slot) {

  sl_nr_ue_phy_params_t *sl_ue1 = &UE_tx->SL_UE_PHY_PARAMS;
  sl_nr_ue_phy_params_t *sl_ue2 = &UE_rx->SL_UE_PHY_PARAMS;

  printf("\nPSBCH SIM -F: %d:%d slss id TX UE:%d, RX UE:%d\n",
                                        frame, slot,sl_ue1->psbch_tx.tx_slss_id,
                                        sl_ue2->sl_config.sl_sync_source.rx_slss_id);
  const int samplesF_per_slot = NR_SYMBOLS_PER_SLOT * sl_ue1->sl_frame_params.ofdm_symbol_size;
  c16_t txdataF_buf[sl_ue1->sl_frame_params.nb_antennas_tx * samplesF_per_slot] __attribute__((aligned(32)));
  memset(txdataF_buf, 0, sizeof(txdataF_buf));
  c16_t *txdataF_ptr[sl_ue1->sl_frame_params.nb_antennas_tx];
  for(int i=0; i< sl_ue1->sl_frame_params.nb_antennas_tx; ++i)
    txdataF_ptr[i] = &txdataF_buf[i * samplesF_per_slot];
  nr_tx_psbch(UE_tx,frame, slot, txdataF_ptr);

  int estimateSz = sl_ue2->sl_frame_params.samples_per_slot_wCP;
  __attribute__ ((aligned(32))) struct complex16 rxdataF[1][estimateSz];
  for (int i=0; i<sl_ue1->sl_frame_params.samples_per_slot_wCP; i++) {
    struct complex16 *rxdataF_ptr = (struct complex16 *)&rxdataF[0][i];
    rxdataF_ptr->r = txdataF_ptr[0]->r;
    rxdataF_ptr->i = txdataF_ptr[0]->i;
    //printf("r,i TXDATAF[%d]-    %d:%d, RXDATAF[%d]-    %d:%d\n", 
    //                                  i, txdataF_ptr->r, txdataF_ptr->i, i, txdataF_ptr->r, txdataF_ptr->i);
  }

  uint8_t err_status = 0;

  UE_nr_rxtx_proc_t proc;
  proc.frame_rx = frame;
  proc.nr_slot_rx = slot;

  struct complex16 dl_ch_estimates[1][estimateSz];
  uint8_t decoded_output[4] = {0};

  LOG_I(PHY,"DEBUG: HIJACKING DL CHANNEL ESTIMATES.\n");
  for (int s=0; s<14; s++) {
    for (int j=0; j<sl_ue2->sl_frame_params.ofdm_symbol_size; j++) {
      struct complex16 *dlch = (struct complex16 *)(&dl_ch_estimates[0][s*sl_ue2->sl_frame_params.ofdm_symbol_size]);
      dlch[j].r = 128;
      dlch[j].i = 0;
    }
  }

  err_status = nr_rx_psbch(UE_rx,
                           &proc,
                           estimateSz,
                           dl_ch_estimates,
                           &sl_ue2->sl_frame_params,
                           decoded_output,
                          rxdataF,
                          sl_ue2->sl_config.sl_sync_source.rx_slss_id);


  int error_payload = 0;
  error_payload = test_rx_mib(decoded_output, frame, slot);

  if (err_status == 0 || error_payload == 0) {
    LOG_I(PHY,"---------PSBCH -F TEST OK.\n");
    return 0;
  }
  LOG_E(PHY, "--------PSBCH -F TEST NOK. FAIL.\n");
  return -1;
}


PHY_VARS_NR_UE *UE_TX; // for tx
PHY_VARS_NR_UE *UE_RX; // for rx

int main(int argc, char **argv) {

  char c;
  int test_freqdomain_loopback = 0;
  int frame = 5, slot = 10, frame_tx = 0, slot_tx = 0;
  int loglvl = OAILOG_INFO;
  uint16_t slss_id = 336, ssb_offset = 0;
  double snr1 = 2.0, snr0 = 2.0, SNR;
  double sigma2 = 0.0, sigma2_dB = 0.0;
  double cfo=0, ip =0.0;

  SCM_t channel_model=AWGN;//Rayleigh1_anticorr;
  int N_RB_DL=106,mu=1;

  uint16_t errors = 0, n_trials = 1;

  int frame_length_complex_samples;
  //int frame_length_complex_samples_no_prefix;
  NR_DL_FRAME_PARMS *frame_parms;

  int seed = 0;

  if ( load_configmodule(argc,argv,CONFIG_ENABLECMDLINEONLY) == 0 ) {
    exit_fun("SIDELINK PSBCH SIM Error, configuration module init failed\n");
  }

  randominit(0);

  while ((c = getopt(argc, argv, "c:hn:o:s:FL:N:R:S:T:")) != -1) {
  
    printf("SIDELINK PSBCH SIM: handling optarg %c\n",c);
    switch (c) {

      case 'c':
        cfo = atof(optarg);
        printf("Setting CFO to %f Hz\n",cfo);
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

      case 'n':
        n_trials = atoi(optarg);
        break;

      case 'o':
        ssb_offset = atoi(optarg);
        printf("SIDELINK PSBCH SIM: ssb offset from pointA:%d\n",ssb_offset);
        break;

      case 's':
        slss_id = atoi(optarg);
        printf("SIDELINK PSBCH SIM: slss_id from arg:%d\n",slss_id);
          AssertFatal(slss_id >= 0 && slss_id <= 671,"SLSS ID not within Range 0-671\n");
          break;

      case 'F':
        test_freqdomain_loopback = 1;
        break;

      case 'L':
        loglvl = atoi(optarg);
        break;

      case 'N':
        snr0 = atoi(optarg);
        snr1 = snr0;
        printf("Setting SNR0 to %f. Test uses this SNR as target SNR\n",snr0);
        break;

      case 'R':
        N_RB_DL = atoi(optarg);
        printf("SIDELINK PSBCH SIM: N_RB_DL:%d\n",N_RB_DL);
        break;

      case 'S':
        snr1 = atof(optarg);
        printf("Setting SNR1 to %f. Test will run until this SNR as target SNR\n",snr1);
        AssertFatal(snr1 <= snr0, "Test runs SNR down, set snr1 to a lower value than %f\n", snr0);
        break;

      case 'T':
        frame = atoi(argv[2]);
        slot = atoi(argv[3]);
        break;

      case 'h':
      default :
        printf("\n\nSIDELINK PSBCH SIM OPTIONS LIST - hus:FL:T:\n");
        printf("-h: HELP\n");
        printf("-c Carrier frequency offset in Hz\n");
        printf("-n Number of trials\n");
        printf("-o ssb offset from PointA - indicates ssb_start subcarrier\n");
        printf("-s: set Sidelink sync id slss_id. ex -s 100\n");
        printf("-F: Run PSBCH frequency domain loopback test of the samples\n");
        printf("-L: Set Log Level.\n");
        printf("-N: Test with Noise. target SNR0 eg -N 10\n");
        printf("-R N_RB_DL\n");
        printf("-S Ending SNR, runs from SNR0 to SNR1\n");
        printf("-T: Frame,Slot to be sent in sl-MIB eg -T 4 2\n");
        return 1;

    }
  }

  randominit(seed);

  logInit();
  set_glog(loglvl);
  T_stdout = 1;

  double fs=0, eps;
  double scs = 30000;
  double bw = 100e6;

  switch (mu) {
    case 1:
      scs = 30000;
      if (N_RB_DL == 217) {
        fs = 122.88e6;
        bw = 80e6;
      }
      else if (N_RB_DL == 245) {
        fs = 122.88e6;
        bw = 90e6;
      }
      else if (N_RB_DL == 273) {
        fs = 122.88e6;
        bw = 100e6;
      }
      else if (N_RB_DL == 106) {
        fs = 61.44e6;
        bw = 40e6;
      }
      else AssertFatal(1==0,"Unsupported numerology for mu %d, N_RB %d\n",mu, N_RB_DL);
      break;
    case 3:
      scs = 120000;
      if (N_RB_DL == 66) {
        fs = 122.88e6;
        bw = 100e6;
      }
      else AssertFatal(1==0,"Unsupported numerology for mu %d, N_RB %d\n",mu, N_RB_DL);
      break;
  }

  // cfo with respect to sub-carrier spacing
  eps = cfo/scs;

  // computation of integer and fractional FO to compare with estimation results
  int IFO;
  if(eps!=0.0){
	  printf("Introducing a CFO of %lf relative to SCS of %d kHz\n",eps,(int)(scs/1000));
	  if (eps>0)
      IFO=(int)(eps+0.5);
    else
      IFO=(int)(eps-0.5);
    printf("FFO = %lf; IFO = %d\n",eps-IFO,IFO);
  }

  channel_desc_t *UE2UE;
  int n_tx = 1, n_rx = 1;
  UE2UE = new_channel_desc_scm(n_tx,
                                n_rx,
                                channel_model,
                                fs,
                                0,
                                bw,
                                300e-9,
                                0.0,
                                CORR_LEVEL_LOW,
                                0,
                                0,
                                0,
                                0);

  if (UE2UE==NULL) {
	  printf("Problem generating channel model. Exiting.\n");
    exit(-1);
  }

  /*****configure UE *************************/
  get_softmodem_params()->sl_mode = 2;
  UE_TX = calloc(1, sizeof(PHY_VARS_NR_UE));
  UE_RX = calloc(1, sizeof(PHY_VARS_NR_UE));
  LOG_I(PHY, "Configure UE-TX and sidelink UE-TX.\n");
  configure_NR_UE(UE_TX, mu, N_RB_DL);
  configure_SL_UE(UE_TX, mu, N_RB_DL,ssb_offset, 0xFFFF);

  LOG_I(PHY, "Configure UE-RX and sidelink UE-RX.\n");
  configure_NR_UE(UE_RX, mu, N_RB_DL);
  configure_SL_UE(UE_RX, mu, N_RB_DL,ssb_offset, slss_id);
  /*****************************************/
  sl_nr_ue_phy_params_t *sl_uetx = &UE_TX->SL_UE_PHY_PARAMS;
  sl_nr_ue_phy_params_t *sl_uerx = &UE_RX->SL_UE_PHY_PARAMS;
  frame_parms = &UE_TX->SL_UE_PHY_PARAMS.sl_frame_params;
  frame_tx = frame % 1024;
  slot_tx = slot % frame_parms->slots_per_frame;

  frame_length_complex_samples = frame_parms->samples_per_subframe*NR_NUMBER_OF_SUBFRAMES_PER_FRAME;
  //frame_length_complex_samples_no_prefix = frame_parms->samples_per_subframe_wCP;

  double **s_re,**s_im,**r_re,**r_im;
  s_re = malloc(2*sizeof(double*));
  s_im = malloc(2*sizeof(double*));
  r_re = malloc(2*sizeof(double*));
  r_im = malloc(2*sizeof(double*));

  s_re[0] = malloc16_clear(frame_length_complex_samples*sizeof(double));
  s_im[0] = malloc16_clear(frame_length_complex_samples*sizeof(double));
  r_re[0] = malloc16_clear(frame_length_complex_samples*sizeof(double));
  r_im[0] = malloc16_clear(frame_length_complex_samples*sizeof(double));

  if(eps!=0.0)
    UE_RX->UE_fo_compensation = 1; // if a frequency offset is set then perform fo estimation and compensation

  UE_nr_rxtx_proc_t proc;
  proc.frame_tx = frame;
  proc.nr_slot_tx = slot;
  nr_phy_data_tx_t phy_data_tx;
  sl_uetx->psbch_tx.tx_slss_id = slss_id;
  sl_uetx->psbch_tx.psbch_payload = 0xFF00000;

  uint8_t sl_mib[4] = {0};
  prepare_mib_bits(sl_mib,frame, slot);
  sl_uetx->psbch_tx.psbch_payload = *(uint32_t *)sl_mib;

  phy_data_tx.sl_tx_action = SL_NR_CONFIG_TYPE_TX_PSBCH;
  proc.frame_rx = frame;
  proc.nr_slot_rx = slot;
  nr_phy_data_t phy_data_rx;
  phy_data_rx.sl_rx_action = SL_NR_CONFIG_TYPE_RX_PSBCH;

  if (test_freqdomain_loopback) {
    errors += freq_domain_loopback(UE_TX, UE_RX, frame_tx, slot_tx);
  }

  printf("\nSidelink TX UE - Frame.Slot %d.%d SLSS id:%d\n",
                            frame, slot,sl_uetx->psbch_tx.tx_slss_id);
  printf("Sidelink RX UE - Frame.Slot %d.%d SLSS id:%d\n",
                                      proc.frame_rx, proc.nr_slot_rx,
                          sl_uerx->sl_config.sl_sync_source.rx_slss_id);

  phy_procedures_nrUE_SL_TX(UE_TX, &proc, &phy_data_tx);

  for (SNR=snr0; SNR>=snr1; SNR-=1) {

    for (int trial=0; trial<n_trials; trial++) {

      for (int i=0; i<frame_length_complex_samples; i++) {
        for (int aa=0; aa<frame_parms->nb_antennas_tx; aa++) {
          struct complex16 *txdata_ptr = (struct complex16 *)&UE_TX->common_vars.txData[aa][i];
          r_re[aa][i] = (double)txdata_ptr->r;
          r_im[aa][i] = (double)txdata_ptr->i;
        }
      }

      //AWGN
      sigma2_dB = 20*log10((double)AMP/4)-SNR;
      sigma2 = pow(10,sigma2_dB/10);
      //printf("sigma2 %f (%f dB), tx_lev %f (%f dB)\n",sigma2,sigma2_dB,txlev,10*log10((double)txlev));

      if(eps!=0.0) {
        rf_rx(r_re,  // real part of txdata
           r_im,  // imag part of txdata
           NULL,  // interference real part
           NULL, // interference imag part
           0,  // interference power
           frame_parms->nb_antennas_rx,  // number of rx antennas
           frame_length_complex_samples,  // number of samples in frame
           1.0e9/fs,   //sampling time (ns)
           cfo,	// frequency offset in Hz
           0.0, // drift (not implemented)
           0.0, // noise figure (not implemented)
           0.0, // rx gain in dB ?
           200, // 3rd order non-linearity in dB ?
           &ip, // initial phase
           30.0e3,  // phase noise cutoff in kHz
           -500.0, // phase noise amplitude in dBc
           0.0,  // IQ imbalance (dB),
	         0.0); // IQ phase imbalance (rad)
      }

        for (int i=0; i<frame_length_complex_samples; i++) {
          for (int aa=0; aa<frame_parms->nb_antennas_rx; aa++) {
            UE_RX->common_vars.rxdata[aa][i].r = (short)(r_re[aa][i] + sqrt(sigma2 / 2) * gaussdouble(0.0, 1.0));
            UE_RX->common_vars.rxdata[aa][i].i = (short)(r_im[aa][i] + sqrt(sigma2 / 2) * gaussdouble(0.0, 1.0));
          }
        }

      nr_sl_initial_sync(&proc, UE_RX, 2);

    } //noise trials

    printf("Runs:%d SNR %f: crc ERRORs = %d, OK = %d\n",
                            n_trials, SNR,sl_uerx->psbch_rx.rx_errors, sl_uerx->psbch_rx.rx_ok);
    errors += sl_uerx->psbch_rx.rx_errors;
    sl_uerx->psbch_rx.rx_errors = 0;
    sl_uerx->psbch_rx.rx_ok = 0;

  } // NSR

  if (errors == 0)
    printf("PSBCH test OK\n");
  else
    printf("PSBCH test NOT OK\n");

  free_channel_desc_scm(UE2UE);

  free(s_re[0]);
  free(s_im[0]);
  free(r_re[0]);
  free(r_im[0]);
  free(s_re);
  free(s_im);
  free(r_re);
  free(r_im);

  term_nr_ue_signal(UE_TX, 1);
  term_nr_ue_signal(UE_RX, 1);

  free(UE_TX);
  free(UE_RX);
  logTerm();
  loader_reset();

  return errors;
}


