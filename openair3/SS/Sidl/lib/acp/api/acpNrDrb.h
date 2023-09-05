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
#include "acp.h"

SIDL_BEGIN_C_INTERFACE

void acpNrDrbProcessFromSSInitClt(acpCtx_t _ctx, struct NR_DRB_COMMON_REQ** FromSS);

int acpNrDrbProcessFromSSEncClt(acpCtx_t _ctx, unsigned char* _buffer, size_t* _size, const struct NR_DRB_COMMON_REQ* FromSS);

int acpNrDrbProcessFromSSDecSrv(acpCtx_t _ctx, const unsigned char* _buffer, size_t _size, struct NR_DRB_COMMON_REQ** FromSS);

void acpNrDrbProcessFromSSFree0Srv(struct NR_DRB_COMMON_REQ* FromSS);

void acpNrDrbProcessFromSSFreeSrv(struct NR_DRB_COMMON_REQ* FromSS);

void acpNrDrbProcessFromSSFree0CltSrv(struct NR_DRB_COMMON_REQ* FromSS);

void acpNrDrbProcessFromSSFreeCltSrv(struct NR_DRB_COMMON_REQ* FromSS);

void acpNrDrbProcessToSSInitSrv(acpCtx_t _ctx, struct NR_DRB_COMMON_IND** ToSS);

int acpNrDrbProcessToSSEncSrv(acpCtx_t _ctx, unsigned char* _buffer, size_t* _size, const struct NR_DRB_COMMON_IND* ToSS);

int acpNrDrbProcessToSSDecClt(acpCtx_t _ctx, const unsigned char* _buffer, size_t _size, struct NR_DRB_COMMON_IND** ToSS);

void acpNrDrbProcessToSSFree0Clt(struct NR_DRB_COMMON_IND* ToSS);

void acpNrDrbProcessToSSFreeClt(struct NR_DRB_COMMON_IND* ToSS);

void acpNrDrbProcessToSSFree0SrvClt(struct NR_DRB_COMMON_IND* ToSS);

void acpNrDrbProcessToSSFreeSrvClt(struct NR_DRB_COMMON_IND* ToSS);

SIDL_END_C_INTERFACE
