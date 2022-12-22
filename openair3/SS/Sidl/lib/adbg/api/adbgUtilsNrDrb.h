/*
 * Copyright 2022 Sequans Communications.
 *
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.0  (the "License"); you may not use this file
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
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

#pragma once

#include "SIDL_NR_DRB_PORT.h"

SIDL_BEGIN_C_INTERFACE

const char* adbgUtilsNrDrbNR_HarqProcessAssignment_TypeToStr(int select);

const char* adbgUtilsNrDrbNR_MAC_ControlElementDL_TypeToStr(int select);

const char* adbgUtilsNrDrbNR_MAC_ControlElementUL_TypeToStr(int select);

const char* adbgUtilsNrDrbNR_MAC_PDU_TypeToStr(int select);

const char* adbgUtilsNrDrbNR_RLC_UMD_PDU_TypeToStr(int select);

const char* adbgUtilsNrDrbNR_RLC_AMD_PDU_TypeToStr(int select);

const char* adbgUtilsNrDrbNR_RLC_AM_StatusPDU_TypeToStr(int select);

const char* adbgUtilsNrDrbNR_RLC_PDU_TypeToStr(int select);

const char* adbgUtilsNrDrbNR_PDCP_PDU_TypeToStr(int select);

const char* adbgUtilsNrDrbSDAP_PDU_TypeToStr(int select);

const char* adbgUtilsNrDrbNR_L2DataList_TypeToStr(int select);

SIDL_END_C_INTERFACE