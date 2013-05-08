/**@file Declarations for TransactionTable and related classes. */
/*
* Copyright 2010 Kestrel Signal Processing, Inc.
* Copyright 2011, 2012 Range Networks, Inc.
* Copyright 2008-2011 Free Software Foundation, Inc.
* Copyright 2012 Fairwaves LLC, Dmitri Soloviev <dmi3sol@gmail.com>
*
* This use of this software may be subject to additional restrictions.
* See the LEGAL file in the main directory for details.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

*/



#ifndef TRANSACTIONTABLE_H
#define TRANSACTIONTABLE_H


#include <stdio.h>
#include <list>

#include <Logger.h>
#include <Interthread.h>
#include <Timeval.h>
#include <Sockets.h>

//#include <osip2/osip.h>

#include <GSML3CommonElements.h>
#include <GSML3MMElements.h>
#include <GSML3CCElements.h>
#include <GSML3RRElements.h>
#include <SIPEngine.h>

namespace GSM {
class LogicalChnanel;
class SACCHLogicalChannel;
}

#include "RadioResource.h"



struct sqlite3;


/**@namespace Control This namepace is for use by the control layer. */
namespace Control {

class HandoverEntry;

typedef std::map<std::string, GSM::Z100Timer> TimerTable;




/**
	A TransactionEntry object is used to maintain the state of a transaction
	as it moves from channel to channel.
	The object itself is not thread safe.
*/
class TransactionEntry {

	private:
	
#define MinimalMeasuredValue	-110

	mutable Mutex mLock;					///< thread-safe control, shared from gTransactionTable

	/**@name Stable variables, fixed in the constructor or written only once. */
	//@{
	unsigned mID;							///< the internal transaction ID, assigned by a TransactionTable

	GSM::L3MobileIdentity mSubscriber;		///< some kind of subscriber ID, preferably IMSI
	GSM::L3CMServiceType mService;			///< the associated service type
	unsigned mL3TI;							///< the L3 short transaction ID, the version we *send* to the MS

	GSM::L3CalledPartyBCDNumber mCalled;	///< the associated called party number, if known
	GSM::L3CallingPartyBCDNumber mCalling;	///< the associated calling party number, if known

	// TODO -- This should be expaned to deal with long messages.
	//char mMessage[522];						///< text messaging payload
	std::string mMessage;					///< text message payload
	std::string mContentType;				///< text message payload content type
	//@}

	SIP::SIPEngine mSIP;					///< the SIP IETF RFC-3621 protocol engine
	mutable SIP::SIPState mPrevSIPState;	///< previous SIP state, prior to most recent transactions
	GSM::CallState mGSMState;				///< the GSM/ISDN/Q.931 call state
	Timeval mStateTimer;					///< timestamp of last state change.
	TimerTable mTimers;						///< table of Z100-type state timers
	
	// if there is a handover attempt, while IMSI is known here already
	// this means loop must be removed after ho succeeds
	TransactionEntry * mExistingTransaction;

	unsigned mNumSQLTries;					///< number of SQL tries for DB operations

	GSM::LogicalChannel *mChannel;			///< current channel of the transaction

	bool mTerminationRequested;
	
	bool mProxyTransaction;
	
	TransactionEntry *mOldTransaction;	// a link to original call, used for outgoing handovers
	
	bool mHOAllowed;	// to prevent from several handover attempts for a single call

	volatile bool mRemoved;			///< true if ready for removal

	bool mFake;					///true if this is a fake message generated internally	

	std::vector <int> mAveragedMeasurements;

	public:

	/** This form is used for MTC or MT-SMS with TI generated by the network. */
	TransactionEntry(const char* proxy,
		const GSM::L3MobileIdentity& wSubscriber, 
		GSM::LogicalChannel* wChannel,
		const GSM::L3CMServiceType& wService,
		const GSM::L3CallingPartyBCDNumber& wCalling,
		GSM::CallState wState = GSM::NullState,
		const char *wMessage = NULL,
		bool wFake=false);

	/** This form is used for MOC, setting mGSMState to MOCInitiated. */
	TransactionEntry(const char* proxy,
		const GSM::L3MobileIdentity& wSubscriber,
		GSM::LogicalChannel* wChannel,
		const GSM::L3CMServiceType& wService,
		unsigned wL3TI,
		const GSM::L3CalledPartyBCDNumber& wCalled);

	/** This form is used for SOS calls, setting mGSMState to MOCInitiated. */
	TransactionEntry(const char* proxy,
		const GSM::L3MobileIdentity& wSubscriber,
		GSM::LogicalChannel* wChannel,
		const GSM::L3CMServiceType& wService,
		unsigned wL3TI);

	/** Form for MO-SMS; sets yet-unknown TI to 7 and GSM state to SMSSubmitting */
	TransactionEntry(const char* proxy,
		const GSM::L3MobileIdentity& wSubscriber,
		GSM::LogicalChannel* wChannel,
		const GSM::L3CalledPartyBCDNumber& wCalled,
		const char* wMessage);

	/** Form for MO-SMS with a parallel call; sets yet-unknown TI to 7 and GSM state to SMSSubmitting */
	TransactionEntry(const char* proxy,
		const GSM::L3MobileIdentity& wSubscriber,
		GSM::LogicalChannel* wChannel);

	/** Form for "handover-originated" calls */
	TransactionEntry(const char* proxy,
		const GSM::L3MobileIdentity& wSubscriber,
		GSM::LogicalChannel* wChannel,
		unsigned wL3TI,
		const GSM::L3CMServiceType& wService, TransactionEntry * wExistingTransaction);
//	void addHandoverEntry(HandoverEntry* wHandoverEntry);
	
	/** Form for "temporary" transaction to support outgoing handover*/
	TransactionEntry(TransactionEntry *wOldTransaction, 
		const GSM::L3MobileIdentity& wSubscriber,
		string whichBTS,
		unsigned wL3TI, string wDRTPIp, short wDRTPPort, unsigned wCodec);
	
	/** Delete the database entry upon destruction. */
	~TransactionEntry();
		
	/**@name Accessors. */
	//@{
	unsigned L3TI() const;
	void L3TI(unsigned wL3TI);

	const GSM::LogicalChannel* channel() const;
	GSM::LogicalChannel* channel();

	void channel(GSM::LogicalChannel* wChannel);

	const GSM::L3MobileIdentity& subscriber() const { return mSubscriber; }

	const GSM::L3CMServiceType& service() const { return mService; }

	const GSM::L3CalledPartyBCDNumber& called() const { return mCalled; }
	void called(const GSM::L3CalledPartyBCDNumber&);

	const GSM::L3CallingPartyBCDNumber& calling() const { return mCalling; }

	bool fake() const {return mFake; }

	const char* message() const { return mMessage.c_str(); }
	void message(const char *wMessage, size_t length);
	const char* messageType() const { return mContentType.c_str(); }
	void messageType(const char *wContentType);

	unsigned ID() const { return mID; }

	GSM::CallState GSMState() const;
	void GSMState(GSM::CallState wState);

	// needed to create a temporary transaction for outgoing handover
	short destRTPPort() const { return mSIP.destRTPPort(); }
	char* destRTPIp() { return mSIP.destRTPIp(); }
	short codec() const { return mSIP.codec(); }
	TransactionEntry* callingTransaction() { return mOldTransaction; }
	
	TransactionEntry* existingTransaction() { return mExistingTransaction; }
	
	void cutHandoverTail(GSM::LogicalChannel* wChannel);
	//@}


	/** Initiate the termination process. */
	void terminate() { ScopedLock lock(mLock); mTerminationRequested=true; }

	bool terminationRequested();

	/**@name SIP-side operations */
	//@{

	SIP::SIPState SIPState() { ScopedLock lock(mLock); return mSIP.state(); } 

	bool SIPFinished() { ScopedLock lock(mLock); return mSIP.finished(); } 

	bool instigator() { ScopedLock lock(mLock); return mSIP.instigator(); }

	SIP::SIPState MOCSendINVITE(const char* calledUser, const char* calledDomain, short rtpPort, unsigned codec);
	SIP::SIPState MOCResendINVITE();
	SIP::SIPState MOCCheckForOK();
	SIP::SIPState MOCSendACK();
	void MOCInitRTP() { ScopedLock lock(mLock); return mSIP.MOCInitRTP(); }

	SIP::SIPState SOSSendINVITE(short rtpPort, unsigned codec);
	SIP::SIPState SOSResendINVITE() { return MOCResendINVITE(); }
	SIP::SIPState SOSCheckForOK() { return MOCCheckForOK(); }
	SIP::SIPState SOSSendACK() { return MOCSendACK(); }
	void SOSInitRTP() { MOCInitRTP(); }

	// few functions below are needed to serve outgoing handover inside a 
	// "temporary" transaction
		SIP::SIPState HOSendINVITE(string whichBTS);
		
		// send 200 Ok  for re-invite from handover chain
		void HOSendOK(osip_message_t * msg);
		SIP::SIPState HOSendACK();
		SIP::SIPState HOSendREINVITE(char *ip, short port, unsigned codec);
	
	// get a raw message for CallID
	osip_message_t * HOGetSIPMessage();
	int HOGetSIPResponse();

	// outgoing handover setup logic to be used inside handover thread
	bool HOSetupFinished();

	void HOTurnToProxy();
	
	SIP::SIPState MTCSendTrying();
	SIP::SIPState MTCSendRinging();
	SIP::SIPState MTCCheckForACK();
	SIP::SIPState MTCCheckForCancel();
	SIP::SIPState MTCSendOK(short rtpPort, unsigned codec);
	void MTCInitRTP() { ScopedLock lock(mLock); mSIP.MTCInitRTP(); }

	SIP::SIPState MODSendBYE();
	SIP::SIPState MODSendERROR(osip_message_t * cause, int code, const char * reason, bool cancel);
	SIP::SIPState MODSendCANCEL();
	SIP::SIPState MODResendBYE();
	SIP::SIPState MODResendCANCEL();
	SIP::SIPState MODResendERROR(bool cancel);
	SIP::SIPState MODWaitForBYEOK();
	SIP::SIPState MODWaitForCANCELOK();
	SIP::SIPState MODWaitForERRORACK(bool cancel);
	SIP::SIPState MODWaitFor487();
	SIP::SIPState MODWaitForResponse(vector<unsigned> *validResponses);

	SIP::SIPState MTDCheckBYE();
	SIP::SIPState MTDSendBYEOK();
	SIP::SIPState MTDSendCANCELOK();

	// TODO: Remove contentType from here and use the setter above.
	SIP::SIPState MOSMSSendMESSAGE(const char* calledUser, const char* calledDomain, const char* contentType);
	SIP::SIPState MOSMSWaitForSubmit();

	SIP::SIPState MTSMSSendOK();

	bool sendINFOAndWaitForOK(unsigned info);
	
	void sendINFO(const char * measurements);

	void txFrame(unsigned char* frame) { ScopedLock lock(mLock); return mSIP.txFrame(frame); }
	int rxFrame(unsigned char* frame) { ScopedLock lock(mLock); return mSIP.rxFrame(frame); }
	bool startDTMF(char key) { ScopedLock lock(mLock); return mSIP.startDTMF(key); }
	void stopDTMF() { ScopedLock lock(mLock); mSIP.stopDTMF(); }

	void SIPUser(const std::string& IMSI) { ScopedLock lock(mLock); SIPUser(IMSI.c_str()); }
	void SIPUser(const char* IMSI);
	void SIPUser(const char* callID, const char *IMSI , const char *origID, const char *origHost);

	const std::string SIPCallID() const { ScopedLock lock(mLock); return mSIP.callID(); }

	
	/** acknowledge handover initiation; publish handoverReference + cellId + chanId */
	SIP::SIPState HOCSendHandoverAck(unsigned handoverReference, unsigned BCC, unsigned NCC, unsigned C0, const char *channelDescription);
	/** drop handover-originated "call setup" */
	SIP::SIPState HOCTimeout();
	/** complete handover-originated "call setup" and provide rtp endpoint*/
	SIP::SIPState HOCSendOK(short rtpPort, unsigned codec);
	
	// Send Handover Command to move the current call
	void HOSendHandoverCommand(GSM::L3CellDescription wCell, GSM::L3ChannelDescription wChan, unsigned wHandoverReference);
	
	bool proxyTransaction() { return mProxyTransaction; }
	
	bool handoverTarget(char *cell, char *chan , unsigned *reference) 
		{ return mSIP.handoverTarget(cell, chan , reference);}
	bool reinviteTarget(char *ip, char *port, unsigned *codec)
		{ return mSIP.reinviteTarget(ip, port , codec);}
	bool reinviteTarget(osip_message_t * msg, char *ip, char *port, unsigned *codec)
		{ return mSIP.reinviteTarget(msg, ip, port , codec);}
	
	void HOProxy_forward_msg(osip_message_t *event);
	
	/* re-transmit SIP INFO threw handover chain*/
	void HOCSendINFO(void *element);
	
	// sending BYE directly to BTS, not to proxy;
	// signs BYE that just flips the loop
	void HOSendBYE(bool flip_loop);
	
	void HOSendBYEOK()
		{ mSIP.HOSendBYEOK(); }
	
	void handoverFailed()
		{ mHOAllowed = true; }
	
	bool handoverAllowed()
		{ return mHOAllowed; }
	
	bool handoverLock();
	
	// FIXME: low pass filtering must be implemented
	vector <int> average(GSM::L3MeasurementResults wMeasurementResults, double wWeights);
	
	void resetMeasurement(unsigned index)
		{ mAveragedMeasurements[index] = MinimalMeasuredValue; }
	
	// These are called by SIPInterface.
	void saveINVITE(const osip_message_t* invite, bool local)
		{ ScopedLock lock(mLock); mSIP.saveINVITE(invite,local); }
	void saveBYE(const osip_message_t* bye, bool local)
		{ ScopedLock lock(mLock); mSIP.saveBYE(bye,local); }

	bool sameINVITE(osip_message_t * msg)
		{ ScopedLock lock(mLock); return mSIP.sameINVITE(msg); }

	//@}

	unsigned stateAge() const { ScopedLock lock(mLock); return mStateTimer.elapsed(); }

	/**@name Timer access. */
	//@{

	bool timerExpired(const char* name) const;

	void setTimer(const char* name);

	void setTimer(const char* name, long newLimit);

	void resetTimer(const char* name);

	/** Return true if any Q.931 timer is expired. */
	bool anyTimerExpired() const;

	/** Reset all Q.931 timers. */
	void resetTimers();
	
	//@}

	/** Return true if clearing is in progress in the GSM side. */
	bool clearingGSM() const;

	/** Retrns true if the transaction is "dead". */
	bool dead() const;

	/** Returns true if dead, or if removal already requested. */
	bool deadOrRemoved() const;

	/** Dump information as text for debugging. */
	void text(std::ostream&) const;

	private:

	friend class TransactionTable;

	/** Create L3 timers from GSM and Q.931 (network side) */
	void initTimers();

	/** Set up a new entry in gTransactionTable's sqlite3 database. */
	void insertIntoDatabase();

	/** Run a database query. */
	void runQuery(const char* query) const;

	/** Echo latest SIPSTATE to the database. */
	SIP::SIPState echoSIPState(SIP::SIPState state) const;

	/** Tag for removal. */
	void remove() { mRemoved=true; mStateTimer.now(); }

	/** Removal status. */
	bool removed() { return mRemoved; }
};


std::ostream& operator<<(std::ostream& os, const TransactionEntry&);


/** A map of transactions keyed by ID. */
class TransactionMap : public std::map<unsigned,TransactionEntry*> {};

/**
	A table for tracking the states of active transactions.
*/
class TransactionTable {

	private:

	sqlite3 *mDB;			///< database connection

	TransactionMap mTable;
	mutable Mutex mLock;
	unsigned mIDCounter;

	public:

	/**
		Initialize a transaction table.
		@param path Path fto sqlite3 database file.
	*/
	void init(const char* path);

	~TransactionTable();

	// TransactionTable does not need a destructor.

	/**
		Return a new ID for use in the table.
	*/
	unsigned newID();

	/**
		Insert a new entry into the table; deleted by the table later.
		@param value The entry to insert into the table; will be deleted by the table later.
	*/
	void add(TransactionEntry* value);

	/**
		Find an entry and return a pointer into the table.
		@param wID The transaction ID to search
		@return NULL if ID is not found or was dead
	*/
	TransactionEntry* find(unsigned wID);

	TransactionEntry* findLegacyTransaction(const GSM::L3MobileIdentity& mobileID);
	/**
		Find the longest-running non-SOS call.
		@return NULL if there are no calls or if all are SOS.
	*/
	TransactionEntry* findLongestCall();

	/**
		Return the availability of this particular RTP port
		@return True if Port is available, False otherwise
	*/
	bool RTPAvailable(short rtpPort);

	/**
		Remove an entry from the table and from gSIPMessageMap.
		@param wID The transaction ID to search.
		@return True if the ID was really in the table and deleted.
	*/
	bool remove(unsigned wID);

	bool remove(TransactionEntry* transaction) { return remove(transaction->ID()); }

	/**
		Remove an entry from the table and from gSIPMessageMap,
		if it is in the Paging state.
		@param wID The transaction ID to search.
		@return True if the ID was really in the table and deleted.
	*/
	bool removePaging(unsigned wID);


	/**
		Find an entry by its channel pointer; returns first entry found.
		Also clears dead entries during search.
		@param chan The channel pointer.
		@return pointer to entry or NULL if no active match
	*/
	TransactionEntry* find(const GSM::LogicalChannel *chan);

	/**
		Find an entry by its SACCH channel pointer; returns first entry found.
		Also clears dead entries during search.
		@param chan The channel pointer.
		@return pointer to entry or NULL if no active match
	*/
	TransactionEntry* findBySACCH(const GSM::SACCHLogicalChannel *chan);

	/**
		Find an entry by its channel type and offset.
		Also clears dead entries during search.
		@param chan The channel pointer to the first record found.
		@return pointer to entry or NULL if no active match
	*/
	TransactionEntry* find(GSM::TypeAndOffset chanDesc);

	/**
		Find an entry in the given state by its mobile ID.
		Also clears dead entries during search.
		@param mobileID The mobile to search for.
		@return pointer to entry or NULL if no match
	*/
	TransactionEntry* find(const GSM::L3MobileIdentity& mobileID, GSM::CallState state);

	/** Return true if there is an ongoing call for this user. */
	bool isBusy(const GSM::L3MobileIdentity& mobileID);


	/** Find by subscriber and SIP call ID. */
	TransactionEntry* find(const GSM::L3MobileIdentity& mobileID, const char* callID);

	/** Find by subscriber and handover other BS transaction ID. */
	TransactionEntry* find(const GSM::L3MobileIdentity& mobileID, unsigned transactionID);

	/** Check for duplicated SMS delivery attempts. */
	bool duplicateMessage(const GSM::L3MobileIdentity& mobileID, const std::string& wMessage);

	/**
		Find an entry in the Paging state by its mobile ID, change state to AnsweredPaging and reset T3113.
		Also clears dead entries during search.
		@param mobileID The mobile to search for.
		@return pointer to entry or NULL if no match
	*/
	TransactionEntry* answeredPaging(const GSM::L3MobileIdentity& mobileID);


	/**
		Find the channel, if any, used for current transactions by this mobile ID.
		@param mobileID The target mobile subscriber.
		@return pointer to TCH/FACCH, SDCCH or NULL.
	*/
	GSM::LogicalChannel* findChannel(const GSM::L3MobileIdentity& mobileID);

	/** Count the number of transactions using a particular channel. */
	unsigned countChan(const GSM::LogicalChannel*);

	size_t size() { ScopedLock lock(mLock); return mTable.size(); }

	size_t dump(std::ostream& os, bool showAll=false) const;

	private:

	friend class TransactionEntry;

	/** Accessor to database connection. */
	sqlite3* DB() { return mDB; }

	/**
		Remove "dead" entries from the table.
		A "dead" entry is a transaction that is no longer active.
		The caller should hold mLock.
	*/
	void clearDeadEntries();

	/**
		Remove and entry from the table and from gSIPInterface.
	*/
	void innerRemove(TransactionMap::iterator);


};





}	//Control



/**@addtogroup Globals */
//@{
/** A single global transaction table in the global namespace. */
extern Control::TransactionTable gTransactionTable;
//@}



#endif

// vim: ts=4 sw=4


