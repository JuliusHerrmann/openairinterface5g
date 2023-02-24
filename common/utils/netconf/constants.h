#pragma once

// constants to set
#define MANAGED_ELEMENT_ID "OAI gNodeB"
#define MANAGED_ELEMENT_GNBDUFUNCTION_ID "OAI-DU"
#define MANAGED_ELEMENT_GNBCUCPFUNCTION_ID "OAI-CUCP"
#define MANAGED_ELEMENT_GNBCUUPFUNCTION_ID "OAI-CUUP"
#define MANAGED_ELEMENT_GNBDUFUNCTION_BWDUPLINK_NAME "Uplink"
#define MANAGED_ELEMENT_GNBDUFUNCTION_BWDDOWNLINK_NAME "Downlink"


// generated xpaths out of the constants
#define XPATH_MANAGED_ELEMENT_BASE1 "/_3gpp-common-managed-element:ManagedElement[id='" MANAGED_ELEMENT_ID
#define XPATH_MANAGED_ELEMENT_BASE XPATH_MANAGED_ELEMENT_BASE1 "']"
#define XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BASE1 XPATH_MANAGED_ELEMENT_BASE "/_3gpp-nr-nrm-gnbdufunction:GNBDUFunction[id='"
#define XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BASE2 XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BASE1 MANAGED_ELEMENT_GNBDUFUNCTION_ID
#define XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BASE XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BASE2 "']"

#define XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPDOWNLINK_BASE1 XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BASE "/_3gpp-nr-nrm-bwp:BWP[id='"
#define XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPDOWNLINK_BASE2 XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPDOWNLINK_BASE1 MANAGED_ELEMENT_GNBDUFUNCTION_BWDDOWNLINK_NAME
#define XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPDOWNLINK_BASE XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPDOWNLINK_BASE2 "']"
#define XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPDOWNLINK_ATTRIBUTES_BASE XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPDOWNLINK_BASE "/attributes"
#define XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPDOWNLINK_ATTRIBUTES_BWPCONTEXT XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPDOWNLINK_ATTRIBUTES_BASE "/bwpContext"
#define XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPDOWNLINK_ATTRIBUTES_ISINITIALBWP XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPDOWNLINK_ATTRIBUTES_BASE "/isInitialBwp"
#define XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPDOWNLINK_ATTRIBUTES_SUBCARRIERSPACING XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPDOWNLINK_ATTRIBUTES_BASE "/subCarrierSpacing"
#define XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPDOWNLINK_ATTRIBUTES_CYCLICPREFIX XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPDOWNLINK_ATTRIBUTES_BASE "/cyclicPrefix"
#define XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPDOWNLINK_ATTRIBUTES_STARTRB XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPDOWNLINK_ATTRIBUTES_BASE "/startRB"
#define XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPDOWNLINK_ATTRIBUTES_NUMBEROFRBS XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPDOWNLINK_ATTRIBUTES_BASE "/numberOfRBs"


#define XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPUPLINK_BASE1 XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BASE "/_3gpp-nr-nrm-bwp:BWP[id='"
#define XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPUPLINK_BASE2 XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPUPLINK_BASE1 MANAGED_ELEMENT_GNBDUFUNCTION_BWDUPLINK_NAME
#define XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPUPLINK_BASE XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPUPLINK_BASE2 "']"
#define XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPUPLINK_ATTRIBUTES_BASE XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPUPLINK_BASE "/attributes"
#define XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPUPLINK_ATTRIBUTES_BWPCONTEXT XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPUPLINK_ATTRIBUTES_BASE "/bwpContext"
#define XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPUPLINK_ATTRIBUTES_ISINITIALBWP XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPUPLINK_ATTRIBUTES_BASE "/isInitialBwp"
#define XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPUPLINK_ATTRIBUTES_SUBCARRIERSPACING XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPUPLINK_ATTRIBUTES_BASE "/subCarrierSpacing"
#define XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPUPLINK_ATTRIBUTES_CYCLICPREFIX XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPUPLINK_ATTRIBUTES_BASE "/cyclicPrefix"
#define XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPUPLINK_ATTRIBUTES_STARTRB XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPUPLINK_ATTRIBUTES_BASE "/startRB"
#define XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPUPLINK_ATTRIBUTES_NUMBEROFRBS XPATH_MANAGED_ELEMENT_GNBDUFUNCTION_BWPUPLINK_ATTRIBUTES_BASE "/numberOfRBs"

