#include <pthread.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/sctp.h>

#include <arpa/inet.h>

#include "assertions.h"
#include "common/utils/system.h"
#include "queue.h"
#include "sctp_common.h"

#include "intertask_interface.h"
#include "common/ran_context.h"

#include "acpNrSys.h"
#include "gnb_config.h"
#include "ss_gNB_sys_task.h"
#include "ss_gNB_context.h"

#include "common/utils/LOG/ss-log.h"
#define MSC_INTERFACE
#include "msc.h"

extern RAN_CONTEXT_t RC;
extern SSConfigContext_t SS_context;

extern uint16_t ss_rnti_nr_g;
extern SSConfigContext_t SS_context;
extern pthread_cond_t cell_config_5G_done_cond;
extern pthread_mutex_t cell_config_5G_done_mutex;
static int sys_send_udp_msg(uint8_t *buffer, uint32_t buffer_len, uint32_t buffer_offset, uint32_t peerIpAddr, uint16_t peerPort);
char *local_5G_address = "127.0.0.1" ;


typedef enum
{
  UndefinedMsg = 0,
  EnquireTiming = 1,
  CellConfig = 2
} sidl_msg_id;

static bool reqCnfFlag_g = false;
void ss_task_sys_nr_handle_deltaValues(struct NR_SYSTEM_CTRL_REQ *req);
int cell_config_5G_done=-1;
int cell_config_5G_done_indication();
bool ss_task_sys_nr_handle_cellConfig5G (struct NR_CellConfigRequest_Type *p_req,int cell_State);
bool ss_task_sys_nr_handle_cellConfigRadioBearer(struct NR_SYSTEM_CTRL_REQ *req);
bool ss_task_sys_nr_handle_cellConfigAttenuation(struct NR_SYSTEM_CTRL_REQ *req);
static int sys_5G_send_init_udp(const udpSockReq_t *req);
static void sys_5G_send_proxy(void *msg, int msgLen);
int proxy_5G_send_port = 7776;
int proxy_5G_recv_port = 7770;
bool ss_task_sys_nr_handle_pdcpCount(struct NR_SYSTEM_CTRL_REQ *req);

/*
 * Utility function to convert integer to binary
 *
 */
static void int_to_bin(uint32_t in, int count, uint8_t *out)
{
  /* assert: count <= sizeof(int)*CHAR_BIT */
  uint32_t mask = 1U << (count - 1);
  int i;
  for (i = 0; i < count; i++)
  {
    out[i] = (in & mask) ? 1 : 0;
    in <<= 1;
  }
}



/*
 * Function : send_sys_cnf
 * Description: Funtion to build and send the SYS_CNF
 * In :
 * resType - Result type of the requested command
 * resVal  - Result value Success/Fail for the command
 * cnfType - Confirmation type for the Request received
 *           needed by TTCN to map to the Request sent.
 */
static void send_sys_cnf(enum ConfirmationResult_Type_Sel resType,
                         bool resVal,
                         enum NR_SystemConfirm_Type_Sel cnfType,
                         void *msg)
{
  LOG_A(GNB_APP, "[SYS-GNB] Entry in fxn:%s\n", __FUNCTION__);
  struct NR_SYSTEM_CTRL_CNF *msgCnf = CALLOC(1, sizeof(struct NR_SYSTEM_CTRL_CNF));
  MessageDef *message_p = itti_alloc_new_message(TASK_SYS_GNB, INSTANCE_DEFAULT, SS_NR_SYS_PORT_MSG_CNF);

  /* The request has send confirm flag flase so do nothing in this funciton */
  if (reqCnfFlag_g == false)
  {
    LOG_A(GNB_APP, "[SYS-GNB] No confirm required\n");
    return ;
  }

  if (message_p)
  {
    LOG_A(GNB_APP, "[SYS-GNB] Send SS_NR_SYS_PORT_MSG_CNF\n");
    msgCnf->Common.CellId = SS_context.eutra_cellId;
    msgCnf->Common.Result.d = resType;
    msgCnf->Common.Result.v.Success = resVal;
    msgCnf->Confirm.d = cnfType;
    switch (cnfType)
    {
      case NR_SystemConfirm_Type_Cell:
        {
          LOG_A(GNB_APP, "[SYS-GNB] Send confirm for cell configuration\n");
          msgCnf->Confirm.v.Cell = true;
          break;
        }
      case NR_SystemConfirm_Type_RadioBearerList:
        {
          LOG_A(GNB_APP, "[SYS-GNB] Send confirm for RadioBearerList\n");
          msgCnf->Confirm.v.RadioBearerList = true;
          break;
        }
      case NR_SystemConfirm_Type_PdcpCount:
        {
          if (msg)
            memcpy(&msgCnf->Confirm.v.PdcpCount, msg, sizeof(struct NR_PDCP_CountCnf_Type));
          else
            SS_NR_SYS_PORT_MSG_CNF(message_p).cnf = msgCnf;
          break;
        }
      case NR_SystemConfirm_Type_AS_Security:
        {
          LOG_A(GNB_APP, "[SYS-GNB] Send confirm for cell configuration NR_SystemConfirm_Type_AS_Security\n");
          msgCnf->Confirm.v.AS_Security = true;
          break;
        }
      default:
        LOG_A(GNB_APP, "[SYS-GNB] Error not handled CNF TYPE to [SS-PORTMAN-GNB]\n");
    }
    SS_NR_SYS_PORT_MSG_CNF(message_p).cnf = msgCnf;
    int send_res = itti_send_msg_to_task(TASK_SS_PORTMAN_GNB, INSTANCE_DEFAULT, message_p);
    if (send_res < 0)
    {
      LOG_A(GNB_APP, "[SYS-GNB] Error sending to [SS-PORTMAN-GNB]\n");
    }
    else
    {
      LOG_A(GNB_APP, "[SYS-GNB] fxn:%s NR_SYSTEM_CTRL_CNF sent for cnfType:%d to Port Manager\n", __FUNCTION__, cnfType);
    }
  }
  LOG_A(GNB_APP, "[SYS-GNB] Exit from fxn:%s\n", __FUNCTION__);
}
/*
 * Function : sys_handle_nr_enquire_timing
 * Description: Sends the NR enquire timing update to PORTMAN
 */
static void sys_handle_nr_enquire_timing(ss_nrset_timinfo_t *tinfo)
{
  MessageDef *message_p = itti_alloc_new_message(TASK_SYS_GNB, INSTANCE_DEFAULT, SS_NRSET_TIM_INFO);
  if (message_p)
  {
    LOG_A(GNB_APP, "[SYS-GNB] Reporting info sfn:%d\t slot:%d.\n", tinfo->sfn, tinfo->slot);
    SS_NRSET_TIM_INFO(message_p).slot = tinfo->slot;
    SS_NRSET_TIM_INFO(message_p).sfn = tinfo->sfn;

    int send_res = itti_send_msg_to_task(TASK_SS_PORTMAN_GNB, INSTANCE_DEFAULT, message_p);
    if (send_res < 0)
    {
      LOG_A(GNB_APP, "[SYS-GNB] Error sending to [SS-PORTMAN-GNB]");
    }
  }
}

/*
 * Function : sys_nr_cell_attn_update
 * Description: Sends the attenuation updates received from TTCN to proxy
 */
static void sys_nr_cell_attn_update(uint8_t cellId, uint8_t attnVal)
{
  LOG_A(GNB_APP, "In sys_nr_cell_attn_update\n");
  attenuationConfigReq_t *attnConf = NULL;

  attnConf = (attenuationConfigReq_t *) calloc(1, sizeof(attenuationConfigReq_t));
  attnConf->header.preamble = 0xFEEDC0DE;
  attnConf->header.msg_id = SS_ATTN_LIST;
  attnConf->header.cell_id = cellId;
  attnConf->attnVal = attnVal;

  /** Send to proxy */
  sys_5G_send_proxy((void *)attnConf, sizeof(attenuationConfigReq_t));
  LOG_A(GNB_APP, "Out sys_nr_cell_attn_update\n");
  return;
}

/*
 * Function : sys_handle_nr_cell_attn_req
 * Description: Handles the attenuation updates received from TTCN
 */
static void sys_handle_nr_cell_attn_req(struct NR_CellAttenuationConfig_Type_NR_CellAttenuationList_Type_Dynamic *CellAttenuationList)
{
  for(int i=0;i<CellAttenuationList->d;i++) {
    uint8_t cellId = (uint8_t)CellAttenuationList->v[i].CellId;
    uint8_t attnVal = 0; // default set it Off

    switch (CellAttenuationList->v[i].Attenuation.d)
    {
    case Attenuation_Type_Value:
      attnVal = CellAttenuationList->v[i].Attenuation.v.Value;
      LOG_A(GNB_APP, "[SYS-GNB] CellAttenuationList for Cell_id %d value %d dBm received\n",
            cellId, attnVal);
      sys_nr_cell_attn_update(cellId, attnVal);
      break;
    case Attenuation_Type_Off:
      attnVal = 80; /* TODO: attnVal hardcoded currently but Need to handle proper Attenuation_Type_Off */
      LOG_A(GNB_APP, "[SYS-GNB] CellAttenuationList turn off for Cell_id %d received with attnVal : %d\n",
            cellId,attnVal);
      sys_nr_cell_attn_update(cellId, attnVal);
      break;
    case Attenuation_Type_UNBOUND_VALUE:
      LOG_A(GNB_APP, "[SYS-GNB] CellAttenuationList Attenuation_Type_UNBOUND_VALUE received\n");
      break;
    default:
      LOG_A(GNB_APP, "[SYS-GNB] Invalid CellAttenuationList received\n");
    }
  }
}

/* 
 * =========================================================================================================== 
 * Function Name: ss_task_sys_nr_handle_req
 * Parameter    : SYSTEM_CTRL_REQ *req, is the message having ASP Defination of NR_SYSTEM_CTRL_REQ (38.523-3)
 *                which is received on SIDL via TTCN.
 *                ss_set_timinfo_t *tinfo, is currently not used.
 * Description  : This function handles the SYS_PORT_NR configuration command received from TTCN via the PORTMAN.
 *                It applies the configuration on RAN Context for NR and sends the confirmation message to 
 *                PORTMAN.
 * Returns      : Void
 * ==========================================================================================================
*/
static void ss_task_sys_nr_handle_req(struct NR_SYSTEM_CTRL_REQ *req, ss_nrset_timinfo_t *tinfo)
{
  int enterState = RC.ss.State;
  if(req->Common.CellId)
    SS_context.eutra_cellId = req->Common.CellId;
  LOG_A(GNB_APP, "[SYS-GNB] Current SS_STATE %d received SystemRequest_Type %d eutra_cellId %d cnf_flag %d\n",
      RC.ss.State, req->Request.d, SS_context.eutra_cellId, req->Common.ControlInfo.CnfFlag);
  switch (RC.ss.State)
  {
    case SS_STATE_NOT_CONFIGURED:
      if (req->Request.d == NR_SystemRequest_Type_Cell)
      {
        LOG_A(GNB_APP, "[SYS-GNB] NR_SystemRequest_Type_Cell received\n");
        if (false == ss_task_sys_nr_handle_cellConfig5G(&req->Request.v.Cell,RC.ss.State) )
        {
          LOG_A(GNB_APP, "[SYS-GNB] Error handling Cell Config 5G for NR_SystemRequest_Type_Cell \n");
          return;
        }
        cell_config_5G_done_indication();

        if (RC.ss.State == SS_STATE_NOT_CONFIGURED)
        {
          RC.ss.State  = SS_STATE_CELL_ACTIVE;
          SS_context.State = SS_STATE_CELL_ACTIVE;
          LOG_A(GNB_APP, "[SYS-GNB] RC.ss.State changed to ACTIVE \n");
        }
        send_sys_cnf(ConfirmationResult_Type_Success, true, NR_SystemConfirm_Type_Cell, NULL);


        if (req->Request.v.Cell.d == NR_CellConfigRequest_Type_AddOrReconfigure)
        {
          CellConfig5GReq_t	*cellConfig = NULL;
          struct NR_CellConfigInfo_Type *p_cellConfig = NULL;
          p_cellConfig = &req->Request.v.Cell.v.AddOrReconfigure;
          SS_context.maxRefPower = p_cellConfig->CellConfigCommon.v.InitialCellPower.v.MaxReferencePower;
          cellConfig = (CellConfig5GReq_t*)malloc(sizeof(CellConfig5GReq_t));
          if (cellConfig == NULL)
          {
		        AssertFatal(cellConfig != NULL , "[SYS-GNB] Failed to allocate memory for proxy cell config\n");
          }
          cellConfig->header.preamble = 0xFEEDC0DE;
          cellConfig->header.msg_id = SS_CELL_CONFIG;
          cellConfig->header.length = sizeof(proxy_ss_header_t);
          cellConfig->initialAttenuation = 0;
          if (req->Request.v.Cell.v.AddOrReconfigure.PhysicalLayer.v.Common.v.PhysicalCellId.d == true)
          {
            cellConfig->header.cell_id = req->Request.v.Cell.v.AddOrReconfigure.PhysicalLayer.v.Common.v.PhysicalCellId.v;
          }
          cellConfig->maxRefPower= p_cellConfig->CellConfigCommon.v.InitialCellPower.v.MaxReferencePower;
          cellConfig->absoluteFrequencyPointA = p_cellConfig->PhysicalLayer.v.Downlink.v.FrequencyInfoDL.v.v.R15.absoluteFrequencyPointA;
          LOG_A(ENB_SS,"5G Cell configuration received for cell_id: %d Initial attenuation: %d \
              Max ref power: %d\n for absoluteFrequencyPointA : %ld =================================== \n",
              cellConfig->header.cell_id,
              cellConfig->pci,
              cellConfig->initialAttenuation, cellConfig->maxRefPower,
              cellConfig->absoluteFrequencyPointA);
          //send_to_proxy();
          sys_5G_send_proxy((void *)cellConfig, sizeof(CellConfig5GReq_t));
        }

      }
      else if (req->Request.d == NR_SystemRequest_Type_DeltaValues)
      {
        /* TBD: Sending the dummy confirmation for now*/
        ss_task_sys_nr_handle_deltaValues(req);
        LOG_A(GNB_APP, "[SYS-GNB] Sent SYS CNF for NR_SystemRequest_Type_DeltaValues\n");
      }
      else
      {
        LOG_E(GNB_APP, "[SYS-GNB] Error ! SS_STATE %d  Invalid SystemRequest_Type %d received\n",
            RC.ss.State, req->Request.d);
      }
      break;
    case SS_STATE_CELL_ACTIVE:
      {
        switch (req->Request.d)
        {
          case NR_SystemRequest_Type_Cell:
            {
              if (false == ss_task_sys_nr_handle_cellConfig5G(&req->Request.v.Cell,RC.ss.State) )
              {
                LOG_E(GNB_APP, "[SYS-GNB] Error handling Cell Config 5G for NR_SystemRequest_Type_Cell \n");
              }
              send_sys_cnf(ConfirmationResult_Type_Success, true, NR_SystemConfirm_Type_Cell, NULL);
            }
            break;
          case NR_SystemRequest_Type_EnquireTiming:
            {
              sys_handle_nr_enquire_timing(tinfo);
              LOG_A(GNB_APP, "[SYS-GNB] NR_SystemRequest_Type_EnquireTiming received\n");
            }
            break;
          case NR_SystemRequest_Type_RadioBearerList:
            {
              LOG_A(GNB_APP, "[SYS-GNB] NR_SystemRequest_Type_RadioBearerList received\n");
              if (false == ss_task_sys_nr_handle_cellConfigRadioBearer(req) )
              {
                LOG_A(GNB_APP, "[SYS-GNB] Error handling Cell Config 5G for NR_SystemRequest_Type_Cell \n");
                return;
              }
            }
            break;
          case NR_SystemRequest_Type_CellAttenuationList:
            {
              LOG_A(GNB_APP, "[SYS-GNB] NR_SystemRequest_Type_CellAttenuationList received\n");
              sys_handle_nr_cell_attn_req(&(req->Request.v.CellAttenuationList));
              if (false == ss_task_sys_nr_handle_cellConfigAttenuation(req) )
              {
                LOG_A(GNB_APP, "[SYS-GNB] Error handling Cell Config 5G for NR_SystemRequest_Type_Cell \n");
                return;
              }
            }
            break;
          case NR_SystemRequest_Type_PdcpCount:
            {
              LOG_A(GNB_APP, "[SYS-GNB] NR_SystemRequest_Type_PdcpCount received\n");
              if (false == ss_task_sys_nr_handle_pdcpCount(req))
              {
                LOG_A(GNB_APP, "[SYS-GNB] Error handling Cell Config 5G for NR_SystemRequest_Type_PdcpCount \n");
                return;
              }
            }
            break;
          case NR_SystemRequest_Type_AS_Security:
            {
              LOG_A(GNB_APP, "[SYS-GNB] Dummy handling for Cell Config 5G NR_SystemRequest_Type_AS_Security \n");
              send_sys_cnf(ConfirmationResult_Type_Success, true, NR_SystemConfirm_Type_AS_Security, NULL);
            }
            break;
          default:
            {
              LOG_E(GNB_APP, "[SYS-GNB] Error ! SS_STATE %d  Invalid SystemRequest_Type %d received\n",
                  RC.ss.State, req->Request.d);
            }
            break;
        }
      }
      break;
  }
  LOG_A(GNB_APP, "[SYS-GNB] SS_STATE %d New SS_STATE %d received SystemRequest_Type %d\n",
      enterState, RC.ss.State, req->Request.d);
}

/* 
 * =============================================================================================================
 * Function Name: valid_nr_sys_msg
 * Parameter    : SYSTEM_CTRL_REQ *req, is the message having ASP Defination of NR_SYSTEM_CTRL_REQ (38.523-3) 
 *                which is received on SIDL via TTCN.
 * Description  : This function validates the validity of System Control Request Type. On successfull validation,
 *                this function sends the dummy confirmation to PORTMAN which is further forwareded towards TTCN.
 * Returns      : TRUE if recevied command is supported by SYS state handler
 *                FALSE if received command is not supported by SYS handler 
 * ============================================================================================================
*/

bool valid_nr_sys_msg(struct NR_SYSTEM_CTRL_REQ *req)
{
  bool valid = false;
  enum ConfirmationResult_Type_Sel resType = ConfirmationResult_Type_Success;
  bool resVal = true;
  bool sendDummyCnf = true;
  enum NR_SystemConfirm_Type_Sel cnfType = 0;

  LOG_A(GNB_APP, "[SYS-GNB] received req : %d for cell %d RC.ss.State %d \n",
      req->Request.d, req->Common.CellId, RC.ss.State);
  switch (req->Request.d)
  {
    case NR_SystemRequest_Type_Cell:
      if (RC.ss.State >= SS_STATE_NOT_CONFIGURED)
      {
        valid = true;
        sendDummyCnf = false;
        reqCnfFlag_g = req->Common.ControlInfo.CnfFlag;
      }
      else
      {
        cnfType = NR_SystemConfirm_Type_Cell;
        reqCnfFlag_g = req->Common.ControlInfo.CnfFlag;
      }
      break;
    case NR_SystemRequest_Type_EnquireTiming:
      valid = true;
      sendDummyCnf = false;
      reqCnfFlag_g = req->Common.ControlInfo.CnfFlag;
      break;
    case NR_SystemRequest_Type_DeltaValues:
      valid = true;
      sendDummyCnf = false;
      reqCnfFlag_g = req->Common.ControlInfo.CnfFlag;
      break;
    case NR_SystemRequest_Type_RadioBearerList:
      valid = true;
      sendDummyCnf = false;
      cnfType = NR_SystemConfirm_Type_RadioBearerList;
      reqCnfFlag_g = req->Common.ControlInfo.CnfFlag;
      break;
    case NR_SystemRequest_Type_CellAttenuationList:
      valid = true;
      sendDummyCnf = false;
      cnfType = NR_SystemConfirm_Type_CellAttenuationList;
      reqCnfFlag_g = req->Common.ControlInfo.CnfFlag;
      break;
    case NR_SystemRequest_Type_AS_Security:
      valid = true;
      sendDummyCnf = false;
      cnfType = NR_SystemConfirm_Type_AS_Security;
      reqCnfFlag_g = req->Common.ControlInfo.CnfFlag;
      break;
    case NR_SystemRequest_Type_PdcpCount:
      valid = true;
      sendDummyCnf = false;
      cnfType = NR_SystemConfirm_Type_PdcpCount;
      reqCnfFlag_g = req->Common.ControlInfo.CnfFlag;
      break;
    default:
      valid = false;
      sendDummyCnf = false;
  }
  if (sendDummyCnf)
  {
    send_sys_cnf(resType, resVal, cnfType, NULL);
    LOG_A(GNB_APP, "[SYS-GNB] Sending Dummy OK Req %d cnTfype %d ResType %d ResValue %d\n",
        req->Request.d, cnfType, resType, resVal);
  }
  return valid;
}


/* 
 * =========================================================================================================== 
 * Function Name: ss_gNB_sys_process_itti_msg
 * Parameter    : notUsed, is a dummy parameter is not being used currently
 * Description  : This function is entry point function for TASK_SYS_5G_NR. This function process the received 
 *                messages from other module and invokes respective handler function
 * Returns      : Void
 * ==========================================================================================================
*/

void *ss_gNB_sys_process_itti_msg(void *notUsed)
{
	MessageDef *received_msg = NULL;
	int result;
	static ss_nrset_timinfo_t tinfo = {.sfn = 0xFFFF, .slot = 0xFF};

	itti_receive_msg(TASK_SYS_GNB, &received_msg);

	LOG_D(GNB_APP, "Entry in fxn:%s \n", __FUNCTION__);
	/* Check if there is a packet to handle */
	if (received_msg != NULL)
	{
		switch (ITTI_MSG_ID(received_msg))
		{
			case SS_NRUPD_TIM_INFO:
				{
					LOG_D(GNB_APP, "TASK_SYS_GNB received SS_NRUPD_TIM_INFO with sfn=%d slot=%d\n", SS_NRUPD_TIM_INFO(received_msg).sfn, SS_NRUPD_TIM_INFO(received_msg).slot);
					tinfo.slot = SS_NRUPD_TIM_INFO(received_msg).slot;
					tinfo.sfn = SS_NRUPD_TIM_INFO(received_msg).sfn;
				}
				break;

			case SS_NR_SYS_PORT_MSG_IND:
				{

					if (valid_nr_sys_msg(SS_NR_SYS_PORT_MSG_IND(received_msg).req))
					{
						ss_task_sys_nr_handle_req(SS_NR_SYS_PORT_MSG_IND(received_msg).req, &tinfo);
					}
					else
					{
						LOG_A(GNB_APP, "TASK_SYS_GNB: Not handled SYS_PORT message received \n");
					}
				}
				break;
			case UDP_DATA_IND:
				{
					LOG_A(GNB_APP, "[TASK_SYS_GNB] received UDP_DATA_IND \n");
					proxy_ss_header_t hdr;
					memcpy(&hdr, (SS_SYS_PROXY_MSG_CNF(received_msg).buffer), sizeof(proxy_ss_header_t));
					LOG_A(GNB_APP, "[TASK_SYS_GNB] received msgId:%d\n", hdr.msg_id);
					switch (hdr.msg_id)
					{
						case SS_CELL_CONFIG_CNF:	
							LOG_A(GNB_APP, "[TASK_SYS_GNB] received UDP_DATA_IND with Message SS_NR_SYS_PORT_MSG_CNF\n");
							break;

						default:
							LOG_E(GNB_APP, "[TASK_SYS_GNB] received unhandled message \n");
					}
				}
				break;
			case TERMINATE_MESSAGE:
				{
					itti_exit_task();
					break;
				}
			default:
				LOG_A(GNB_APP, "TASK_SYS_GNB: Received unhandled message %d:%s\n",
						ITTI_MSG_ID(received_msg), ITTI_MSG_NAME(received_msg));
				break;
		}
		result = itti_free(ITTI_MSG_ORIGIN_ID(received_msg), received_msg);
		AssertFatal(result == EXIT_SUCCESS, "[SYS] Failed to free memory (%d)!\n", result);
		received_msg = NULL;
	}
	return NULL;
}

/*
 * Function : ss_gNB_sys_task
 * Description:  The SYS_TASK main function handler. Initilizes the UDP
 * socket towards the Proxy for the configuration updates. Initilizes
 * the SYS_TASK state machine Init_State. Invoke the itti message
 * handler for the SYY_PORT.
 */
void *ss_gNB_sys_task(void *arg)
{
  udpSockReq_t req;
  req.address = local_5G_address;
  req.port = proxy_5G_recv_port;
  sys_5G_send_init_udp(&req);
  sleep(5);
  if (RC.ss.configured == 0)
  {
    //RCconfig_nr_ssparam();
    RC.ss.configured = 1;
  }
  // Set the state to NOT_CONFIGURED for Cell Config processing mode
  if (RC.ss.mode == SS_SOFTMODEM)
  {
    RC.ss.State = SS_STATE_NOT_CONFIGURED;
    LOG_A(GNB_APP, "TASK_SYS_GNB: fxn:%s line:%d RC.ss.mode:SS_STATE_NOT_CONFIGURED \n", __FUNCTION__, __LINE__);
  }
  // Set the state to CELL_ACTIVE for SRB processing mode
  else if (RC.ss.mode == SS_SOFTMODEM_SRB)
  {
    RC.ss.State = SS_STATE_CELL_ACTIVE;
    SS_context.State = SS_STATE_CELL_ACTIVE;
    LOG_A(GNB_APP, "TASK_SYS_GNB: fxn:%s line:%d RC.ss.mode:SS_STATE_CELL_ACTIVE \n", __FUNCTION__, __LINE__);
  }

  while (1)
  {
    (void) ss_gNB_sys_process_itti_msg(NULL);
  }

  return NULL;
}

/*
 * Function   : ss_task_sys_nr_handle_deltaValues
 * Description: This function handles the NR_SYSTEM_CTRL_REQ for DeltaValues and updates the CNF structures as 
 *              per cell's band configuration.
 * Returns    : None
 */
void ss_task_sys_nr_handle_deltaValues(struct NR_SYSTEM_CTRL_REQ *req)
{
	LOG_A(GNB_APP, "[SYS-GNB] Entry in fxn:%s\n", __FUNCTION__);
	struct NR_SYSTEM_CTRL_CNF *msgCnf = CALLOC(1, sizeof(struct NR_SYSTEM_CTRL_CNF));
	MessageDef *message_p = itti_alloc_new_message(TASK_SYS_GNB, INSTANCE_DEFAULT, SS_NR_SYS_PORT_MSG_CNF);
	if (!message_p)
	{
		LOG_A(GNB_APP, "[SYS-GNB] Error Allocating Memory for message NR_SYSTEM_CTRL_CNF \n");
		return ;

	}
	msgCnf->Common.CellId = 0;
	msgCnf->Common.RoutingInfo.d = NR_RoutingInfo_Type_None;
	msgCnf->Common.RoutingInfo.v.None = true;
	msgCnf->Common.TimingInfo.d = TimingInfo_Type_None;
	msgCnf->Common.TimingInfo.v.None = true;
	msgCnf->Common.Result.d = ConfirmationResult_Type_Success;
	msgCnf->Common.Result.v.Success = true;

	msgCnf->Confirm.d = NR_SystemConfirm_Type_DeltaValues;
	struct UE_NR_DeltaValues_Type* vals = &msgCnf->Confirm.v.DeltaValues;
	struct DeltaValues_Type* deltaPrimaryBand = &vals->DeltaPrimaryBand;
	struct DeltaValues_Type* deltaSecondaryBand = &vals->DeltaSecondaryBand;

	deltaPrimaryBand->DeltaNRf1 = 0;
	deltaPrimaryBand->DeltaNRf2 = 0;
	deltaPrimaryBand->DeltaNRf3 = 0;
	deltaPrimaryBand->DeltaNRf4 = 0;

	deltaSecondaryBand->DeltaNRf1 = 0;
	deltaSecondaryBand->DeltaNRf2 = 0;
	deltaSecondaryBand->DeltaNRf3 = 0;
	deltaSecondaryBand->DeltaNRf4 = 0;
	msgCnf->Confirm.v.Cell = true;

	SS_NR_SYS_PORT_MSG_CNF(message_p).cnf = msgCnf;
	int send_res = itti_send_msg_to_task(TASK_SS_PORTMAN_GNB, INSTANCE_DEFAULT, message_p);
	if (send_res < 0)
	{
		LOG_A(GNB_APP, "[SYS-GNB] Error sending to [SS-PORTMAN-GNB]");
	}
	LOG_A(GNB_APP, "[SYS-GNB] Exit from fxn:%s\n", __FUNCTION__);

}

/*
 * Function   : ss_task_sys_nr_handle_cellConfig5G
 * Description: This function handles the NR_SYSTEM_CTRL_REQ for request type AddOrReconfigure. TTCN provides the values of for Cell Config Req 5G on SYS Port
 *              and those values are populated here in corresponding structures of NR RRC.
 * Returns    : None
 */

bool ss_task_sys_nr_handle_cellConfig5G(struct NR_CellConfigRequest_Type *p_req,int cell_State)
{
  uint32_t gnbId = 0;
  if (p_req->d == NR_CellConfigRequest_Type_AddOrReconfigure)
  {
    /* populate each config */
    /* 1. StaticResource Config */
    if(p_req->v.AddOrReconfigure.StaticResourceConfig.d)
    {

    }

    /* 2. CellConfigCommon: currently NR_InitialCellPower_Type is processed after cell config  */
    if(p_req->v.AddOrReconfigure.CellConfigCommon.d)
    {

    }

    /* 3.  PhysicalLayer */
    /* TODO: populate fields to
         RC.nrrrc[gnbId]->carrier.servingcellconfigcommon
         RC.nrrrc[gnbId]->carrier.cellConfigDedicated
    */
    if(p_req->v.AddOrReconfigure.PhysicalLayer.d)
    {

    }

    /* 4.  BcchConfig */
    /* TODO: populate all BcchConfig fields to
              RC.nrrrc[gnbId]->carrier.mib
              RC.nrrrc[gnbId]->carrier.siblock1
              RC.nrrrc[gnbId]->carrier.systemInformation
     */
    if(p_req->v.AddOrReconfigure.BcchConfig.d)
    {

    }

    /* 5. PcchConfig */
    if(p_req->v.AddOrReconfigure.PcchConfig.d)
    {

    }

    /* 6. RachProcedureConfig */
    if(p_req->v.AddOrReconfigure.RachProcedureConfig.d)
    {

    }

    /* 7. DcchDtchConfig */
    if(p_req->v.AddOrReconfigure.DcchDtchConfig.d)
    {
      if(NULL == RC.nrrrc[gnbId]->carrier.dcchDtchConfig){
        RC.nrrrc[gnbId]->carrier.dcchDtchConfig = calloc(1,sizeof(NR_DcchDtchConfig_t));
      }
      NR_DcchDtchConfig_t * dcchDtchConfig = RC.nrrrc[gnbId]->carrier.dcchDtchConfig;
      if(p_req->v.AddOrReconfigure.DcchDtchConfig.v.DL.d)
      {

      }

      if(p_req->v.AddOrReconfigure.DcchDtchConfig.v.UL.d)
      {
        if(NULL==dcchDtchConfig->ul){
          dcchDtchConfig->ul = calloc(1,sizeof(*(dcchDtchConfig->ul)));
        }
        if(p_req->v.AddOrReconfigure.DcchDtchConfig.v.UL.v.SearchSpaceAndDci.d){

          if(p_req->v.AddOrReconfigure.DcchDtchConfig.v.UL.v.SearchSpaceAndDci.v.DciInfo.d){
            if(NULL == dcchDtchConfig->ul->dci_info){
              dcchDtchConfig->ul->dci_info = calloc(1,sizeof(*(dcchDtchConfig->ul->dci_info)));
            }
            if(p_req->v.AddOrReconfigure.DcchDtchConfig.v.UL.v.SearchSpaceAndDci.v.DciInfo.v.ResoureAssignment.d){
              if(NULL == dcchDtchConfig->ul->dci_info->resoure_assignment){
                 dcchDtchConfig->ul->dci_info->resoure_assignment = calloc(1,sizeof(*(dcchDtchConfig->ul->dci_info->resoure_assignment)));
              }
              if(p_req->v.AddOrReconfigure.DcchDtchConfig.v.UL.v.SearchSpaceAndDci.v.DciInfo.v.ResoureAssignment.v.FreqDomain.d){
                dcchDtchConfig->ul->dci_info->resoure_assignment->FirstRbIndex =
                            p_req->v.AddOrReconfigure.DcchDtchConfig.v.UL.v.SearchSpaceAndDci.v.DciInfo.v.ResoureAssignment.v.FreqDomain.v.FirstRbIndex;
                dcchDtchConfig->ul->dci_info->resoure_assignment->Nprb =
                            p_req->v.AddOrReconfigure.DcchDtchConfig.v.UL.v.SearchSpaceAndDci.v.DciInfo.v.ResoureAssignment.v.FreqDomain.v.Nprb;
              }
              if(p_req->v.AddOrReconfigure.DcchDtchConfig.v.UL.v.SearchSpaceAndDci.v.DciInfo.v.ResoureAssignment.v.TransportBlockScheduling.d) {
                if(p_req->v.AddOrReconfigure.DcchDtchConfig.v.UL.v.SearchSpaceAndDci.v.DciInfo.v.ResoureAssignment.v.TransportBlockScheduling.v.d > 0){
                  struct NR_TransportBlockSingleTransmission_Type * tbst = &p_req->v.AddOrReconfigure.DcchDtchConfig.v.UL.v.SearchSpaceAndDci.v.DciInfo.v.ResoureAssignment.v.TransportBlockScheduling.v.v[0];
                  dcchDtchConfig->ul->dci_info->resoure_assignment->transportBlock_scheduling.imcs = tbst->ImcsValue;
                  dcchDtchConfig->ul->dci_info->resoure_assignment->transportBlock_scheduling.RedundancyVersion = tbst->RedundancyVersion;
                  dcchDtchConfig->ul->dci_info->resoure_assignment->transportBlock_scheduling.ToggleNDI = tbst->ToggleNDI;
                }
              }
            }
          }
        }
      }

      if(p_req->v.AddOrReconfigure.DcchDtchConfig.v.DrxCtrl.d)
      {

      }

      if(p_req->v.AddOrReconfigure.DcchDtchConfig.v.MeasGapCtrl.d)
      {

      }
    }

    /* 8. ServingCellConfig */
    /* TODO: populate ServingCellConfig to
        RC.nrrrc[gnbId]->carrier.cell_GroupId
        RC.nrrrc[gnbId]->carrier.mac_cellGroupConfig
        RC.nrrrc[gnbId]->carrier.physicalCellGroupConfig
    */
    if(p_req->v.AddOrReconfigure.ServingCellConfig.d)
    {

    }
    if(cell_State != SS_STATE_NOT_CONFIGURED){
      /* Trigger RRC Cell reconfig when cell is active */
      MessageDef *msg_p = NULL;
      msg_p = itti_alloc_new_message (TASK_GNB_APP, 0, NRRRC_CONFIGURATION_REQ);
      LOG_I(GNB_APP,"ss_gNB Sending configuration message to NR_RRC task\n");
      memcpy(&NRRRC_CONFIGURATION_REQ(msg_p), &RC.nrrrc[gnbId]->configuration,sizeof(NRRRC_CONFIGURATION_REQ(msg_p)));
      itti_send_msg_to_task (TASK_RRC_GNB, GNB_MODULE_ID_TO_INSTANCE(gnbId), msg_p);
      return true;
    }

    /* following code shall be optimized and moved to "PhysicalLayer" populating*/
    /*****************************************************************************/
    /* Populating PhyCellId */
    if (p_req->v.AddOrReconfigure.PhysicalLayer.v.Common.v.PhysicalCellId.d == true)
    {
      RC.nrrrc[gnbId]->carrier.physCellId = p_req->v.AddOrReconfigure.PhysicalLayer.v.Common.v.PhysicalCellId.v;
      *RC.nrrrc[gnbId]->configuration.scc->physCellId = p_req->v.AddOrReconfigure.PhysicalLayer.v.Common.v.PhysicalCellId.v;
    }

    /* Populating NR_ARFCN */
    if (p_req->v.AddOrReconfigure.PhysicalLayer.v.Downlink.v.FrequencyInfoDL.v.d == NR_ASN1_FrequencyInfoDL_Type_R15)
    {
      RC.nrrrc[gnbId]->configuration.scc->downlinkConfigCommon->frequencyInfoDL->absoluteFrequencyPointA =
        p_req->v.AddOrReconfigure.PhysicalLayer.v.Downlink.v.FrequencyInfoDL.v.v.R15.absoluteFrequencyPointA;
      LOG_A(GNB_APP, "fxn:%s DL absoluteFrequencyPointA :%ld\n", __FUNCTION__, 
          RC.nrrrc[gnbId]->configuration.scc->downlinkConfigCommon->frequencyInfoDL->absoluteFrequencyPointA);
    }

    /* Populating absoluteFrequencySSB */
    if (p_req->v.AddOrReconfigure.PhysicalLayer.v.Downlink.v.FrequencyInfoDL.v.v.R15.absoluteFrequencySSB.d == true)
    {
      *RC.nrrrc[gnbId]->configuration.scc->downlinkConfigCommon->frequencyInfoDL->absoluteFrequencySSB =
        p_req->v.AddOrReconfigure.PhysicalLayer.v.Downlink.v.FrequencyInfoDL.v.v.R15.absoluteFrequencySSB.v;
      LOG_A(GNB_APP, "fxn:%s DL absoluteFrequencySSB:%ld\n", __FUNCTION__, 
          *RC.nrrrc[gnbId]->configuration.scc->downlinkConfigCommon->frequencyInfoDL->absoluteFrequencySSB);
    }

    /* Populating frequency band list */

    for (int i = 0; i < p_req->v.AddOrReconfigure.PhysicalLayer.v.Downlink.v.FrequencyInfoDL.v.v.R15.frequencyBandList.d; i++)
    {
      *RC.nrrrc[gnbId]->configuration.scc->downlinkConfigCommon->frequencyInfoDL->frequencyBandList.list.array[i] =
        p_req->v.AddOrReconfigure.PhysicalLayer.v.Downlink.v.FrequencyInfoDL.v.v.R15.frequencyBandList.v[i];

      if (p_req->v.AddOrReconfigure.PhysicalLayer.v.Common.v.DuplexMode.v.d == NR_DuplexMode_Type_TDD)
      {
        LOG_A(NR_MAC, "Duplex mode TDD\n");
        *RC.nrrrc[gnbId]->configuration.scc->uplinkConfigCommon->frequencyInfoUL->frequencyBandList->list.array[i]= 
          p_req->v.AddOrReconfigure.PhysicalLayer.v.Downlink.v.FrequencyInfoDL.v.v.R15.frequencyBandList.v[i];

      }
      LOG_A(GNB_APP, "fxn:%s DL band[%d]:%ld UL band[%d]:%ld\n", __FUNCTION__, i, 
          *RC.nrrrc[gnbId]->configuration.scc->downlinkConfigCommon->frequencyInfoDL->frequencyBandList.list.array[i],
          i,
          *RC.nrrrc[gnbId]->configuration.scc->uplinkConfigCommon->frequencyInfoUL->frequencyBandList->list.array[i]);

    }

    /* Populating scs_SpecificCarrierList */
    for (int i = 0; i < p_req->v.AddOrReconfigure.PhysicalLayer.v.Downlink.v.FrequencyInfoDL.v.v.R15.scs_SpecificCarrierList.d; i++)
    {
      RC.nrrrc[gnbId]->configuration.scc->downlinkConfigCommon->frequencyInfoDL->scs_SpecificCarrierList.list.array[i]->offsetToCarrier =
        p_req->v.AddOrReconfigure.PhysicalLayer.v.Downlink.v.FrequencyInfoDL.v.v.R15.scs_SpecificCarrierList.v[i].offsetToCarrier;

      RC.nrrrc[gnbId]->configuration.scc->downlinkConfigCommon->frequencyInfoDL->scs_SpecificCarrierList.list.array[i]->subcarrierSpacing =
        p_req->v.AddOrReconfigure.PhysicalLayer.v.Downlink.v.FrequencyInfoDL.v.v.R15.scs_SpecificCarrierList.v[i].subcarrierSpacing;

      RC.nrrrc[gnbId]->configuration.scc->downlinkConfigCommon->frequencyInfoDL->scs_SpecificCarrierList.list.array[i]->carrierBandwidth =
        p_req->v.AddOrReconfigure.PhysicalLayer.v.Downlink.v.FrequencyInfoDL.v.v.R15.scs_SpecificCarrierList.v[i].carrierBandwidth;

      *RC.nrrrc[gnbId]->configuration.scc->ssbSubcarrierSpacing = 
        RC.nrrrc[gnbId]->configuration.scc->downlinkConfigCommon->frequencyInfoDL->scs_SpecificCarrierList.list.array[i]->subcarrierSpacing;


      LOG_A(GNB_APP, "fxn:%s DL scs_SpecificCarrierList.offsetToCarrier :%ld subcarrierSpacing:%ld carrierBandwidth:%ld \n",
          __FUNCTION__, 
          RC.nrrrc[gnbId]->configuration.scc->downlinkConfigCommon->frequencyInfoDL->scs_SpecificCarrierList.list.array[i]->offsetToCarrier,
          RC.nrrrc[gnbId]->configuration.scc->downlinkConfigCommon->frequencyInfoDL->scs_SpecificCarrierList.list.array[i]->subcarrierSpacing,
          RC.nrrrc[gnbId]->configuration.scc->downlinkConfigCommon->frequencyInfoDL->scs_SpecificCarrierList.list.array[i]->carrierBandwidth);
    }

    if (p_req->v.AddOrReconfigure.PhysicalLayer.v.Common.v.DuplexMode.v.d == NR_DuplexMode_Type_TDD)
    {
      RC.nrrrc[gnbId]->configuration.scc->tdd_UL_DL_ConfigurationCommon->referenceSubcarrierSpacing = 
        p_req->v.AddOrReconfigure.PhysicalLayer.v.Common.v.DuplexMode.v.v.TDD.v.Config.Common.v.v.R15.referenceSubcarrierSpacing;

      RC.nrrrc[gnbId]->configuration.scc->tdd_UL_DL_ConfigurationCommon->pattern1.dl_UL_TransmissionPeriodicity = 
        p_req->v.AddOrReconfigure.PhysicalLayer.v.Common.v.DuplexMode.v.v.TDD.v.Config.Common.v.v.R15.pattern1.dl_UL_TransmissionPeriodicity;

      RC.nrrrc[gnbId]->configuration.scc->tdd_UL_DL_ConfigurationCommon->pattern1.nrofDownlinkSlots = 
        p_req->v.AddOrReconfigure.PhysicalLayer.v.Common.v.DuplexMode.v.v.TDD.v.Config.Common.v.v.R15.pattern1.nrofDownlinkSlots;

      RC.nrrrc[gnbId]->configuration.scc->tdd_UL_DL_ConfigurationCommon->pattern1.nrofDownlinkSymbols = 
        p_req->v.AddOrReconfigure.PhysicalLayer.v.Common.v.DuplexMode.v.v.TDD.v.Config.Common.v.v.R15.pattern1.nrofDownlinkSymbols;

      RC.nrrrc[gnbId]->configuration.scc->tdd_UL_DL_ConfigurationCommon->pattern1.nrofUplinkSlots = 
        p_req->v.AddOrReconfigure.PhysicalLayer.v.Common.v.DuplexMode.v.v.TDD.v.Config.Common.v.v.R15.pattern1.nrofUplinkSlots;

      RC.nrrrc[gnbId]->configuration.scc->tdd_UL_DL_ConfigurationCommon->pattern1.nrofUplinkSymbols = 
        p_req->v.AddOrReconfigure.PhysicalLayer.v.Common.v.DuplexMode.v.v.TDD.v.Config.Common.v.v.R15.pattern1.nrofUplinkSymbols;

    }

    RC.nrrrc[gnbId]->configuration.ssb_SubcarrierOffset = 
      p_req->v.AddOrReconfigure.BcchConfig.v.BcchInfo.v.MIB.v.message.v.mib.ssb_SubcarrierOffset;

    /* UL Absolute Frequency Population  */
    /* Populating NR_ARFCN */
    if (p_req->v.AddOrReconfigure.PhysicalLayer.v.Uplink.v.Uplink.v.v.Config.FrequencyInfoUL.v.d == NR_ASN1_FrequencyInfoUL_Type_R15)
    {

      LOG_I(NR_MAC,"fxn:%s Populating FrequencyInfoUL from TTCN. number of scs:%ld \n", 
      __FUNCTION__,
      p_req->v.AddOrReconfigure.PhysicalLayer.v.Downlink.v.FrequencyInfoDL.v.v.R15.scs_SpecificCarrierList.d);

      /* Populating scs_SpecificCarrierList */
      for (int i = 0; i < p_req->v.AddOrReconfigure.PhysicalLayer.v.Uplink.v.Uplink.v.v.Config.FrequencyInfoUL.v.v.R15.scs_SpecificCarrierList.d; i++)
      {

        RC.nrrrc[gnbId]->configuration.scc->uplinkConfigCommon->frequencyInfoUL->scs_SpecificCarrierList.list.array[i]->carrierBandwidth=
          p_req->v.AddOrReconfigure.PhysicalLayer.v.Uplink.v.Uplink.v.v.Config.FrequencyInfoUL.v.v.R15.scs_SpecificCarrierList.v[i].carrierBandwidth;

        RC.nrrrc[gnbId]->configuration.scc->uplinkConfigCommon->frequencyInfoUL->scs_SpecificCarrierList.list.array[i]->offsetToCarrier =
          p_req->v.AddOrReconfigure.PhysicalLayer.v.Uplink.v.Uplink.v.v.Config.FrequencyInfoUL.v.v.R15.scs_SpecificCarrierList.v[i].offsetToCarrier;

        RC.nrrrc[gnbId]->configuration.scc->uplinkConfigCommon->frequencyInfoUL->scs_SpecificCarrierList.list.array[i]->subcarrierSpacing =
          p_req->v.AddOrReconfigure.PhysicalLayer.v.Uplink.v.Uplink.v.v.Config.FrequencyInfoUL.v.v.R15.scs_SpecificCarrierList.v[i].subcarrierSpacing;
        LOG_A(GNB_APP, 
            "fxn:%s UL scs_SpecificCarrierList.carrierBandwidth:%ld\n scs_SpecificCarrierList.offsetToCarrier:%ld\n scs_SpecificCarrierList.subcarrierSpacing:%ld", 
            __FUNCTION__, 
            RC.nrrrc[gnbId]->configuration.scc->uplinkConfigCommon->frequencyInfoUL->scs_SpecificCarrierList.list.array[i]->carrierBandwidth,
            RC.nrrrc[gnbId]->configuration.scc->uplinkConfigCommon->frequencyInfoUL->scs_SpecificCarrierList.list.array[i]->offsetToCarrier,
            RC.nrrrc[gnbId]->configuration.scc->uplinkConfigCommon->frequencyInfoUL->scs_SpecificCarrierList.list.array[i]->subcarrierSpacing);
      }
      *RC.nrrrc[gnbId]->configuration.scc->uplinkConfigCommon->frequencyInfoUL->p_Max =
        p_req->v.AddOrReconfigure.PhysicalLayer.v.Uplink.v.Uplink.v.v.Config.FrequencyInfoUL.v.v.R15.p_Max.v;

      LOG_A(GNB_APP, "fxn:%s UL p_Max :%ld\n", __FUNCTION__, 
          *RC.nrrrc[gnbId]->configuration.scc->uplinkConfigCommon->frequencyInfoUL->p_Max);

    }
    /*****************************************************************************/
  }

  return true;
}

/*
 * Function : cell_config_5G_done_indication
 * Description: Sends the cell_config_done_mutex signl to LTE_SOFTMODEM,
 * as in SS mode the eNB is waiting for the cell configration to be
 * received form TTCN. After receiving this signal only the eNB's init
 * is completed and its ready for processing.
 */
int cell_config_5G_done_indication()
{
  if (cell_config_5G_done < 0)
  {
    LOG_A(GNB_APP, "[SYS-GNB] fxn:%s Signal to TASK_GNB_APP about cell configuration complete", __FUNCTION__);
    pthread_mutex_lock(&cell_config_5G_done_mutex);
    cell_config_5G_done = 0;
    pthread_cond_broadcast(&cell_config_5G_done_cond);
    pthread_mutex_unlock(&cell_config_5G_done_mutex);
  }

  return 0;
}

/*
 * Function    : ss_task_sys_nr_handle_cellConfigRadioBearer
 * Description : This function handles the CellConfig 5G API on SYS Port and send processes the request. 
 * Returns     : true/false
 */

bool ss_task_sys_nr_handle_cellConfigRadioBearer(struct NR_SYSTEM_CTRL_REQ *req)
{
  struct NR_RadioBearer_Type_NR_RadioBearerList_Type_Dynamic *BearerList =  &(req->Request.v.RadioBearerList);
  LOG_A(GNB_APP, "[SYS-GNB] Entry in fxn:%s\n", __FUNCTION__);
  MessageDef *msg_p = itti_alloc_new_message(TASK_SYS_GNB, 0, NRRRC_RBLIST_CFG_REQ);
  if (msg_p)
  {
    LOG_A(GNB_APP, "[SYS-GNB] BearerList size:%lu\n", BearerList->d);
    NRRRC_RBLIST_CFG_REQ(msg_p).rb_count = 0;
    NRRRC_RBLIST_CFG_REQ(msg_p).cell_index = 0;  //TODO: change to multicell index later
    for (int i = 0; i < BearerList->d; i++)
    {
      LOG_A(GNB_APP,"[SYS-GNB] RB Index i:%d\n", i);
      nr_rb_info * rb_info = &(NRRRC_RBLIST_CFG_REQ(msg_p).rb_list[i]);
      memset(rb_info, 0, sizeof(nr_rb_info));
      if (BearerList->v[i].Id.d == NR_RadioBearerId_Type_Srb)
      {
        rb_info->RbId = BearerList->v[i].Id.v.Srb;
      }
      else if (BearerList->v[i].Id.d == NR_RadioBearerId_Type_Drb)
      {
        rb_info->RbId = BearerList->v[i].Id.v.Drb + 2; // Added 2 for MAXSRB because DRB1 starts from index-3
      }

      NRRadioBearerConfig * rbConfig = &rb_info->RbConfig;
      if (BearerList->v[i].Config.d == NR_RadioBearerConfig_Type_AddOrReconfigure)
      {
        NRRRC_RBLIST_CFG_REQ(msg_p).rb_count++;
        /* Populate the SDAP Configuration for the radio Bearer */
        if(BearerList->v[i].Config.v.AddOrReconfigure.Sdap.d)
        {
          if(BearerList->v[i].Config.v.AddOrReconfigure.Sdap.v.d == SDAP_Configuration_Type_Config)
          {
            if(BearerList->v[i].Config.v.AddOrReconfigure.Sdap.v.v.Config.d == SdapConfigInfo_Type_SdapConfig)
            {
              NR_SDAP_Config_t * sdap = CALLOC(1,sizeof(NR_SDAP_Config_t));
              sdap->pdu_Session =  BearerList->v[i].Config.v.AddOrReconfigure.Sdap.v.v.Config.v.SdapConfig.Pdu_SessionId;
              if(BearerList->v[i].Config.v.AddOrReconfigure.Sdap.v.v.Config.v.SdapConfig.Sdap_HeaderDL.d)
              {
                sdap->sdap_HeaderDL =  BearerList->v[i].Config.v.AddOrReconfigure.Sdap.v.v.Config.v.SdapConfig.Sdap_HeaderDL.v;
              }
              if(BearerList->v[i].Config.v.AddOrReconfigure.Sdap.v.v.Config.v.SdapConfig.MappedQoS_Flows.d)
              {
                sdap->mappedQoS_FlowsToAdd = CALLOC(1,sizeof(*sdap->mappedQoS_FlowsToAdd));
                for(int j=0; j < BearerList->v[i].Config.v.AddOrReconfigure.Sdap.v.v.Config.v.SdapConfig.MappedQoS_Flows.v.d; j++)
                {
                  NR_QFI_t * qfi = CALLOC(1,sizeof(NR_QFI_t));
                  *qfi = BearerList->v[i].Config.v.AddOrReconfigure.Sdap.v.v.Config.v.SdapConfig.MappedQoS_Flows.v.v[j];
                  ASN_SEQUENCE_ADD(&sdap->mappedQoS_FlowsToAdd->list,qfi);
                }
              }
              rbConfig->Sdap = sdap;
            }else if(BearerList->v[i].Config.v.AddOrReconfigure.Sdap.v.v.Config.d == SdapConfigInfo_Type_TransparentMode){

            }
          }
         }

        /* Populate the PDCP Configuration for the radio Bearer */
        if (BearerList->v[i].Config.v.AddOrReconfigure.Pdcp.d)
        {
          if (BearerList->v[i].Config.v.AddOrReconfigure.Pdcp.v.d == NR_PDCP_Configuration_Type_RBTerminating)
          {
            if (BearerList->v[i].Config.v.AddOrReconfigure.Pdcp.v.v.RBTerminating.RbConfig.d)
            {
              if (BearerList->v[i].Config.v.AddOrReconfigure.Pdcp.v.v.RBTerminating.RbConfig.v.d == NR_PDCP_RbConfig_Type_Params)
              {
                if(BearerList->v[i].Config.v.AddOrReconfigure.Pdcp.v.v.RBTerminating.RbConfig.v.v.Params.Rb.d == NR_PDCP_RB_Config_Parameters_Type_Srb)
                {
                  LOG_A(GNB_APP,"[SYS-GNB] PDCP Config for Bearer Id: %d is Null\n", rb_info->RbId);
                }
                else if(BearerList->v[i].Config.v.AddOrReconfigure.Pdcp.v.v.RBTerminating.RbConfig.v.v.Params.Rb.d == NR_PDCP_RB_Config_Parameters_Type_Drb)
                {
                  NR_PDCP_Config_t *pdcp = CALLOC(1,sizeof(NR_PDCP_Config_t));
                  pdcp->drb = CALLOC(1, sizeof(*pdcp->drb));
                  pdcp->drb->pdcp_SN_SizeUL=CALLOC(1,sizeof(long));
                  *(pdcp->drb->pdcp_SN_SizeUL) = BearerList->v[i].Config.v.AddOrReconfigure.Pdcp.v.v.RBTerminating.RbConfig.v.v.Params.Rb.v.Drb.SN_SizeUL;
                  pdcp->drb->pdcp_SN_SizeDL=CALLOC(1,sizeof(long));
                  *(pdcp->drb->pdcp_SN_SizeDL) = BearerList->v[i].Config.v.AddOrReconfigure.Pdcp.v.v.RBTerminating.RbConfig.v.v.Params.Rb.v.Drb.SN_SizeDL;

                  if(BearerList->v[i].Config.v.AddOrReconfigure.Pdcp.v.v.RBTerminating.RbConfig.v.v.Params.Rb.v.Drb.IntegrityProtectionEnabled)
                  {
                    pdcp->drb->integrityProtection = CALLOC(1,sizeof(long));
                    *(pdcp->drb->integrityProtection) = 1;
                  }
                  //No rohc config for DRB from TTCN
                  if(BearerList->v[i].Config.v.AddOrReconfigure.Pdcp.v.v.RBTerminating.RbConfig.v.v.Params.Rb.v.Drb.HeaderCompression.d == NR_PDCP_DRB_HeaderCompression_Type_None)
                  {
                  }
                  rbConfig->Pdcp = pdcp;
                }
              }
              else if (BearerList->v[i].Config.v.AddOrReconfigure.Pdcp.v.v.RBTerminating.RbConfig.v.d == NR_PDCP_RbConfig_Type_TransparentMode)
              {
                rbConfig->pdcpTransparentSN_Size = CALLOC(1,sizeof(long));
                *(rbConfig->pdcpTransparentSN_Size) = BearerList->v[i].Config.v.AddOrReconfigure.Pdcp.v.v.RBTerminating.RbConfig.v.v.TransparentMode.SN_Size;
              }
            }

            if(BearerList->v[i].Config.v.AddOrReconfigure.Pdcp.v.v.RBTerminating.LinkToOtherCellGroup.d)
            {
              if(BearerList->v[i].Config.v.AddOrReconfigure.Pdcp.v.v.RBTerminating.LinkToOtherCellGroup.v.d == RlcBearerRouting_Type_EUTRA)
              {
                //BearerList->v[i].Config.v.AddOrReconfigure.Pdcp.v.v.RBTerminating.LinkToOtherCellGroup.v.v.EUTRA
              }
              else if(BearerList->v[i].Config.v.AddOrReconfigure.Pdcp.v.v.RBTerminating.LinkToOtherCellGroup.v.d == RlcBearerRouting_Type_NR)
              {
                //BearerList->v[i].Config.v.AddOrReconfigure.Pdcp.v.v.RBTerminating.LinkToOtherCellGroup.v.v.NR
              }
            }
          }
          else if(BearerList->v[i].Config.v.AddOrReconfigure.Pdcp.v.d == NR_PDCP_Configuration_Type_Proxy)
          {
            if(BearerList->v[i].Config.v.AddOrReconfigure.Pdcp.v.v.Proxy.LinkToOtherNode.d == RlcBearerRouting_Type_EUTRA)
            {
              //BearerList->v[i].Config.v.AddOrReconfigure.Pdcp.v.v.Proxy.LinkToOtherNode.v.EUTRA
            }
            else if(BearerList->v[i].Config.v.AddOrReconfigure.Pdcp.v.v.Proxy.LinkToOtherNode.d == RlcBearerRouting_Type_NR)
            {
              //BearerList->v[i].Config.v.AddOrReconfigure.Pdcp.v.v.Proxy.LinkToOtherNode.v.NR
            }
          }
        }

        /* Populate the RlcBearerConfig for the radio Bearer */
        if (BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.d)
        {
          NR_RLC_BearerConfig_t * rlcBearer = CALLOC(1,sizeof(NR_RLC_BearerConfig_t));
          if (BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.d == NR_RlcBearerConfig_Type_Config)
          {
            /* Populate the Rlc Config of RlcBearerConfig for the radio Bearer */
            if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.d)
            {
              if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.d)
              {
                NR_RLC_Config_t *rlc = CALLOC(1,sizeof(NR_RLC_Config_t));
                if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.d == NR_RLC_RbConfig_Type_AM)
                {
                  rlc->present = NR_RLC_Config_PR_am;
                  rlc->choice.am = CALLOC(1,sizeof(*rlc->choice.am));
                  if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.v.AM.Tx.d)
                  {
                    if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.v.AM.Tx.v.d == NR_ASN1_UL_AM_RLC_Type_R15)
                    {
                      if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.v.AM.Tx.v.v.R15.sn_FieldLength.d)
                      {
                        rlc->choice.am->ul_AM_RLC.sn_FieldLength = CALLOC(1,sizeof(long));
                        *(rlc->choice.am->ul_AM_RLC.sn_FieldLength) =  BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.v.AM.Tx.v.v.R15.sn_FieldLength.v;
                      }
                      rlc->choice.am->ul_AM_RLC.t_PollRetransmit = BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.v.AM.Tx.v.v.R15.t_PollRetransmit;
                      rlc->choice.am->ul_AM_RLC.pollPDU = BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.v.AM.Tx.v.v.R15.pollPDU;
                      rlc->choice.am->ul_AM_RLC.pollByte = BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.v.AM.Tx.v.v.R15.pollByte;
                      rlc->choice.am->ul_AM_RLC.maxRetxThreshold = BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.v.AM.Tx.v.v.R15.maxRetxThreshold;
                    }
                  }
                  if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.v.AM.Rx.d)
                  {
                    if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.v.AM.Rx.v.d == NR_ASN1_DL_AM_RLC_Type_R15)
                    {
                      if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.v.AM.Rx.v.v.R15.sn_FieldLength.d)
                      {
                        rlc->choice.am->dl_AM_RLC.sn_FieldLength = CALLOC(1,sizeof(long));
                        *(rlc->choice.am->dl_AM_RLC.sn_FieldLength) = BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.v.AM.Rx.v.v.R15.sn_FieldLength.v;
                      }
                      rlc->choice.am->dl_AM_RLC.t_Reassembly = BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.v.AM.Rx.v.v.R15.t_Reassembly;
                      rlc->choice.am->dl_AM_RLC.t_StatusProhibit = BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.v.AM.Rx.v.v.R15.t_StatusProhibit;
                    }
                  }
                }
                else if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.d == NR_RLC_RbConfig_Type_UM)
                {
                  NR_UL_UM_RLC_t *ul_UM_RLC = NULL;
                  NR_DL_UM_RLC_t *dl_UM_RLC = NULL;
                  if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.v.UM.Tx.d && BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.v.UM.Rx.d)
                  {
                    rlc->present = NR_RLC_Config_PR_um_Bi_Directional;
                    rlc->choice.um_Bi_Directional = CALLOC(1,sizeof(*rlc->choice.um_Bi_Directional));
                    ul_UM_RLC = &rlc->choice.um_Bi_Directional->ul_UM_RLC;
                    dl_UM_RLC = &rlc->choice.um_Bi_Directional->dl_UM_RLC;
                  }
                  else if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.v.UM.Tx.d)
                  {
                    rlc->present = NR_RLC_Config_PR_um_Uni_Directional_UL;
                    rlc->choice.um_Uni_Directional_UL = CALLOC(1,sizeof(*rlc->choice.um_Uni_Directional_UL));
                    ul_UM_RLC = &rlc->choice.um_Uni_Directional_UL->ul_UM_RLC;
                  }
                  else if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.v.UM.Rx.d)
                  {
                    rlc->present = NR_RLC_Config_PR_um_Uni_Directional_DL;
                    rlc->choice.um_Uni_Directional_DL = CALLOC(1,sizeof(*rlc->choice.um_Uni_Directional_DL));
                    dl_UM_RLC = &rlc->choice.um_Uni_Directional_DL->dl_UM_RLC;
                  }

                  if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.v.UM.Tx.d)
                  {
                    if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.v.UM.Tx.v.d == NR_ASN1_UL_UM_RLC_Type_R15)
                    {
                      if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.v.UM.Tx.v.v.R15.sn_FieldLength.d)
                      {
                        ul_UM_RLC->sn_FieldLength = CALLOC(1,sizeof(long));
                        *(ul_UM_RLC->sn_FieldLength) = BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.v.UM.Tx.v.v.R15.sn_FieldLength.v;
                      }
                    }
                  }
                  if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.v.UM.Rx.d)
                  {
                     if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.v.UM.Rx.v.d == NR_ASN1_DL_UM_RLC_Type_R15)
                    {
                      if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.v.UM.Rx.v.v.R15.sn_FieldLength.d)
                      {
                        dl_UM_RLC->sn_FieldLength = CALLOC(1,sizeof(long));
                        *(dl_UM_RLC->sn_FieldLength) = BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.v.UM.Rx.v.v.R15.sn_FieldLength.v;
                      }
                      dl_UM_RLC->t_Reassembly = BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.v.UM.Rx.v.v.R15.t_Reassembly;
                    }
                  }
                }
                else if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.Rb.v.d == NR_RLC_RbConfig_Type_TM)
                {
                  //TODO: TransparentMode
                }
                else
                {
                  rlc->present = NR_RLC_Config_PR_NOTHING;
                }
                rlcBearer->rlc_Config = rlc;
              }
              if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.TestMode.d)
              {
                //TODO: RLC TestMode
                if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.TestMode.v.d == NR_RLC_TestModeConfig_Type_Info)
                {
                  if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.TestMode.v.v.Info.d == NR_RLC_TestModeInfo_Type_AckProhibit)
                  {
                  }
                  else if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.TestMode.v.v.Info.d == NR_RLC_TestModeInfo_Type_NotACK_NextRLC_PDU)
                  {
                  }
                  else if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.TestMode.v.v.Info.d == NR_RLC_TestModeInfo_Type_TransparentMode)
                  {
                  }
                }
                else if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Rlc.v.TestMode.v.d == NR_RLC_TestModeConfig_Type_None)
                {
                }
              }
            }
            /* Populate the LogicalChannelId of RlcBearerConfig for the radio Bearer */
            if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.LogicalChannelId.d)
            {
              rlcBearer->logicalChannelIdentity = BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.LogicalChannelId.v;
            }

            /* Populate the LogicalChannelConfig of RlcBearerConfig for the radio Bearer */
            if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Mac.d)
            {
              if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Mac.v.LogicalChannel.d)
              {
                NR_LogicalChannelConfig_t * logicalChannelConfig = CALLOC(1,sizeof(NR_LogicalChannelConfig_t));
                logicalChannelConfig->ul_SpecificParameters = CALLOC(1,sizeof(*logicalChannelConfig->ul_SpecificParameters));
                logicalChannelConfig->ul_SpecificParameters->priority =  BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Mac.v.LogicalChannel.v.Priority;
                logicalChannelConfig->ul_SpecificParameters->prioritisedBitRate = BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Mac.v.LogicalChannel.v.PrioritizedBitRate;
                rlcBearer->mac_LogicalChannelConfig = logicalChannelConfig;
              }
              if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.Mac.v.TestMode.d)
              {
                //TODO: MAC test mode
              }
            }

            /* Populate the DiscardULData of RlcBearerConfig for the radio Bearer */
            if(BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.DiscardULData.d)
            {
              //typically applicable for UM DRBs only: if true: SS shall discard any data in UL for this radio bearer
              rbConfig->DiscardULData =CALLOC(1,sizeof(bool));
              *(rbConfig->DiscardULData) = BearerList->v[i].Config.v.AddOrReconfigure.RlcBearer.v.v.Config.DiscardULData.v;
            }
          }
          rbConfig->RlcBearer = rlcBearer;
        }
      }
    }
    LOG_A(GNB_APP, "[SYS-GNB] Send NRRRC_RBLIST_CFG_REQ to TASK_RRC_GNB, RB Count : %d, Message: %s  \n", NRRRC_RBLIST_CFG_REQ(msg_p).rb_count, ITTI_MSG_NAME(msg_p));
    int send_res = itti_send_msg_to_task(TASK_RRC_GNB, 0, msg_p);
    if (send_res < 0)
    {
      LOG_A(GNB_APP, "[SYS-GNB] Error sending NRRRC_RBLIST_CFG_REQ to RRC_GNB \n");
    }

  }

  send_sys_cnf(ConfirmationResult_Type_Success, true, NR_SystemConfirm_Type_RadioBearerList, NULL);
  LOG_A(GNB_APP, "[SYS-GNB] Exit from fxn:%s\n", __FUNCTION__);
  return true;
}

/*
 * Function    : ss_task_sys_nr_handle_cellConfigAttenuation
 * Description : This function handles the CellConfig 5G API on SYS Port for request type CellAttenuation and send processes the request. 
 * Returns     : true/false
 */

bool ss_task_sys_nr_handle_cellConfigAttenuation(struct NR_SYSTEM_CTRL_REQ *req)
{
  LOG_A(GNB_APP, "[SYS-GNB] Entry in fxn:%s\n", __FUNCTION__);
  struct NR_SYSTEM_CTRL_CNF *msgCnf = CALLOC(1, sizeof(struct NR_SYSTEM_CTRL_CNF));
  MessageDef *message_p = itti_alloc_new_message(TASK_SYS_GNB, INSTANCE_DEFAULT, SS_NR_SYS_PORT_MSG_CNF);

  if (message_p)
  {
    LOG_A(GNB_APP, "[SYS-GNB] Send SS_NR_SYS_PORT_MSG_CNF\n");
    msgCnf->Common.CellId = nr_Cell_NonSpecific;
    msgCnf->Common.Result.d = ConfirmationResult_Type_Success;
    msgCnf->Common.Result.v.Success = true;
    msgCnf->Confirm.d = NR_SystemConfirm_Type_CellAttenuationList;
    msgCnf->Confirm.v.CellAttenuationList = true;

    SS_NR_SYS_PORT_MSG_CNF(message_p).cnf = msgCnf;
    int send_res = itti_send_msg_to_task(TASK_SS_PORTMAN_GNB, INSTANCE_DEFAULT, message_p);
    if (send_res < 0)
    {
      LOG_A(GNB_APP, "[SYS-GNB] Error sending to [SS-PORTMAN-GNB]\n");
      return false;
    }
    else
    {
      LOG_A(GNB_APP, "[SYS-GNB] fxn:%s NR_SYSTEM_CTRL_CNF sent for cnfType:CellAttenuationList to Port Manager\n", __FUNCTION__);
    }
  }
  LOG_A(GNB_APP, "[SYS-GNB] Exit from fxn:%s\n", __FUNCTION__);
  return true;
}

/*
 * Function : sys_5G_send_proxy
 * Description: Sends the messages from SYS to proxy
 */
static void sys_5G_send_proxy(void *msg, int msgLen)
{
  LOG_A(ENB_SS, "Entry in %s\n", __FUNCTION__);
  uint32_t peerIpAddr = 0;
  uint16_t peerPort = proxy_5G_send_port;

  IPV4_STR_ADDR_TO_INT_NWBO(local_5G_address,peerIpAddr, " BAD IP Address");

  LOG_A(ENB_SS, "Sending CELL CONFIG 5G to Proxy\n");
  int8_t *temp = msg;

  /** Send to proxy */
  sys_send_udp_msg((uint8_t *)msg, msgLen, 0, peerIpAddr, peerPort);
  LOG_A(ENB_SS, "Exit from %s\n", __FUNCTION__);
  return;
}

/*
 * Function : sys_send_udp_msg
 * Description: Sends the UDP_INIT message to UDP_TASK to create the listening socket
 */
static int sys_send_udp_msg(
    uint8_t *buffer,
    uint32_t buffer_len,
    uint32_t buffer_offset,
    uint32_t peerIpAddr,
    uint16_t peerPort)
{
  // Create and alloc new message
  MessageDef *message_p = NULL;
  udp_data_req_t *udp_data_req_p = NULL;
  message_p = itti_alloc_new_message(TASK_SYS_GNB, 0, UDP_DATA_REQ);

  if (message_p)
  {
    LOG_A(ENB_SS, "Sending UDP_DATA_REQ length %u offset %u buffer %d %d %d \n", buffer_len, buffer_offset, buffer[0], buffer[1], buffer[2]);
    udp_data_req_p = &message_p->ittiMsg.udp_data_req;
    udp_data_req_p->peer_address = peerIpAddr;
    udp_data_req_p->peer_port = peerPort;
    udp_data_req_p->buffer = buffer;
    udp_data_req_p->buffer_length = buffer_len;
    udp_data_req_p->buffer_offset = buffer_offset;
    return itti_send_msg_to_task(TASK_UDP, 0, message_p);
  }
  else
  {
    LOG_A(ENB_SS, "Failed Sending UDP_DATA_REQ length %u offset %u \n", buffer_len, buffer_offset);
    return -1;
  }
}

/*
 * Function : sys_send_init_udp
 * Description: Sends the UDP_INIT message to UDP_TASK to create the receiving socket
 * for the SYS_TASK from the Proxy for the configuration confirmations.
 */
static int sys_5G_send_init_udp(const udpSockReq_t *req)
{ 
  // Create and alloc new message
  MessageDef *message_p;
  message_p = itti_alloc_new_message(TASK_SYS_GNB, 0, UDP_INIT);
  if (message_p == NULL)
  {
    return -1;
  }
  UDP_INIT(message_p).port = req->port;
  //addr.s_addr = req->ss_ip_addr;
  UDP_INIT(message_p).address = req->address; //inet_ntoa(addr);
  LOG_A(ENB_SS, "Tx UDP_INIT IP addr %s (%x)\n", UDP_INIT(message_p).address, UDP_INIT(message_p).port);
  MSC_LOG_EVENT(
      MSC_GTPU_ENB,
      "0 UDP bind  %s:%u",
      UDP_INIT(message_p).address,
      UDP_INIT(message_p).port);
  return itti_send_msg_to_task(TASK_UDP, 0, message_p);
}


bool ss_task_sys_nr_handle_pdcpCount(struct NR_SYSTEM_CTRL_REQ *req)
{

  LOG_A(ENB_SS, "Entry in fxn:%s\n", __FUNCTION__);
  struct NR_PDCP_CountCnf_Type PdcpCount;

  PdcpCount.d = NR_PDCP_CountCnf_Type_Get;
  PdcpCount.v.Get.d = 1;
  const size_t size = sizeof(struct NR_PdcpCountInfo_Type) * PdcpCount.v.Get.d;
  PdcpCount.v.Get.v =(struct NR_PdcpCountInfo_Type *)acpMalloc(size);
  for (int i = 0; i < PdcpCount.v.Get.d; i++)
  {
    PdcpCount.v.Get.v[i].RadioBearerId.d = NR_RadioBearerId_Type_Srb;
    PdcpCount.v.Get.v[i].RadioBearerId.v.Srb = 1;
    PdcpCount.v.Get.v[i].UL.d = true;
    PdcpCount.v.Get.v[i].DL.d = true;

    PdcpCount.v.Get.v[i].UL.v.Format = E_PdcpCount_Srb;
    PdcpCount.v.Get.v[i].DL.v.Format = E_PdcpCount_Srb;

    int_to_bin(1, 32, PdcpCount.v.Get.v[i].UL.v.Value);
    int_to_bin(1, 32, PdcpCount.v.Get.v[i].DL.v.Value);
  }

  send_sys_cnf(ConfirmationResult_Type_Success, true, NR_SystemConfirm_Type_PdcpCount, (void *)&PdcpCount);
  LOG_A(ENB_SS, "Exit from fxn:%s\n", __FUNCTION__);
  return true;

}