#ifndef ns_pathChirp_h
#define ns_pathChirp_h

#include "agent.h"
#include "tclcl.h"
#include "packet.h"
#include "address.h"
#include "ip.h"

#define EXP 1
#define LIN 2

class pathChirpAgentRcv;

struct hdr_pathChirpSnd {
	int pktnum_;
	int packetSize_;
	double send_time;
	double lowrate_;
	double spreadfactor_;
	int num_pkts_per_chirp;
	int chirpType;
	double increment;
	// Header access methods
	static int offset_; // required by PacketHeaderManager
	inline static int& offset() { return offset_; }
	inline static hdr_pathChirpSnd* access(const Packet* p) {
		return (hdr_pathChirpSnd*) p->access(offset_);
	}
};

struct hdr_pathChirpRcv {
	int loss;
	double inst_bw;

	// Header access methods
	static int offset_; // required by PacketHeaderManager
	inline static int& offset() { return offset_; }
	inline static hdr_pathChirpRcv* access(const Packet* p) {
		return (hdr_pathChirpRcv*) p->access(offset_);
	}
};



class pathChirpAgentRcv : public Agent {
 public:
	pathChirpAgentRcv();
 protected:
	virtual void start();
	virtual void resetting();
	virtual int command(int argc, const char*const* argv);
	virtual void recv(Packet*, Handler*);

	virtual void write_instant_bw();

	virtual void sendpkt(double,int);

	virtual double computeavbw();
	virtual void allocatemem();
	void allocatemem1();
	void allocatemem2();

	double lowrate_;
	double spreadfactor_;
	int running_;
	int pktnum_;
	int prevpktnum_;
	int packetSize_;

	int num_interarrival;

	double total_inst_bw;
	int inst_head;
	int num_inst_bw;
	int busy_period_thresh;
	int decrease_factor;
	int congestionflag_;
	long inst_bw_count;
	double *inst_bw_estimates;
	double *qing_delay;
	double *rates;
	double *av_bw_per_pkt;
	double *iat;
	
	int chirpType;
	double increment;
	
	int pkt_sum; //接受到的包总数

};

#endif


