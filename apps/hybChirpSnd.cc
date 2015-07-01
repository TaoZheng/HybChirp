#include "pathChirpSnd.h"

int hdr_pathChirpSnd::offset_;

static class pathChirpHeaderClass : public PacketHeaderClass {
public:
	pathChirpHeaderClass() : PacketHeaderClass("PacketHeader/pathChirpSnd", 
					      sizeof(hdr_pathChirpSnd)) {
		bind_offset(&hdr_pathChirpSnd::offset_);
	}
} class_pathChirphdrSnd;

/*int hdr_pathChirpRcv::offset_;

static class pathChirpHeaderClass : public PacketHeaderClass {
public:
	pathChirpHeaderClass() : PacketHeaderClass("PacketHeader/pathChirpRcv", 
					      sizeof(hdr_pathChirpRcv)) {
		bind_offset(&hdr_pathChirpRcv::offset_);
	}
} class_pathChirphdrRcv;
*/

static class pathChirpSndClass : public TclClass {
public:
	pathChirpSndClass() : TclClass("Agent/pathChirpSnd") {}
	TclObject* create(int, const char*const*) {
		return (new pathChirpAgentSnd());
	}
} class_pathChirpSnd;

/* constructor*/
pathChirpAgentSnd::pathChirpAgentSnd() : Agent(PT_PATHCHIRPSND),running_(0), timer_(this)
{  	
    
      bind("packetSize_", &size_);
      bind("lowrate_", &lowrate_);
      bind("highrate_", &highrate_);
      bind("spreadfactor_", &spreadfactor_);
      bind("avgrate_", &avgrate_);
      bind("a_", &a_);
      bind("m_", &m_);
      bind("n_", &n_);
      
      complete_count = 0;
      
      int i;
      for (i=0; i<10; i++)
      	inst_array[i] = 0.0;
}


/* send out a packet with timestamp and appropriate  packet number */
void pathChirpAgentSnd::sendpkt()
{

      // Create a new packet
      Packet* pkt = allocpkt();
      

      // Access the pathChirp header for the new packet:
      hdr_pathChirpSnd* hdr = hdr_pathChirpSnd::access(pkt);

      // Store the current time in the 'send_time' field
      hdr->send_time = Scheduler::instance().clock();
      //hdr->size=packetSize_;
      hdr->pktnum_ = pktnum_;

      hdr->lowrate_ = lowrate_;
      hdr->spreadfactor_ = spreadfactor_;
      hdr->num_pkts_per_chirp = num_pkts_per_chirp;
      hdr->packetSize_ = packetSize_;
	  hdr->chirpType = chirpType;
	  hdr->increment = increment;
      // Send the packet
      send(pkt, 0);
      //   return (TCL_OK);
      return;
}
     
/* if timer expires just do a timeout */
void pathChirpAgentSndTimer::expire(Event*)
{
        t_->timeout();
}

/* initialization */
void pathChirpAgentSnd::start()
{
        
        packetSize_=size_;
        running_ = 1;
	pktnum_ = 0;


	if ((lowrate_>highrate_) || (lowrate_<0) || (highrate_<0) || (avgrate_<0))
	  {
	    fprintf(stderr,"pathChirpSnd: error in rate specification\n");
	    exit(0);
	  }
	if (spreadfactor_<=1.0)
	  {
	    fprintf(stderr,"pathChirpSnd: spreadfactor_ too low\n");
	    exit(0);
	  }

	interchirpgap_=computeinterchirpgap1();
	if (interchirpgap_<0)
	  interchirpgap_=0;

	lowcount=0;
	highcount=0;
	losscount=0;
	midcount=0;
	resetflag=0;
	
	lastInstBw=-1000.0;//初始定为负值，只要被赋过新值就变成正值
	increment=0.0;
	newIncrement=0.0;
	chirpType=EXP;
	newChirpType=EXP;
	//NOTE: (TO DO) put in random start time to avoid synchronization
	curriat_ = interchirpgap_;
	timer_.sched(curriat_);
}

/* reset to new chirp parameters */
void pathChirpAgentSnd::resetting()
{
  lowrate_=newlowrate_;
  highrate_=newhighrate_;
  chirpType=newChirpType;
  increment=newIncrement;
  /* make sure average rate is not too high  */
  if (avgrate_>lowrate_/3.0)
    avgrate_=lowrate_/3.0;
  else
    if (avgrate_<lowrate_/10.0)
      {
	avgrate_=lowrate_/10.0;
      }
  /*make sure avgrate_ is less than 10Mbps*/
  if(avgrate_>10000000.0)
    avgrate_=10000000.0;
  
  /* making sure the average probing rate is at least 5kbps */
  if (avgrate_<5000.0) avgrate_=5000.0;
  
  if (chirpType==EXP) interchirpgap_=computeinterchirpgap1();
  else if (chirpType==LIN) interchirpgap_=computeinterchirpgap2();
  if (interchirpgap_<0)
    interchirpgap_=0;
  
  resetflag=0;
  
  printf("调整发包策略：low=%fM high=%fM increment=%fM type=%d\n", lowrate_/1000000.0, highrate_/1000000.0, increment/1000000.0, chirpType);
  
  return;
}


/* stop the pathChirp agent sending packets */
void pathChirpAgentSnd::stop()
{
        running_ = 0;
	return;
}

/* sends a packet and timesout for the appropriate time*/
void pathChirpAgentSnd::timeout()
{
	if (chirpType == EXP) timeout1();
	else if (chirpType == LIN) timeout2();
}

void pathChirpAgentSnd::timeout1()
{
  if (running_==1) 
    {
      //if new params and pktnum
      if (resetflag==1 && pktnum_==0)
	{
	  resetting();
	  curriat_ = interchirpgap_;

	}

      sendpkt();
      /* reschedule the timer */
      curriat_ = next1(curriat_);
      
      timer_.resched(curriat_);
    }
}

//////////////////////////////////////////////////////////////
void pathChirpAgentSnd::timeout2()
{
  if (running_==1) 
    {
      //if new params and pktnum
      if (resetflag==1 && pktnum_==0)
	{
	  resetting();
	  curriat_ = interchirpgap_;

	}

      sendpkt();
      /* reschedule the timer */
      curriat_ = next2(curriat_);
      
      timer_.resched(curriat_);
    }
}

/* returns the next inter packet spacing, set pkt number of next packet */
double pathChirpAgentSnd::next1(double iat)
{  

     pktnum_++;
     if (pktnum_>num_pkts_per_chirp-1)
       {
	 pktnum_=0;
	 iat=interchirpgap_;
       }
     else
       {
	 if (pktnum_==1)
	   iat=8*(double)packetSize_/lowrate_;
	 else
	   iat=iat/spreadfactor_; 
       }

     return iat;
}
/////////////////////////////////////////////////////////////////////////////////////
double pathChirpAgentSnd::next2(double iat)
{  

     pktnum_++;
     if (pktnum_>num_pkts_per_chirp-1)
       {
	 pktnum_=0;
	 iat=interchirpgap_;
//	 printf("                             序列开始：lowrate=%fM , highrate=%fM chirpType=%d\n", lowrate_/increment, highrate_/increment, chirpType);
       }
     else
       {
	 if (pktnum_==1)
	   iat=8*(double)packetSize_/lowrate_;
	 else
	   iat= 8*(double)packetSize_ / (8*(double)packetSize_/iat + increment );
       }

     return iat;
}




/*time between chirps to maintain the average specified probing rate */
double pathChirpAgentSnd::computeinterchirpgap1()
{
	int num_pkts=1;
	double rate;
	double chirptime=0.0;
	double interchirptime=0.0;

	rate=lowrate_;
	
	while(rate<highrate_)
	  {
	    num_pkts++;
	    chirptime+=1/rate;
	    rate=rate*spreadfactor_;
	  }
	
	highrate_=rate;/////////////////////////////这里应该更新的
	chirptime=chirptime*(double)(8*packetSize_);

	interchirptime=(double)(num_pkts)*8.0*(double)(packetSize_)/avgrate_ - chirptime;
	if (interchirptime<0.0) interchirptime=0.0;
	
	num_pkts_per_chirp=num_pkts;
	return interchirptime;
}
//////////////////////////////////////////////////////////////////////
double pathChirpAgentSnd::computeinterchirpgap2()
{
	int num_pkts=1;
	double rate;
	double chirptime=0.0;
	double interchirptime=0.0;
	
	rate=lowrate_;
	
	while(rate<=highrate_)
	  {
	    num_pkts++;
	    chirptime+=1/rate;
	    rate=rate+increment;
	  }
	highrate_=rate;/////////////////////////////这里应该更新的
	
	chirptime=chirptime*(double)(8*packetSize_);

	interchirptime=(double)(num_pkts)*8.0*(double)(packetSize_)/avgrate_ - chirptime;
	if (interchirptime<0.0) interchirptime=0.0;
	
	num_pkts_per_chirp=num_pkts;
	return interchirptime;
}

/* used by tcl script */
int pathChirpAgentSnd::command(int argc, const char*const* argv)
{
	if (argc == 2) {
	  if (strcmp(argv[1], "start") == 0) {
	    start();
	    return (TCL_OK);
	  }
	  else if(strcmp(argv[1], "stop") == 0)
	    {
	      stop();
	      return (TCL_OK);
	    }
	}
	return (Agent::command(argc, argv));
}


/* on receiving packet run the pathchirp algorithm */
void pathChirpAgentSnd::recv(Packet* pkt, Handler*)
{
 
  // Access the IP header for the received packet:
  hdr_ip* hdrip = hdr_ip::access(pkt);
  
  // Access the pathChirp header for the received packet:
  hdr_pathChirpRcv* hdr = hdr_pathChirpRcv::access(pkt);
  
  // check if in brdcast mode
  if ((u_int32_t)hdrip->daddr() == IP_BROADCAST) {
      Packet::free(pkt);
      return;
  }

  if (chirpType == EXP) adjust1(hdr);
  else if (chirpType == LIN) adjust2(hdr);

  /* if estimate too low or high change probing rates */
  if (lowcount>2)
    {
      if (lowrate_<=1000000.0) /*less than 1Mbps, have smaller range of rates, that is fewer packets per chirp. This allows us to send more chirps per unit time.*/
	{
	  newhighrate_=lowrate_* m_;
	  newlowrate_=lowrate_/m_;
	}
      else{
	  newhighrate_=lowrate_*m_; //7.0－>4.0
	  newlowrate_=lowrate_/m_; //7.0－>4.0
	}
	  newChirpType=EXP;
      resetflag=1;
      lowcount=0;
    }
    
    
  if (midcount>2)
    {

      newhighrate_=hdr->inst_bw;
      newlowrate_=hdr->inst_bw/1.2;
	  newIncrement=chooseIncrement(hdr->inst_bw); 
	  newChirpType=LIN;
	  resetflag=1;
	  midcount=0; 
    }  

  if (highcount>2)
    {
      if (highrate_>1000000.0) 
	{
	  newlowrate_=highrate_/m_; //7.0－>4.0
	  newhighrate_=highrate_*m_; //7.0－>4.0
	}
      else{
	  newlowrate_=highrate_/m_;
	  newhighrate_=highrate_*m_;
	}
	  newChirpType=EXP;
      resetflag=1;
      highcount=0;
    }


  if (losscount>2)
    {
      newlowrate_=lowrate_/10.0;
      newhighrate_=highrate_/10.0;
      newChirpType=EXP;
      resetflag=1;
      losscount=0;
    }

  lastInstBw = hdr->inst_bw;
  Packet::free(pkt);
  return;

}

/*
double pathChirpAgentSnd::chooseIncrement(double bandwidth)
{
    if (bandwidth < 5000000.0) return 10000; //0M - 5M精度0.05M
	else if (bandwidth < 10000000.0) return 10000; //5M - 10M精度0.1M
	else if (bandwidth < 50000000.0) return 100000.0; //10M - 50M以下精度0.5M
	else if (bandwidth < 150000000.0) return 1000000.0; //50M - 150M，精度1M
	else if (bandwidth < 500000000.0) return 10000000.0; //150M - 500M,精度10M
	else return 50000000.0; //大于500M，精度50M
}
*/

double pathChirpAgentSnd::chooseIncrement(double bandwidth)
{	
	double range = bandwidth - bandwidth/1.2;
	return range / 15.0;
	
}

void pathChirpAgentSnd::adjust1(hdr_pathChirpRcv* hdr)
{//比较认可的为1.33和0.75即4/3和3/4,现在为测参数关系先用怪数。
	  //ALGO TO CHANGE RATES  
  if (hdr->loss==1)
    {losscount++;//printf("发生丢包。。\n");
    }
  else 
    losscount=0;

  if (hdr->loss!=1 && hdr->inst_bw<=a_*lowrate_)
    {lowcount++;}
  else 
    lowcount=0;

  if (hdr->loss!=1 && hdr->inst_bw>=highrate_/a_)
    {highcount++;}
  else 
    highcount=0;

  if (hdr->loss!=1 && hdr->inst_bw>a_*lowrate_ && hdr->inst_bw<highrate_/a_)
    {midcount++;}
  else
    midcount=0;

}
//////////////////////////////////////////////////////////////////

void pathChirpAgentSnd::adjust2(hdr_pathChirpRcv* hdr)
{
	  //ALGO TO CHANGE RATES
  if (hdr->loss==1)
    {losscount++; //printf("发生丢包。。\n");
    }
  else 
    losscount=0;

  if (hdr->loss!=1 && hdr->inst_bw <= lowrate_ + n_ * increment)
    {lowcount++;}
  else 
    lowcount=0;

  if (hdr->loss!=1 && hdr->inst_bw >= highrate_ - n_ * increment)
    {highcount++;}
  else 
    highcount=0;  
    
  if (hdr->loss!=1 && hdr->inst_bw > (lowrate_ + n_ * increment) && hdr->inst_bw < (highrate_ - n_ * increment))
    {
    	midcount=0; 
    	if (complete_count<2)
    	{
    		inst_array[complete_count++]=hdr->inst_bw;
    	}
    	else
    	{
    		float sum_inst = 0.0;
    		int i;
    		for (i=0; i<3; i++)
    			sum_inst += inst_array[i];
    		float avg_inst = sum_inst / 3;
//    		fprintf(stdout,"%f %fM \n",Scheduler::instance().clock(),avg_inst/1000000.0);
    		complete_count = 0;
    	}	
    }
  else
    {midcount=0; complete_count=0;}
}


/*
void pathChirpAgentSnd::adjust2(hdr_pathChirpRcv* hdr)
{

	  //ALGO TO CHANGE RATES
  if (hdr->loss==1)
    {losscount++; //printf("发生丢包。。\n");
    }
  else 
    losscount=0;

  if (hdr->loss!=1 && hdr->inst_bw < a2_*lowrate_)
    {lowcount++;}
  else 
    lowcount=0;

  if (hdr->loss!=1 && hdr->inst_bw > highrate_/a2_)
    {highcount++;}
  else 
    highcount=0;  
    
  if (hdr->loss!=1 && hdr->inst_bw >= (a2_*lowrate_) && hdr->inst_bw <= (highrate_/a2_))
    {
    	midcount=0;
    }
  else
    midcount=0;  
    
}
*/

