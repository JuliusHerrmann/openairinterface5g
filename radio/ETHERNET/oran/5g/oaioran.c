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


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common.h"
#include "config.h"
#include "xran_fh_o_du.h"
#include "xran_compression.h"
#include "xran_cp_api.h"
#include "xran_sync_api.h"
#include "oran_isolate.h"
#include "xran_common.h"
#include "common/utils/threadPool/thread-pool.h"
#include "oaioran.h"


#define USE_POLLING 1
// Declare variable useful for the send buffer function
volatile uint8_t first_call_set = 0;
volatile uint8_t first_rx_set = 0;
volatile int first_read_set = 0;

// Variable declaration useful for fill IQ samples from file
#define IQ_PLAYBACK_BUFFER_BYTES (XRAN_NUM_OF_SLOT_IN_TDD_LOOP*N_SYM_PER_SLOT*XRAN_MAX_PRBS*N_SC_PER_PRB*4L)
/*
int rx_tti;
int rx_sym;
volatile uint32_t rx_cb_tti = 0;
volatile uint32_t rx_cb_frame = 0;
volatile uint32_t rx_cb_subframe = 0;
volatile uint32_t rx_cb_slot = 0;
*/

#define GetFrameNum(tti,SFNatSecStart,numSubFramePerSystemFrame, numSlotPerSubFrame)  ((((uint32_t)tti / ((uint32_t)numSubFramePerSystemFrame * (uint32_t)numSlotPerSubFrame)) + SFNatSecStart) & 0x3FF)
#define GetSlotNum(tti, numSlotPerSfn) ((uint32_t)tti % ((uint32_t)numSlotPerSfn))

//#define ORAN_BRONZE 1
#ifdef ORAN_BRONZE
extern struct xran_fh_config  xranConf;
int xran_is_prach_slot(uint32_t subframe_id, uint32_t slot_id);
#else
#include "app_io_fh_xran.h"
int xran_is_prach_slot(uint8_t PortId, uint32_t subframe_id, uint32_t slot_id);
#endif
#include "common/utils/LOG/log.h"

#ifndef USE_POLLING
extern notifiedFIFO_t oran_sync_fifo;
#else
volatile oran_sync_info_t oran_sync_info;
#endif
void oai_xran_fh_rx_callback(void *pCallbackTag, xran_status_t status){
    struct xran_cb_tag *callback_tag = (struct xran_cb_tag *)pCallbackTag;
    uint64_t second;
    uint32_t tti;
    uint32_t frame;
    uint32_t subframe;
    uint32_t slot,slot2;
    uint32_t rx_tti,rx_sym;

    static int32_t last_slot=-1;
    static int32_t last_frame=-1;

    tti = xran_get_slot_idx(
#ifndef ORAN_BRONZE
		    0,
#endif
		    &frame,&subframe,&slot,&second);

    rx_tti = callback_tag->slotiId;
    rx_sym = callback_tag->symbol;
    if (rx_sym == 7) {
      if (first_call_set) {
        if (!first_rx_set) {
          LOG_I(PHY,"first_rx is set\n");
        }
        first_rx_set = 1;
       if (first_read_set == 1) {    
         slot2=slot+(subframe<<1);	     
         if (last_frame>0 && frame>0 && (slot2>0 && last_frame!=frame) || (slot2 ==0 && last_frame!=((1024+frame-1)&1023)))
	      LOG_E(PHY,"Jump in frame counter last_frame %d => %d, slot %d\n",last_frame,frame,slot2);
         if (last_slot == -1 || slot2 != last_slot) {	     
#ifndef USE_POLLING
           notifiedFIFO_elt_t *req=newNotifiedFIFO_elt(sizeof(oran_sync_info_t), 0, &oran_sync_fifo,NULL);
	   oran_sync_info_t *info = (oran_sync_info_t *)NotifiedFifoData(req);
           info->sl = slot2;
           info->f  = frame;
           LOG_D(PHY,"Push %d.%d (slot %d, subframe %d,last_slot %d)\n",frame,info->sl,slot,subframe,last_slot);
#else
           LOG_D(PHY,"Writing %d.%d (slot %d, subframe %d,last_slot %d)\n",frame,slot2,slot,subframe,last_slot);
	   oran_sync_info.tti = tti;
           oran_sync_info.sl = slot2;
	   oran_sync_info.f  = frame;
#endif
#ifndef USE_POLLING
           pushNotifiedFIFO(&oran_sync_fifo, req);
#else
#endif
         }
         else 
           LOG_E(PHY,"Cannot Push %d.%d (slot %d, subframe %d,last_slot %d)\n",frame,slot2,slot,subframe,last_slot);
         last_slot = slot2;
         last_frame = frame;
       } // first_read_set == 1
      } // first_call_set
    } // rx_sym == 7
}
void oai_xran_fh_srs_callback(void *pCallbackTag, xran_status_t status){
    rte_pause();
}
void oai_xran_fh_rx_prach_callback(void *pCallbackTag, xran_status_t status){
    rte_pause();
}


int oai_physide_dl_tti_call_back(void * param)
{
  if (!first_call_set)
    printf("first_call set from phy cb first_call_set=%p\n",&first_call_set);
  first_call_set = 1;
  return 0;
}

int oai_physide_ul_half_slot_call_back(void * param)
{
    rte_pause();
    return 0;
}

int oai_physide_ul_full_slot_call_back(void * param)
{
    rte_pause();
    return 0;
}


int read_prach_data(ru_info_t *ru, int frame, int slot)
{
	struct rte_mbuf *mb;

	/* calculate tti and subframe_id from frame, slot num */
	int tti = 20 * (frame) + (slot);
	uint32_t subframe = XranGetSubFrameNum(tti, 2, 10);
	uint32_t is_prach_slot = xran_is_prach_slot(
#ifndef ORAN_BRONZE
			0,
#endif
			subframe, (slot % 2));
	int sym_idx = 0;

        struct xran_device_ctx *xran_ctx = xran_dev_get_ctx();
	struct xran_prach_cp_config *pPrachCPConfig = &(xran_ctx->PrachCPConfig);

	/* If it is PRACH slot, copy prach IQ from XRAN PRACH buffer to OAI PRACH buffer */
	if(is_prach_slot) {
	  for(sym_idx = 0; sym_idx < pPrachCPConfig->numSymbol; sym_idx++) {
		for (int aa=0;aa<ru->nb_rx;aa++) {
	  	  mb = (struct rte_mbuf *) xran_ctx->sFHPrachRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][0][aa].sBufferList.pBuffers[sym_idx].pCtrl;
		  if(mb) {
			uint16_t *dst, *src;
			int idx = 0;
			  dst = (uint16_t * )((uint8_t *)ru->prach_buf[aa]);// + (sym_idx*576));
			  src = (uint16_t *) ((uint8_t *) xran_ctx->sFHPrachRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][0][aa].sBufferList.pBuffers[sym_idx].pData);

			/* convert Network order to host order */
			  if (sym_idx==0) {
			    for (idx = 0; idx < 576/2; idx++)
			    {
			    	((int16_t*)dst)[idx] = ((int16_t)ntohs(src[idx]))>>4;
			    }
			  }
			  else {
			    for (idx = 0; idx < 576/2; idx++)
			    {
			    	((int16_t*)dst)[idx] += ((int16_t)ntohs(src[idx]))>>4;
			    }
			  }


	          } else {
			  /* TODO: Unlikely this code never gets executed */
			    printf("%s():%d, %d.%d There is no prach ctrl data for symb %d ant %d\n", __func__, __LINE__, frame, slot, sym_idx,aa);
	          }
		}
	  }
        }
	return(0);
}




int xran_fh_rx_read_slot(ru_info_t *ru, int *frame, int *slot){

  void *ptr = NULL;
  int32_t  *pos = NULL;
  int idx = 0;
  static int last_slot = -1;
  first_read_set = 1; 

  long old_rx_counter[XRAN_PORTS_NUM] = {0,0,0,0,0,0,0,0};
  long old_tx_counter[XRAN_PORTS_NUM] = {0,0,0,0,0,0,0,0};
  struct xran_common_counters x_counters[XRAN_PORTS_NUM];

#ifndef USE_POLLING
  // pull next even from oran_sync_fifo
  notifiedFIFO_elt_t *res=pollNotifiedFIFO(&oran_sync_fifo);
  while (res==NULL) {
     res=pollNotifiedFIFO(&oran_sync_fifo);
  }
  oran_sync_info_t *info = (oran_sync_info_t *)NotifiedFifoData(res);

  *slot       = info->sl;
  *frame      = info->f;
  delNotifiedFIFO_elt(res);
#else
  LOG_D(PHY,"In  xran_fh_rx_read_slot, first_rx_set %d\n",first_rx_set); 
  while (first_rx_set ==0) {}

  *slot = oran_sync_info.sl;
  *frame = oran_sync_info.f;
  uint32_t tti_in=oran_sync_info.tti;

  LOG_D(PHY,"oran slot %d, last_slot %d\n",*slot,last_slot);
  int cnt=0;
  //while (*slot == last_slot)  {
  while (tti_in == oran_sync_info.tti)  {
    //*slot = oran_sync_info.sl;
    cnt++;
  }
  LOG_D(PHY,"cnt %d, Reading %d.%d\n",cnt,*frame,*slot);
  last_slot = *slot;
#endif
  //return(0);

  int tti=(*frame*20) + *slot;
  
  read_prach_data(ru, *frame, *slot);

       struct xran_device_ctx *xran_ctx = xran_dev_get_ctx();
#ifdef ORAN_BRONZE       
       int num_eaxc = xranConf.neAxc;
       int num_eaxc_ul = xranConf.neAxcUl;
#else
       int num_eaxc = app_io_xran_fh_config[0].neAxc;
       int num_eaxc_ul = app_io_xran_fh_config[0].neAxcUl;
#endif
       uint32_t xran_max_antenna_nr = RTE_MAX(num_eaxc, num_eaxc_ul);
       
       int slot_offset_rxdata = 3&(*slot);
       uint32_t slot_size = 4*14*4096;
       uint8_t *rx_data = (uint8_t *)ru->rxdataF[0];
       uint8_t *start_ptr = NULL;
       for(uint16_t cc_id=0; cc_id<1/*nSectorNum*/; cc_id++){ // OAI does not support multiple CC yet.
           for(uint8_t ant_id = 0; ant_id < xran_max_antenna_nr && ant_id<ru->nb_rx; ant_id++){
              rx_data = (uint8_t *)ru->rxdataF[ant_id];
              start_ptr = rx_data + (slot_size*slot_offset_rxdata);
              // This loop would better be more inner to avoid confusion and maybe also errors.
              for(int32_t sym_idx = 0; sym_idx < XRAN_NUM_OF_SYMBOL_PER_SLOT; sym_idx++) {

                 uint8_t *pData = xran_ctx->sFrontHaulRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[sym_idx%XRAN_NUM_OF_SYMBOL_PER_SLOT].pData;
                 uint8_t *pPrbMapData = xran_ctx->sFrontHaulRxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers->pData;
                 struct xran_prb_map *pPrbMap = (struct xran_prb_map *)pPrbMapData;
                 ptr = pData;
                 pos = (int32_t *)(start_ptr + (4*sym_idx*4096));

                 uint8_t *u8dptr;
                 struct xran_prb_map *pRbMap = pPrbMap;
                 int32_t sym_id = sym_idx%XRAN_NUM_OF_SYMBOL_PER_SLOT;
                 if(ptr && pos){
                    uint32_t idxElm = 0;
                    u8dptr = (uint8_t*)ptr;
                    int16_t payload_len = 0;

                    uint8_t *src = (uint8_t *)u8dptr;
                    // first half
                    uint8_t  *src1 = (uint8_t *)u8dptr;
                    uint8_t  *dst1 = (uint8_t *)pos;
                    // second half
                    uint8_t  *src2 = (uint8_t *)u8dptr;
                    uint8_t  *dst2 = (uint8_t *)pos;

                    struct xran_prb_elm* p_prbMapElm = &pRbMap->prbMap[idxElm];

                    for (idxElm = 0;  idxElm < pRbMap->nPrbElm; idxElm++) {
                       struct xran_section_desc *p_sec_desc = NULL;
                       p_prbMapElm = &pRbMap->prbMap[idxElm];
#ifdef ORAN_BRONZE 
                       p_sec_desc = p_prbMapElm->p_sec_desc[sym_id];
                       if(pRbMap->nPrbElm==1 && idxElm==0){
                         src = pData;
                       }
                       else if(p_sec_desc->pData==NULL){
                         return -1;
                       }else{
                         src =  p_sec_desc->pData;
                       }

                       src2 = src;

                       if(p_sec_desc == NULL){
                          printf ("p_sec_desc == NULL\n");
                          exit(-1);
                       }
                       // Calculation of the pointer for the section in the buffer.
                       // first half
                       dst1 = (uint8_t *)(pos + p_prbMapElm->nRBStart*N_SC_PER_PRB);
                       // second half
                       dst2 = (uint8_t *)(pos + (p_prbMapElm->nRBStart*N_SC_PER_PRB + 3276/2) + 4096 - 3276);
                       if(p_prbMapElm->compMethod == XRAN_COMPMETHOD_NONE) {
                          payload_len = p_prbMapElm->nRBSize*N_SC_PER_PRB*4L;
                          src1 = src2 + payload_len/2;
                          for (idx = 0; idx < payload_len/(2*sizeof(int16_t)); idx++) {
                            ((int16_t *)dst1)[idx] = ((int16_t)ntohs(((uint16_t *)src1)[idx]))>>2;
                            ((int16_t *)dst2)[idx] = ((int16_t)ntohs(((uint16_t *)src2)[idx]))>>2;
                          }
                       } else if (p_prbMapElm->compMethod == XRAN_COMPMETHOD_BLKFLOAT) {
                          struct xranlib_decompress_request  bfp_decom_req_2;
                          struct xranlib_decompress_response bfp_decom_rsp_2;
                          struct xranlib_decompress_request  bfp_decom_req_1;
                          struct xranlib_decompress_response bfp_decom_rsp_1;


                          payload_len = (3* p_prbMapElm->iqWidth + 1)*p_prbMapElm->nRBSize;

                          memset(&bfp_decom_req_2, 0, sizeof(struct xranlib_decompress_request));
                          memset(&bfp_decom_rsp_2, 0, sizeof(struct xranlib_decompress_response));
                          memset(&bfp_decom_req_1, 0, sizeof(struct xranlib_decompress_request));
                          memset(&bfp_decom_rsp_1, 0, sizeof(struct xranlib_decompress_response));

                          bfp_decom_req_2.data_in    = (int8_t*)src2;
                          bfp_decom_req_2.numRBs     = p_prbMapElm->nRBSize/2;
                          bfp_decom_req_2.len        = payload_len/2;
                          bfp_decom_req_2.compMethod = p_prbMapElm->compMethod;
                          bfp_decom_req_2.iqWidth    = p_prbMapElm->iqWidth;

                          bfp_decom_rsp_2.data_out   = (int16_t*)dst2;
                          bfp_decom_rsp_2.len        = 0;

                          xranlib_decompress_avx512(&bfp_decom_req_2, &bfp_decom_rsp_2);
                          
                          int16_t first_half_len = bfp_decom_rsp_2.len;
                          src1 = src2+(payload_len/2);

                          bfp_decom_req_1.data_in    = (int8_t*)src1;
                          bfp_decom_req_1.numRBs     = p_prbMapElm->nRBSize/2;
                          bfp_decom_req_1.len        = payload_len/2;
                          bfp_decom_req_1.compMethod = p_prbMapElm->compMethod;
                          bfp_decom_req_1.iqWidth    = p_prbMapElm->iqWidth;

                          bfp_decom_rsp_1.data_out   = (int16_t*)dst1;
                          bfp_decom_rsp_1.len        = 0;

                          xranlib_decompress_avx512(&bfp_decom_req_1, &bfp_decom_rsp_1);
                          payload_len = bfp_decom_rsp_1.len+first_half_len;
                       }else {
                          printf ("p_prbMapElm->compMethod == %d is not supported\n",
                                   p_prbMapElm->compMethod);
                          exit(-1);
                       }
#else

                       if(idxElm==0) src =  pData;
			       
                       if(p_prbMapElm->compMethod == XRAN_COMPMETHOD_NONE) {
                          payload_len = p_prbMapElm->UP_nRBSize*N_SC_PER_PRB*4L;
                          src1 = src2 + payload_len/2;
                          for (idx = 0; idx < payload_len/(2*sizeof(int16_t)); idx++) {
                            ((int16_t *)dst1)[idx] = ((int16_t)ntohs(((uint16_t *)src1)[idx]))>>2;
                            ((int16_t *)dst2)[idx] = ((int16_t)ntohs(((uint16_t *)src2)[idx]))>>2;
                          }
			  src += payload_len;

                       } else if (p_prbMapElm->compMethod == XRAN_COMPMETHOD_BLKFLOAT) {
                          struct xranlib_decompress_request  bfp_decom_req_2;
                          struct xranlib_decompress_response bfp_decom_rsp_2;
                          struct xranlib_decompress_request  bfp_decom_req_1;
                          struct xranlib_decompress_response bfp_decom_rsp_1;


                          payload_len = (3* p_prbMapElm->iqWidth + 1)*p_prbMapElm->nRBSize;

                          memset(&bfp_decom_req_2, 0, sizeof(struct xranlib_decompress_request));
                          memset(&bfp_decom_rsp_2, 0, sizeof(struct xranlib_decompress_response));
                          memset(&bfp_decom_req_1, 0, sizeof(struct xranlib_decompress_request));
                          memset(&bfp_decom_rsp_1, 0, sizeof(struct xranlib_decompress_response));

                          bfp_decom_req_2.data_in    = (int8_t*)src2;
                          bfp_decom_req_2.numRBs     = p_prbMapElm->nRBSize/2;
                          bfp_decom_req_2.len        = payload_len/2;
                          bfp_decom_req_2.compMethod = p_prbMapElm->compMethod;
                          bfp_decom_req_2.iqWidth    = p_prbMapElm->iqWidth;

                          bfp_decom_rsp_2.data_out   = (int16_t*)dst2;
                          bfp_decom_rsp_2.len        = 0;

                          xranlib_decompress_avx512(&bfp_decom_req_2, &bfp_decom_rsp_2);
                          
                          int16_t first_half_len = bfp_decom_rsp_2.len;
                          src1 = src2+(payload_len/2);

                          bfp_decom_req_1.data_in    = (int8_t*)src1;
                          bfp_decom_req_1.numRBs     = p_prbMapElm->nRBSize/2;
                          bfp_decom_req_1.len        = payload_len/2;
                          bfp_decom_req_1.compMethod = p_prbMapElm->compMethod;
                          bfp_decom_req_1.iqWidth    = p_prbMapElm->iqWidth;

                          bfp_decom_rsp_1.data_out   = (int16_t*)dst1;
                          bfp_decom_rsp_1.len        = 0;

                          xranlib_decompress_avx512(&bfp_decom_req_1, &bfp_decom_rsp_1);
                          payload_len = bfp_decom_rsp_1.len+first_half_len;
                       }else {
                          printf ("p_prbMapElm->compMethod == %d is not supported\n",
                                   p_prbMapElm->compMethod);
                          exit(-1);
                       }
		       src += payload_len;
#endif
                   }

                } else {
                     exit(-1);
                     printf("ptr ==NULL\n");
                }
              }
            }
          }

        if ((*frame&0x7f)==0 && *slot == 0 && xran_get_common_counters(app_io_xran_handle, &x_counters[0]) == XRAN_STATUS_SUCCESS) {
            for (int o_xu_id = 0; o_xu_id <  1 /*p_usecaseConfiguration->oXuNum*/;  o_xu_id++) {
                printf("[%s%d][rx %7ld pps %7ld kbps %7ld][tx %7ld pps %7ld kbps %7ld] [on_time %ld early %ld late %ld corrupt %ld pkt_dupl %ld Invalid_Ext1_packets %ld Total %ld]\n",
                    "o-du ",
                    o_xu_id,
                    x_counters[o_xu_id].rx_counter,
                    x_counters[o_xu_id].rx_counter-old_rx_counter[o_xu_id],
                    x_counters[o_xu_id].rx_bytes_per_sec*8/1000L,
                    x_counters[o_xu_id].tx_counter,
                    x_counters[o_xu_id].tx_counter-old_tx_counter[o_xu_id],
                    x_counters[o_xu_id].tx_bytes_per_sec*8/1000L,
                    x_counters[o_xu_id].Rx_on_time,
                    x_counters[o_xu_id].Rx_early,
                    x_counters[o_xu_id].Rx_late,
                    x_counters[o_xu_id].Rx_corrupt,
                    x_counters[o_xu_id].Rx_pkt_dupl,
                    x_counters[o_xu_id].rx_invalid_ext1_packets,
                    x_counters[o_xu_id].Total_msgs_rcvd);

                if (x_counters[o_xu_id].rx_counter > old_rx_counter[o_xu_id])
                    old_rx_counter[o_xu_id] = x_counters[o_xu_id].rx_counter;
                if (x_counters[o_xu_id].tx_counter > old_tx_counter[o_xu_id])
                    old_tx_counter[o_xu_id] = x_counters[o_xu_id].tx_counter;
	    }
	}
return(0);                                   
}

int xran_fh_tx_send_slot(ru_info_t *ru, int frame, int slot, uint64_t timestamp){


  int tti = /*frame*SUBFRAMES_PER_SYSTEMFRAME*SLOTNUM_PER_SUBFRAME+*/20*frame+slot; //commented out temporarily to check that compilation of oran 5g is working.

  void *ptr = NULL;
  int32_t  *pos = NULL;
  int idx = 0;


       struct xran_device_ctx *xran_ctx = xran_dev_get_ctx();
#ifdef ORAN_BRONZE
       int num_eaxc = xranConf.neAxc;
       int num_eaxc_ul = xranConf.neAxcUl;
#else
       int num_eaxc = app_io_xran_fh_config[0].neAxc;
       int num_eaxc_ul = app_io_xran_fh_config[0].neAxcUl;
#endif       
       uint32_t xran_max_antenna_nr = RTE_MAX(num_eaxc, num_eaxc_ul);
       /*
       for (nSectorNum = 0; nSectorNum < XRAN_MAX_SECTOR_NR; nSectorNum++)
       {
           nSectorIndex[nSectorNum] = nSectorNum;
       }
       */

       for(uint16_t cc_id=0; cc_id<1/*nSectorNum*/; cc_id++){ // OAI does not support multiple CC yet.
           for(uint8_t ant_id = 0; ant_id < xran_max_antenna_nr && ant_id<ru->nb_tx; ant_id++){
              // This loop would better be more inner to avoid confusion and maybe also errors.
              for(int32_t sym_idx = 0; sym_idx < XRAN_NUM_OF_SYMBOL_PER_SLOT; sym_idx++) {

                 uint8_t *pData = xran_ctx->sFrontHaulTxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[sym_idx%XRAN_NUM_OF_SYMBOL_PER_SLOT].pData;
                 uint8_t *pPrbMapData = xran_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers->pData;
                 struct xran_prb_map *pPrbMap = (struct xran_prb_map *)pPrbMapData;
                 ptr = pData;
                 pos = &ru->txdataF_BF[ant_id][sym_idx * 4096 /*fp->ofdm_symbol_size*/]; // We had to use a different ru structure than benetel so the access to the buffer is not the same.

                 uint8_t *u8dptr;
                 struct xran_prb_map *pRbMap = pPrbMap;
                 int32_t sym_id = sym_idx%XRAN_NUM_OF_SYMBOL_PER_SLOT;
                 if(ptr && pos){
                    uint32_t idxElm = 0;
                    u8dptr = (uint8_t*)ptr;
                    int16_t payload_len = 0;

                    uint8_t *dst = (uint8_t *)u8dptr;
                    // first half
                    uint8_t  *dst1 = (uint8_t *)u8dptr;
                    uint8_t  *src1 = (uint8_t *)pos;
                    // second half
                    uint8_t  *dst2 = (uint8_t *)u8dptr;
                    uint8_t  *src2 = (uint8_t *)pos;

                    struct xran_prb_elm* p_prbMapElm = &pRbMap->prbMap[idxElm];

                    for (idxElm = 0;  idxElm < pRbMap->nPrbElm; idxElm++) {
                       struct xran_section_desc *p_sec_desc = NULL;
                       p_prbMapElm = &pRbMap->prbMap[idxElm];
                       p_sec_desc = 
#ifdef ORAN_BRONZE 
			       p_prbMapElm->p_sec_desc[sym_id];
#else
		       //assumes on section descriptor per symbol
			       &p_prbMapElm->sec_desc[sym_id][0];
#endif

                       payload_len = p_prbMapElm->nRBSize*N_SC_PER_PRB*4L;
                       dst =  xran_add_hdr_offset(dst, p_prbMapElm->compMethod);
                       dst2 = dst;
                       dst1 = dst2 + payload_len/2;

                       if(p_sec_desc == NULL){
                          printf ("p_sec_desc == NULL\n");
                          exit(-1);
                       }
                       // Calculation of the pointer for the section in the buffer.
                       // first half
                       src1 = (uint8_t *)(pos + p_prbMapElm->nRBStart*N_SC_PER_PRB);
                       // second half
                       src2 = (uint8_t *)(pos + (p_prbMapElm->nRBStart*N_SC_PER_PRB + 3276/2) + 4096 - 3276);
                       if(p_prbMapElm->compMethod == XRAN_COMPMETHOD_NONE) {
                         /* convert to Network order */
                         for (idx = 0; idx < payload_len/(2*sizeof(uint16_t)); idx++)
                         {
                           ((uint16_t *)dst1)[idx] = htons(((uint16_t *)src1)[idx]);
                           ((uint16_t *)dst2)[idx] = htons(((uint16_t *)src2)[idx]);
                         }
                       } else if (p_prbMapElm->compMethod == XRAN_COMPMETHOD_BLKFLOAT) {
                          printf("idxElm=%d, compMeth==BLKFLOAT\n",idxElm);
                          struct xranlib_compress_request  bfp_com_req;
                          struct xranlib_compress_response bfp_com_rsp;

                          memset(&bfp_com_req, 0, sizeof(struct xranlib_compress_request));
                          memset(&bfp_com_rsp, 0, sizeof(struct xranlib_compress_response));

                          bfp_com_req.data_in    = (int16_t*)src2;
                          bfp_com_req.numRBs     = p_prbMapElm->nRBSize/2;
                          bfp_com_req.len        = payload_len/2;
                          bfp_com_req.compMethod = p_prbMapElm->compMethod;
                          bfp_com_req.iqWidth    = p_prbMapElm->iqWidth;

                          bfp_com_rsp.data_out   = (int8_t*)dst2;
                          bfp_com_rsp.len        = 0;

                          xranlib_compress_avx512(&bfp_com_req, &bfp_com_rsp);
                          
                          int16_t first_half_len = bfp_com_rsp.len;

                          dst1 = dst2 + first_half_len;

                          bfp_com_req.data_in    = (int16_t*)src1;
                          bfp_com_req.numRBs     = p_prbMapElm->nRBSize/2;
                          bfp_com_req.len        = payload_len/2;
                          bfp_com_req.compMethod = p_prbMapElm->compMethod;
                          bfp_com_req.iqWidth    = p_prbMapElm->iqWidth;

                          bfp_com_rsp.data_out   = (int8_t*)dst1;
                          bfp_com_rsp.len        = 0;

                          xranlib_compress_avx512(&bfp_com_req, &bfp_com_rsp);
                          payload_len = bfp_com_rsp.len+first_half_len;
                       }else {
                          printf ("p_prbMapElm->compMethod == %d is not supported\n",
                                   p_prbMapElm->compMethod);
                          exit(-1);
                       }

                       p_sec_desc->iq_buffer_offset = RTE_PTR_DIFF(dst, u8dptr);
                       p_sec_desc->iq_buffer_len = payload_len;
                       
                       dst += payload_len;
                       dst  = xran_add_hdr_offset(dst, p_prbMapElm->compMethod);
                   }

                   // The tti should be updated as it increased.
                   pRbMap->tti_id = tti;

                } else {
                     exit(-1);
                     printf("ptr ==NULL\n");
                }
              }
            }
          }
return(0);                                   

}



#if 0
int64_t count_sec =0;
struct xran_common_counters x_counters;
uint64_t nTotalTime;
uint64_t nUsedTime;
uint32_t nCoreUsed;
float nUsedPercent;
uint64_t old_rx_counter = 0;
uint64_t old_tx_counter = 0;

int compute_xran_statistics(void *xranlib_){
  xranLibWraper *xranlib = ((xranLibWraper *) xranlib_);
           
  if(xran_get_common_counters(xranlib->get_xranhandle(), &x_counters) == XRAN_STATUS_SUCCESS) {
     xran_get_time_stats(&nTotalTime, &nUsedTime, &nCoreUsed, 1);
     nUsedPercent = ((float)nUsedTime * 100.0) / (float)nTotalTime;

     printf("[rx %7ld pps %7ld kbps %7ld][tx %7ld pps %7ld kbps %7ld] [on_time %ld early %ld late %ld corrupt %ld pkt_dupl %ld Total %ld] IO Util: %5.2f %%\n",
                                 x_counters.rx_counter,
                                 x_counters.rx_counter-old_rx_counter,
                                 x_counters.rx_bytes_per_sec*8/1000L,
                                 x_counters.tx_counter,
                                 x_counters.tx_counter-old_tx_counter,
                                 x_counters.tx_bytes_per_sec*8/1000L,
                                 x_counters.Rx_on_time,
                                 x_counters.Rx_early,
                                 x_counters.Rx_late,
                                 x_counters.Rx_corrupt,
                                 x_counters.Rx_pkt_dupl,
                                 x_counters.Total_msgs_rcvd,
                                 nUsedPercent);

      if(x_counters.rx_counter > old_rx_counter)
         old_rx_counter = x_counters.rx_counter;
      if(x_counters.tx_counter > old_tx_counter)
         old_tx_counter = x_counters.tx_counter;
  } else {
      printf("error xran_get_common_counters\n");
      return(1);
  }

  return (0);

}
#endif


void check_xran_ptp_sync(){
   int res;
   if ((res=xran_is_synchronized()) != 0)
        printf("Machine is not synchronized using PTP (%x)!\n",res);
   else
        printf("Machine is synchronized using PTP!\n");

}
