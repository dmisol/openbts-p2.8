/**@file GSM Radio Resource procedures, GSM 04.18 and GSM 04.08. */
/*
* Copyright 2008-2011 Free Software Foundation, Inc.
* Copyright 2010 Kestrel Signal Processing, Inc.
* Copyright 2011 Range Networks, Inc.
* Copyright 2012 Fairwaves LLC, Dmitri Soloviev <dmi3sol@gmail.com>
*
* This software is distributed under the terms of the GNU Affero Public License.
* See the COPYING file in the main directory for details.
*
* This use of this software may be subject to additional restrictions.
* See the LEGAL file in the main directory for details.

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU Affero General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Affero General Public License for more details.

	You should have received a copy of the GNU Affero General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef RADIORESOURCE_H
#define RADIORESOURCE_H

#include <list>
#include <GSML3CommonElements.h>


namespace GSM {
class Time;
class TCHFACCHLogicalChannel;
class L3PagingResponse;
class L3AssignmentComplete;
class TCHFACCHLogicalChannel;
class L3PhysicalInformation;
class L3HandoverComplete;
class L3HandoverFailure;
class L3MobileIdentity;
class L3MeasurementResults;
class Z100Timer;
};

namespace Control {

class TransactionEntry;




/** Find and compelte the in-process transaction associated with a paging repsonse. */
void PagingResponseHandler(const GSM::L3PagingResponse*, GSM::LogicalChannel*);

/** Find and compelte the in-process transaction associated with a completed assignment. */
void AssignmentCompleteHandler(const GSM::L3AssignmentComplete*, GSM::TCHFACCHLogicalChannel*);

/** Find and compelte the in-process transaction associated with a successful handover. */
void HandoverCompleteHandler(const GSM::L3HandoverComplete*, GSM::LogicalChannel*);

/** Find transaction and allow new handovers for it. */
void HandoverFailureHandler(const GSM::L3HandoverFailure*, GSM::LogicalChannel*);



class TransactionEntry;


/** Used to perform call setup activities at the "target" side.
 * Handover entry is created when handover is requested from outside, by means of SIP,
 *  and is removed when handover complete message is got at DCCH*/
class HandoverEntry{
	private:
	mutable Mutex mLock;
	
	int mInitialTA;
	//GSM::L3PhysicalInformation mPhysicalInformation;
	
	/** number of attempts to check against Ny1*/
	unsigned mPhysicalInfoAttempts;
	
	unsigned mNy1;
	
	/** traffic channel allocated to accept handover */
	//GSM::TCH mTCH;
	GSM::TCHFACCHLogicalChannel *mTCH;
	
	/** ether waiting for Handover Access or sending PhysicalInformation */
	bool mGotHA;
	
	/** SIP Register needed */
	bool mGotHComplete;
	
	bool mRegisterPerformed;
	
	/** what handover reference was assigned for this attempt */
	unsigned mHandoverReference;
	
	/** in this content acts as an attempt timeout*/
	GSM::Z100Timer mT3103;
	
	/** transaction stores SIP session, mobileID etc*/
	TransactionEntry* mTransaction;
	
	const char *mCallID;
	
	public:
	
	unsigned handoverReference() const { return mHandoverReference; }
	
	/** acknowledge via SIP, set timeout, change convolution in Transceiver */
	HandoverEntry(TransactionEntry* wTransaction, GSM::TCHFACCHLogicalChannel* wTCH, unsigned wHandoverReference, const char* wCallID);
	
	/** must be called from
	 * TCHFACCHL1Decoder::processBurst( const RxBurst& inBurst)
	 * file GSML1FEC.cpp
	 * 
	 * turn RACH decoding off, start sending Physical Info */
	void HandoverAccessDetected(int initialTA);
	
	/** called from Handover thread;
	 * returns TRUE if Physical Information sent */
	bool T3105Tick();
	
	/** must be called from
	 * file DCCHDispatch.cpp
	 * stop sending Physical Info, T3103 timer, try to perform SIP Register */
	void HandoverCompleteDetected();
	
	GSM::TCHFACCHLogicalChannel* channel() const { return mTCH; }
	
	/** true if performed (or attempted) */
	bool SipRegister();
	
	bool removeHandoverEntry();
	
	TransactionEntry* transaction(){ return mTransaction; }
	
	const char *callID() const { return mCallID; }
	
	void status(const char *intro);
};




class OutgoingHandover{
	public:
		OutgoingHandover(TransactionEntry* wTransaction);
		
		bool isFinished();
		
		const char *status() const;
		
		TransactionEntry * getMscTransaction()
			{ return mTransactionMSC; }
		
		void destroyTail()
			{ mDestroyTail = true; }
	private:
		GSM::Z100Timer mT3103;
		
		bool mDestroyTail;
		//bool mProxy;
		
		// to interface the switch
		TransactionEntry* mTransactionMSC;
		// to interface the chain
		TransactionEntry* mTransactionHO;
	
};


typedef std::list<HandoverEntry> HandoverEntryList;
typedef std::list<OutgoingHandover> OutgoingHandoverList;


class Handover{
	public:
		Handover();
		
		void start();
		
		/** request to accommodate a handover */
		bool addHandover(const char* CallID, const char* IMSI, unsigned l3ti, const char* callerHost, void* msg, TransactionEntry* existingTransaction);
		/** got RACH with Handover Access */
		void handoverAccess(unsigned wTN, int initialTA);
		/** got Handover Complete at DCCH */
		void handoverComplete(unsigned wTN);
		
		void showHandovers();
		

		
		/** try to push a handset to the particular site*/
		bool performHandover(const GSM::L3MobileIdentity& wSubscriber,
					string whichBTS);
		void showOutgoingHandovers();
		
		void dump(std::ostream&) const;
		
		
		/* as an option, handover decision can be taken locally.
		* If target site refuses, initial call is not disturbed in any way */
		void BTSDecision(Control::TransactionEntry *transaction, GSM::L3MeasurementResults wMeasurementResults);
		
		void removeProxy(Control::TransactionEntry *mscTransaction);
	
	private:
		unsigned allocateHandoverReference();

		HandoverEntryList mHandovers;		///< active handovers.
		OutgoingHandoverList mOutgoingHandovers;
		mutable Mutex mLock;			///< Lock for thread-safe access.
		Signal mHandoverSignal;			///< signal to wake the loop
		Thread mHandoverThread;			///< Thread for the loop.
		volatile bool mRunning;
	
		unsigned mHandoverReference;		///< variable for allocating new referencies		
	
		/** !! Attention: in usec needed here */
		unsigned mT3105;			///< a value in usecs to sleep between sending PhysicalInfo
	
		friend void *HandoverServiceLoop(Handover *);
	
		void handoverHandler();
		
		
		
		
		/* stuff needed to take a decision locally */
			/* a permission to take a decision locally, based on MS measurements */
			bool mBTSDesicion;
			
			/* must follow the order of ARFCNs in GSM.CellSelection.Neighbors */
			std::vector <std::string> mNeighborAddresses;
			
			/* just for reference. and used in constructor to fill mNeighborAddresses*/
			std::vector <unsigned> mNeighborArfcns;
		
};

void *HandoverServiceLoop(Handover *);


/**@ Access Grant mechanisms */
//@{


/** Decode RACH bits and send an immediate assignment; may block waiting for a channel. */
//void AccessGrantResponder(
//	unsigned requestReference, const GSM::Time& when,
//	float RSSI, float timingError);


/** This record carries all of the parameters associated with a RACH burst. */
class ChannelRequestRecord {

	private:

	unsigned mRA;		///< request reference
	GSM::Time mFrame;	///< receive timestamp
	float mRSSI;		///< dB wrt full scale
	float mTimingError;	///< correlator timing error in symbol periods

	public:

	ChannelRequestRecord(
		unsigned wRA, const GSM::Time& wFrame,
		float wRSSI, float wTimingError)
		:mRA(wRA), mFrame(wFrame),
		mRSSI(wRSSI), mTimingError(wTimingError)
	{ }

	unsigned RA() const { return mRA; }
	const GSM::Time& frame() const { return mFrame; }
	float RSSI() const { return mRSSI; }
	float timingError() const { return mTimingError; }

};


/** A thread to process contents of the channel request queue. */
void* AccessGrantServiceLoop(void*);


//@}

/**@ Paging mechanisms */
//@{

/** An entry in the paging list. */
class PagingEntry {

	private:

	GSM::L3MobileIdentity mID;		///< The mobile ID.
	GSM::ChannelType mType;			///< The needed channel type.
	unsigned mTransactionID;		///< The associated transaction ID.
	Timeval mExpiration;			///< The expiration time for this entry.

	public:

	/**
		Create a new entry, with current timestamp.
		@param wID The ID to be paged.
		@param wLife The number of milliseconds to keep paging.
	*/
	PagingEntry(const GSM::L3MobileIdentity& wID, GSM::ChannelType wType,
			unsigned wTransactionID, unsigned wLife)
		:mID(wID),mType(wType),mTransactionID(wTransactionID),mExpiration(wLife)
	{}

	/** Access the ID. */
	const GSM::L3MobileIdentity& ID() const { return mID; }

	/** Access the channel type needed. */
	GSM::ChannelType type() const { return mType; }

	unsigned transactionID() const { return mTransactionID; }

	/** Renew the timer. */
	void renew(unsigned wLife) { mExpiration = Timeval(wLife); }

	/** Returns true if the entry is expired. */
	bool expired() const { return mExpiration.passed(); }

};

typedef std::list<PagingEntry> PagingEntryList;


/**
	The pager is a global object that generates paging messages on the CCCH.
	To page a mobile, add the mobile ID to the pager.
	The entry will be deleted automatically when it expires.
	All pager operations are linear time.
	Not much point in optimizing since the main operation is inherently linear.
*/
class Pager {

	private:

	PagingEntryList mPageIDs;				///< List of ID's to be paged.
	mutable Mutex mLock;					///< Lock for thread-safe access.
	Signal mPageSignal;						///< signal to wake the paging loop
	Thread mPagingThread;					///< Thread for the paging loop.
	volatile bool mRunning;

	public:

	Pager()
		:mRunning(false)
	{}

	/** Set the output FIFO and start the paging loop. */
	void start();

	/**
		Add a mobile ID to the paging list.
		@param addID The mobile ID to be paged.
		@param chanType The channel type to be requested.
		@param transaction The transaction record, which will be modified.
		@param wLife The paging duration in ms, default is SIP Timer B.
	*/
	void addID(
		const GSM::L3MobileIdentity& addID,
		GSM::ChannelType chanType,
		TransactionEntry& transaction,
		unsigned wLife=gConfig.getNum("SIP.Timer.B")
	);

	/**
		Remove a mobile ID.
		This is used to stop the paging when a phone responds.
		@return The transaction ID associated with this entry.
	*/
	unsigned removeID(const GSM::L3MobileIdentity&);

	private:

	/**
		Traverse the paging list, paging all IDs.
		@return Number of IDs paged.
	*/
	unsigned pageAll();

	/** A loop that repeatedly calls pageAll. */
	void serviceLoop();

	/** C-style adapter. */
	friend void *PagerServiceLoopAdapter(Pager*);

public:

	/** return size of PagingEntryList */
	size_t pagingEntryListSize();

	/** Dump the paging list to an ostream. */
	void dump(std::ostream&) const;
};


void *PagerServiceLoopAdapter(Pager*);


//@}	// paging mech

}


#endif
