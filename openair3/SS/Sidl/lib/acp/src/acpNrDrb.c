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

#include "acpNrDrb.h"
#include "acpCtx.h"
#include "acpProto.h"
#include "acpMsgIds.h"
#include "serNrDrb.h"

void acpNrDrbProcessFromSSInitClt(acpCtx_t _ctx, struct NR_DRB_COMMON_REQ** FromSS)
{
	if (!acpCtxIsValid(_ctx)) {
		SIDL_ASSERT(_ctx != _ctx);
	}
	serNrDrbProcessFromSSInitClt(ACP_CTX_CAST(_ctx)->arena, ACP_CTX_CAST(_ctx)->aSize, FromSS);
}

int acpNrDrbProcessFromSSEncClt(acpCtx_t _ctx, unsigned char* _buffer, size_t* _size, const struct NR_DRB_COMMON_REQ* FromSS)
{
	if (!acpCtxIsValid(_ctx)) {
		return -ACP_ERR_INVALID_CTX;
	}
	size_t _lidx = ACP_HEADER_SIZE;
	int _ret = serNrDrbProcessFromSSEncClt(_buffer, *_size, &_lidx, FromSS);
	if (_ret == SIDL_STATUS_OK) {
		acpBuildHeader(_ctx, ACP_LID_NrDrbProcessFromSS, _lidx, _buffer);
	}
	*_size = _lidx;
	return _ret;
}

int acpNrDrbProcessFromSSDecSrv(acpCtx_t _ctx, const unsigned char* _buffer, size_t _size, struct NR_DRB_COMMON_REQ** FromSS)
{
	if (!acpCtxIsValid(_ctx)) {
		return -ACP_ERR_INVALID_CTX;
	}
	return serNrDrbProcessFromSSDecSrv(_buffer + ACP_HEADER_SIZE, _size - ACP_HEADER_SIZE, ACP_CTX_CAST(_ctx)->arena, ACP_CTX_CAST(_ctx)->aSize, FromSS);
}

void acpNrDrbProcessFromSSFree0Srv(struct NR_DRB_COMMON_REQ* FromSS)
{
	serNrDrbProcessFromSSFree0Srv(FromSS);
}

void acpNrDrbProcessFromSSFreeSrv(struct NR_DRB_COMMON_REQ* FromSS)
{
	serNrDrbProcessFromSSFreeSrv(FromSS);
}

void acpNrDrbProcessFromSSFree0CltSrv(struct NR_DRB_COMMON_REQ* FromSS)
{
	serNrDrbProcessFromSSFree0Srv(FromSS);
}

void acpNrDrbProcessFromSSFreeCltSrv(struct NR_DRB_COMMON_REQ* FromSS)
{
	serNrDrbProcessFromSSFreeSrv(FromSS);
}

void acpNrDrbProcessToSSInitSrv(acpCtx_t _ctx, struct NR_DRB_COMMON_IND** ToSS)
{
	if (!acpCtxIsValid(_ctx)) {
		SIDL_ASSERT(_ctx != _ctx);
	}
	serNrDrbProcessToSSInitSrv(ACP_CTX_CAST(_ctx)->arena, ACP_CTX_CAST(_ctx)->aSize, ToSS);
}

int acpNrDrbProcessToSSEncSrv(acpCtx_t _ctx, unsigned char* _buffer, size_t* _size, const struct NR_DRB_COMMON_IND* ToSS)
{
	if (!acpCtxIsValid(_ctx)) {
		return -ACP_ERR_INVALID_CTX;
	}
	size_t _lidx = ACP_HEADER_SIZE;
	int _ret = serNrDrbProcessToSSEncSrv(_buffer, *_size, &_lidx, ToSS);
	if (_ret == SIDL_STATUS_OK) {
		acpBuildHeader(_ctx, ACP_LID_NrDrbProcessToSS, _lidx, _buffer);
	}
	*_size = _lidx;
	return _ret;
}

int acpNrDrbProcessToSSDecClt(acpCtx_t _ctx, const unsigned char* _buffer, size_t _size, struct NR_DRB_COMMON_IND** ToSS)
{
	if (!acpCtxIsValid(_ctx)) {
		return -ACP_ERR_INVALID_CTX;
	}
	return serNrDrbProcessToSSDecClt(_buffer + ACP_HEADER_SIZE, _size - ACP_HEADER_SIZE, ACP_CTX_CAST(_ctx)->arena, ACP_CTX_CAST(_ctx)->aSize, ToSS);
}

void acpNrDrbProcessToSSFree0Clt(struct NR_DRB_COMMON_IND* ToSS)
{
	serNrDrbProcessToSSFree0Clt(ToSS);
}

void acpNrDrbProcessToSSFreeClt(struct NR_DRB_COMMON_IND* ToSS)
{
	serNrDrbProcessToSSFreeClt(ToSS);
}

void acpNrDrbProcessToSSFree0SrvClt(struct NR_DRB_COMMON_IND* ToSS)
{
	serNrDrbProcessToSSFree0Clt(ToSS);
}

void acpNrDrbProcessToSSFreeSrvClt(struct NR_DRB_COMMON_IND* ToSS)
{
	serNrDrbProcessToSSFreeClt(ToSS);
}