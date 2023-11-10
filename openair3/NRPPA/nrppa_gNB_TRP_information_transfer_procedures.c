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

/*! \file nrppa_gNB_TRP_information_transfer_procedures.c
 * \brief NRPPA gNB tasks related to TRP information transfer
 * \author Adeel Malik
 * \email adeel.malik@eurecom.fr
 *\date 2023
 * \version 1.0
 * @ingroup _nrppa
 */


#include "intertask_interface.h"

#include "nrppa_common.h"
#include "nrppa_gNB_TRP_information_transfer_procedures.h"
#include "nrppa_gNB_itti_messaging.h"

/* TRPInformationExchange (Parent) procedure for  TRPInformationRequest, TRPInformationResponse, and TRPInformationFailure*/
// adeel TODO fill F1AP msg for rrc
int nrppa_gNB_handle_TRPInformationExchange(nrppa_gnb_ue_info_t *nrppa_msg_info, NRPPA_NRPPA_PDU_t *pdu)
{
    LOG_D(NRPPA, "Processing Received TRPInformationRequest \n");
// Processing Received TRPInformationRequest
    NRPPA_TRPInformationRequest_t     *container;
    NRPPA_TRPInformationRequest_IEs_t *ie;
    uint32_t                         nrppa_transaction_id;

    DevAssert(pdu != NULL);

    container = &pdu->choice.initiatingMessage->value.choice.TRPInformationRequest; //IE 9.2.3 Message type (M)
    nrppa_transaction_id = pdu->choice.initiatingMessage->nrppatransactionID; // IE 9.2.4 nrppatransactionID (M)

    /* IE TRP List */
    NRPPA_FIND_PROTOCOLIE_BY_ID(NRPPA_TRPInformationRequest_IEs_t, ie, container,
                                NRPPA_ProtocolIE_ID_id_TRPList, true);
//    NRPPA_TRPList_t TRP_List = ie->value.choice.TRPList;  // TODO process this information


    /* IE TRP Information Type List */
    NRPPA_FIND_PROTOCOLIE_BY_ID(NRPPA_TRPInformationRequest_IEs_t, ie, container,
                                NRPPA_ProtocolIE_ID_id_TRPInformationTypeList, true);
    //NRPPA_TRPInformationTypeList_t TRP_Info_Type_List= ie->value.choice.TRPInformationTypeList; // TODO process this information



    // Forward request to RRC
    MessageDef *msg = itti_alloc_new_message (TASK_RRC_GNB, 0, F1AP_TRP_INFORMATION_REQ);
    f1ap_trp_information_req_t *f1ap_req = &F1AP_TRP_INFORMATION_REQ(msg);
    f1ap_req->transaction_id = 0;
    f1ap_req->nrppa_msg_info.nrppa_transaction_id=nrppa_transaction_id;
    f1ap_req->nrppa_msg_info.instance=nrppa_msg_info->instance;
    f1ap_req->nrppa_msg_info.gNB_ue_ngap_id=nrppa_msg_info->gNB_ue_ngap_id;
    f1ap_req->nrppa_msg_info.amf_ue_ngap_id=nrppa_msg_info->amf_ue_ngap_id;
    f1ap_req->nrppa_msg_info.routing_id_buffer=nrppa_msg_info->routing_id_buffer;
    f1ap_req->nrppa_msg_info.routing_id_length=nrppa_msg_info->routing_id_length;

    LOG_I(NRPPA,"Forwarding to RRC TRPInformationRequest transaction_id=%d\n", f1ap_req->transaction_id);
    itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);
}


// adeel TODO fill F1AP msg for rrc

int nrppa_gNB_TRPInformationResponse(instance_t instance, MessageDef *msg_p)//(uint32_t nrppa_transaction_id, uint8_t *buffer)
{
    f1ap_trp_information_resp_t *resp = &F1AP_TRP_INFORMATION_RESP(msg_p);
    LOG_I(NRPPA, "Received TRPInformationResponse info from RRC  transaction_id=%d,  rnti= %04x\n", resp->transaction_id, resp->nrppa_msg_info.ue_rnti);

    // Prepare NRPPA TRP Information transfer Response
    NRPPA_NRPPA_PDU_t pdu;  // TODO rename
    uint8_t  *buffer= NULL;
    uint32_t  length=0;
    /* Prepare the NRPPA message to encode for successfulOutcome TRPInformationResponse */

    //IE: 9.2.3 Message Type successfulOutcome TRPInformationResponse /* mandatory */
    memset(&pdu, 0, sizeof(pdu));
    pdu.present = NRPPA_NRPPA_PDU_PR_successfulOutcome;
    asn1cCalloc(pdu.choice.successfulOutcome, head);
    head->procedureCode = NRPPA_ProcedureCode_id_tRPInformationExchange;
    head->criticality = NRPPA_Criticality_reject;
    head->value.present = NRPPA_SuccessfulOutcome__value_PR_TRPInformationResponse;

    //IE 9.2.4 nrppatransactionID  /* mandatory */
    head->nrppatransactionID =resp->nrppa_msg_info.nrppa_transaction_id;



    NRPPA_TRPInformationResponse_t *out = &head->value.choice.TRPInformationResponse;

    //IE TRP Information List
    {
        asn1cSequenceAdd(out->protocolIEs.list, NRPPA_TRPInformationResponse_IEs_t, ie);
        ie->id = NRPPA_ProtocolIE_ID_id_TRPInformationList;
        ie->criticality = NRPPA_Criticality_ignore;
        ie->value.present = NRPPA_TRPInformationResponse_IEs__value_PR_TRPInformationList;

        // TODO Retrieve TRP information from RAN Context

        int nb_of_TRP= 1;  // TODO find the acutal number for TRP and add here
        for (int i = 0; i < nb_of_TRP; i++)
        {
            asn1cSequenceAdd(ie->value.choice.TRPInformationList.list, TRPInformationList__Member, item);
            item->tRP_ID= 0;  // long NRPPA_TRP_ID_t

            //Preparing tRPInformation IE of NRPPA_TRPInformationList__Member
            int nb_tRPInfoTypes =1; // TODO find the acutal size add here
            for(int k=0; k < nb_tRPInfoTypes; k++)  //Preparing NRPPA_TRPInformation_t a list of  TRPInformation_item
            {
                asn1cSequenceAdd(item->tRPInformation.list, NRPPA_TRPInformationItem_t, trpinfo_item);

                // TODO adeel retrive relevent info and add
                trpinfo_item->choice.pCI_NR = 0; // long dummy value
                trpinfo_item->choice.sSBinformation=NULL; //dummy values
                trpinfo_item->choice.nG_RAN_CGI=NULL; //dummy values
                trpinfo_item->choice.pRSConfiguration=NULL; //dummy values
                trpinfo_item->choice.geographicalCoordinates=NULL; //dummy values

            } //for(int k=0; k < nb_tRPInfoTypes; k++)
        } //for (int i = 0; i < nb_of_TRP; i++)

    } //IE Information List

//  TODO IE 9.2.2 CriticalityDiagnostics (O)
    {
        asn1cSequenceAdd(out->protocolIEs.list, NRPPA_TRPInformationResponse_IEs_t, ie);
        ie->id = NRPPA_ProtocolIE_ID_id_CriticalityDiagnostics;
        ie->criticality = NRPPA_Criticality_ignore;
        ie->value.present = NRPPA_TRPInformationResponse_IEs__value_PR_CriticalityDiagnostics;
        // TODO Retreive CriticalityDiagnostics information and assign
        //ie->value.choice.CriticalityDiagnostics.procedureCode = ; //TODO adeel retrieve and add
        //ie->value.choice.CriticalityDiagnostics.triggeringMessage; = ; //TODO adeel retrieve and add
        //ie->value.choice.CriticalityDiagnostics.procedureCriticality; = ; //TODO adeel retrieve and add
        ie->value.choice.CriticalityDiagnostics.nrppatransactionID =resp->nrppa_msg_info.nrppa_transaction_id ;
        //ie->value.choice.CriticalityDiagnostics.iEsCriticalityDiagnostics = ; //TODO adeel retrieve and add
        //ie->value.choice.CriticalityDiagnostics.iE_Extensions = ; //TODO adeel retrieve and add
    }


    /* Encode NRPPA message */
    if (nrppa_gNB_encode_pdu(&pdu, &buffer, &length) < 0)
    {
        NRPPA_ERROR("Failed to encode Uplink NRPPa TRPInformationResponse\n");
        /* Encode procedure has failed... */
        return -1;
    }

    /* Forward the NRPPA PDU to NGAP */
    if (resp->nrppa_msg_info.gNB_ue_ngap_id >0 && resp->nrppa_msg_info.amf_ue_ngap_id >0) //( 1) // TODO
    {
        LOG_D(NRPPA, "Sending UplinkUEAssociatedNRPPa (TRPInformationResponse) to NGAP (gNB_ue_ngap_id= %d, amf_ue_ngap_id =%ld)  \n", resp->nrppa_msg_info.gNB_ue_ngap_id, resp->nrppa_msg_info.amf_ue_ngap_id);
        nrppa_gNB_itti_send_UplinkUEAssociatedNRPPa(resp->nrppa_msg_info.instance,
                resp->nrppa_msg_info.gNB_ue_ngap_id,
                resp->nrppa_msg_info.amf_ue_ngap_id,
                resp->nrppa_msg_info.routing_id_buffer,
                resp->nrppa_msg_info.routing_id_length,
                buffer, length);
        return length;
    }
    else if (resp->nrppa_msg_info.gNB_ue_ngap_id ==-1 && resp->nrppa_msg_info.amf_ue_ngap_id == -1) //
    {
        LOG_D(NRPPA, "Sending UplinkNonUEAssociatedNRPPa (TRPInformationResponse) to NGAP (gNB_ue_ngap_id= %d, amf_ue_ngap_id =%ld)  \n", resp->nrppa_msg_info.gNB_ue_ngap_id, resp->nrppa_msg_info.amf_ue_ngap_id);
        nrppa_gNB_itti_send_UplinkNonUEAssociatedNRPPa(resp->nrppa_msg_info.instance,
                resp->nrppa_msg_info.routing_id_buffer,
                resp->nrppa_msg_info.routing_id_length,
                buffer, length);
        return length;
    }
    else
    {
        NRPPA_ERROR("Failed to find context for Uplink NonUE/UE Associated NRPPa TRPInformationResponse\n");
        return -1;
    }
}


// adeel TODO fill F1AP msg for rrc
int nrppa_gNB_TRPInformationFailure(instance_t instance, MessageDef *msg_p)//(uint32_t nrppa_transaction_id, uint8_t *buffer)
{

    f1ap_trp_information_failure_t *failure_msg = &F1AP_TRP_INFORMATION_FAILURE(msg_p);
    LOG_I(NRPPA, "Received TrpInformationFailure info from RRC  transaction_id=%d,  rnti= %04x\n", failure_msg->transaction_id, failure_msg->nrppa_msg_info.ue_rnti);

    // TODO UPDATE data from F1AP Message

// Prepare NRPPA Position Information failure
    NRPPA_NRPPA_PDU_t pdu;  // TODO rename
    uint8_t  *buffer= NULL;
    uint32_t  length=0;;
    /* Prepare the NRPPA message to encode for unsuccessfulOutcome TRPInformationFailure */

    //IE: 9.2.3 Message Type unsuccessfulOutcome TRPInformationFaliure /* mandatory */
    //IE 9.2.3 Message type (M)
    memset(&pdu, 0, sizeof(pdu));
    pdu.present = NRPPA_NRPPA_PDU_PR_unsuccessfulOutcome;
    asn1cCalloc(pdu.choice.unsuccessfulOutcome, head);
    head->procedureCode = NRPPA_ProcedureCode_id_tRPInformationExchange;
    head->criticality = NRPPA_Criticality_reject;
    head->value.present = NRPPA_UnsuccessfulOutcome__value_PR_TRPInformationFailure;

    //IE 9.2.4 nrppatransactionID  /* mandatory */
    head->nrppatransactionID =failure_msg->nrppa_msg_info.nrppa_transaction_id;

    NRPPA_TRPInformationFailure_t *out = &head->value.choice.TRPInformationFailure;
// TODO IE 9.2.1 Cause (M)
    {
        asn1cSequenceAdd(out->protocolIEs.list, NRPPA_TRPInformationFailure_IEs_t, ie);
        ie->id = NRPPA_ProtocolIE_ID_id_Cause;
        ie->criticality = NRPPA_Criticality_ignore;
        ie->value.present = NRPPA_TRPInformationFailure_IEs__value_PR_Cause;
        //TODO Reteive Cause and assign
        //ie->value.choice.Cause. = ; //IE 1
        //ie->value.choice.Cause. =;  // IE 2 and so on
        /* Send a dummy cause */
//sample
//    ie->value.present = NGAP_NASNonDeliveryIndication_IEs__value_PR_Cause;
//   ie->value.choice.Cause.present = NGAP_Cause_PR_radioNetwork;
        //  ie->value.choice.Cause.choice.radioNetwork = NGAP_CauseRadioNetwork_radio_connection_with_ue_lost;
    }


//  TODO IE 9.2.2 CriticalityDiagnostics (O)
    {
        asn1cSequenceAdd(out->protocolIEs.list, NRPPA_TRPInformationFailure_IEs_t, ie);
        ie->id = NRPPA_ProtocolIE_ID_id_CriticalityDiagnostics;
        ie->criticality = NRPPA_Criticality_ignore;
        ie->value.present = NRPPA_TRPInformationFailure_IEs__value_PR_CriticalityDiagnostics;
        // TODO Retreive CriticalityDiagnostics information and assign
        //ie->value.choice.CriticalityDiagnostics.procedureCode = ; //TODO adeel retrieve and add
        //ie->value.choice.CriticalityDiagnostics.triggeringMessage; = ; //TODO adeel retrieve and add
        //ie->value.choice.CriticalityDiagnostics.procedureCriticality; = ; //TODO adeel retrieve and add
        ie->value.choice.CriticalityDiagnostics.nrppatransactionID =failure_msg->nrppa_msg_info.nrppa_transaction_id ;
        //ie->value.choice.CriticalityDiagnostics.iEsCriticalityDiagnostics = ; //TODO adeel retrieve and add
        //ie->value.choice.CriticalityDiagnostics.iE_Extensions = ; //TODO adeel retrieve and add
    }


    /* Encode NRPPA message */
    if (nrppa_gNB_encode_pdu(&pdu, &buffer, &length) < 0)
    {
        NRPPA_ERROR("Failed to encode Uplink NRPPa TRPInformationFailure \n");
        /* Encode procedure has failed... */
        return -1;
    }


    /* Forward the NRPPA PDU to NGAP */
    if(failure_msg->nrppa_msg_info.gNB_ue_ngap_id >0 && failure_msg->nrppa_msg_info.amf_ue_ngap_id >0)
    {
        LOG_D(NRPPA, "Sending UplinkUEAssociatedNRPPa (TRPInformationFailure) to NGAP (gNB_ue_ngap_id= %d, amf_ue_ngap_id =%ld)  \n", failure_msg->nrppa_msg_info.gNB_ue_ngap_id, failure_msg->nrppa_msg_info.amf_ue_ngap_id);
        nrppa_gNB_itti_send_UplinkUEAssociatedNRPPa(failure_msg->nrppa_msg_info.instance,
                failure_msg->nrppa_msg_info.gNB_ue_ngap_id,
                failure_msg->nrppa_msg_info.amf_ue_ngap_id,
                failure_msg->nrppa_msg_info.routing_id_buffer,
                failure_msg->nrppa_msg_info.routing_id_length,
                buffer, length); //tx_nrppa_pdu=buffer, nrppa_pdu_length=length
        return length;
    }
    else if (failure_msg->nrppa_msg_info.gNB_ue_ngap_id ==-1 && failure_msg->nrppa_msg_info.amf_ue_ngap_id == -1) //
    {
        LOG_D(NRPPA, "Sending UplinkNonUEAssociatedNRPPa (TRPInformationFailure) to NGAP (gNB_ue_ngap_id= %d, amf_ue_ngap_id =%ld)  \n", failure_msg->nrppa_msg_info.gNB_ue_ngap_id, failure_msg->nrppa_msg_info.amf_ue_ngap_id);
        nrppa_gNB_itti_send_UplinkNonUEAssociatedNRPPa(failure_msg->nrppa_msg_info.instance,
                failure_msg->nrppa_msg_info.routing_id_buffer,
                failure_msg->nrppa_msg_info.routing_id_length,
                buffer, length);
        return length;
    }
    else
    {
        NRPPA_ERROR("Failed to find context for Uplink NonUE/UE Associated NRPPa PositioningInformationFailure\n");

        return -1;
    }

}
