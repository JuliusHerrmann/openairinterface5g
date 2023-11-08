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

/* Definitions for LTE Reference signals */
/* Author R. Knopp / EURECOM / OpenAirInterface.org */
#ifndef __NR_REFSIG_DEFS__H__
#define __NR_REFSIG_DEFS__H__

#include "PHY/defs_nr_UE.h"
#include "PHY/LTE_REFSIG/lte_refsig.h"


/*!\brief This function generates the NR Gold sequence (38-211, Sec 5.2.1) for the PBCH DMRS.
@param PHY_VARS_NR_UE* ue structure provides configuration, frame parameters and the pointers to the 32 bits sequence storage tables
 */
int nr_pbch_dmrs_rx(const int dmrss, const unsigned int *nr_gold_pbch, int32_t *output);

/*!\brief This function generates the NR Gold sequence (38-211, Sec 5.2.1) for the PDCCH DMRS.
@param PHY_VARS_NR_UE* ue structure provides configuration, frame parameters and the pointers to the 32 bits sequence storage tables
 */
int nr_pdcch_dmrs_rx(const unsigned int *nr_gold_pdcch,
                     int32_t *output,
                     const unsigned short p,
                     const unsigned short nb_rb_coreset);

int nr_pdsch_dmrs_rx(const PHY_VARS_NR_UE *ue,
                     const unsigned int Ns,
                     const unsigned int *nr_gold_pdsch,
                     const unsigned short p,
                     const unsigned char lp,
                     const unsigned short nb_pdsch_rb,
                     const uint8_t config_type,
                     int32_t *output);

void nr_gold_pbch(PHY_VARS_NR_UE* ue);

void nr_gold_pdcch(PHY_VARS_NR_UE* ue,
                   unsigned short n_idDMRS);

void nr_gold_pdsch(const int nscid, const unsigned int nid, PHY_VARS_NR_UE *ue);

void nr_init_pusch_dmrs(PHY_VARS_NR_UE* ue,
                        uint16_t N_n_scid,
                        uint8_t n_scid);

void nr_init_csi_rs(const NR_DL_FRAME_PARMS *fp, uint32_t ***csi_rs, uint32_t Nid);
void init_nr_gold_prs(PHY_VARS_NR_UE* ue);

#endif
