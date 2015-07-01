
#ifndef ns_pathChirp_h
#define ns_pathChirp_h

#include "agent.h"
#include "tclcl.h"
#include "packet.h"
#include "address.h"
#include "ip.h"

#define EXP 1
#define LIN 2

class pathChirpAgentSnd;

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


class pathChirpAgentSndTimer : public TimerHandler {
public:
	pathChirpAgentSndTimer(pathChirpAgentSnd *t) : TimerHandler() { t_ = t; }
protected:
	virtual void expire(Event *e);
	pathChirpAgentSnd *t_;
};



class pathChirpAgentSnd : public Agent {
 public:
	pathChirpAgentSnd();
	virtual void timeout();
	void timeout1();
	void timeout2();
 protected:
	virtual void start();
	virtual void stop();
	virtual void sendpkt();
	virtual void resetting();
	virtual int command(int argc, const char*const* argv);
	virtual void recv(Packet*, Handler*);

	inline double next1(double);
	inline double next2(double);
	inline double computeinterchirpgap1();
	inline double computeinterchirpgap2();
	inline double chooseIncrement(double bandwidth);
	inline void	  adjust1(hdr_pathChirpRcv* hdr );
	inline void	  adjust2(hdr_pathChirpRcv* hdr );
	double lowrate_;
	double avgrate_;
	double highrate_;
	double newlowrate_;
	double newavgrate_;
	double newhighrate_;
	double spreadfactor_;
	double interchirpgap_;
	
	double a_;
	double m_;
	int n_;
	
	int running_;
	int pktnum_;
	int packetSize_;
	double curriat_;
	int num_pkts_per_chirp;
	int lowcount;
	int losscount;
	int highcount;
	int resetflag;
	
	double lastInstBw;
	double increment;
	double newIncrement;
	int chirpType;
	int newChirpType;
	int midcount;
	
	
	int complete_count;
	float inst_array[10];
	
	pathChirpAgentSndTimer timer_;
};

#endif


